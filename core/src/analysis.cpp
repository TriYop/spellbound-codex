#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/dsp/biquad.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace mt {

namespace {

constexpr int kNumBands      = AnalysisSnapshot::kNumBands;
constexpr int kNumCrossovers = kNumBands - 1;
constexpr int kBlockSize     = 1024;   // offline block size; matches typical plugin block
constexpr float kQ707        = 0.7071067811865476f;  // 1/√2 — Butterworth Q

// 4th-order Linkwitz-Riley crossover: two cascaded 2nd-order Butterworth stages.
struct LR4 {
    dsp::BiquadCoeffs c;
    dsp::BiquadState  st[2][2] {};  // [stage][channel]

    void setLowpass(float fc, float sr) noexcept {
        c = dsp::BiquadCoeffs::lowpass(fc, kQ707, sr);
        for (auto& stageArr : st) for (auto& s : stageArr) s.reset();
    }
    void setHighpass(float fc, float sr) noexcept {
        c = dsp::BiquadCoeffs::highpass(fc, kQ707, sr);
        for (auto& stageArr : st) for (auto& s : stageArr) s.reset();
    }

    float process(int ch, float x) noexcept {
        return dsp::biquadProcess(c, st[1][static_cast<size_t>(ch)],
               dsp::biquadProcess(c, st[0][static_cast<size_t>(ch)], x));
    }
};

static float blockRmsLinear(const float* data, int n) noexcept {
    if (n <= 0) return 0.f;
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += static_cast<double>(data[i]) * data[i];
    return static_cast<float>(std::sqrt(sum / n));
}

static float blockPeak(const float* data, int n) noexcept {
    float peak = 0.f;
    for (int i = 0; i < n; ++i) {
        const float a = std::abs(data[i]);
        if (a > peak) peak = a;
    }
    return peak;
}

static float blockCorrelation(const float* L, const float* R, int n) noexcept {
    if (n <= 0) return 1.f;
    double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
    for (int i = 0; i < n; ++i) {
        sumLR += static_cast<double>(L[i]) * R[i];
        sumL2 += static_cast<double>(L[i]) * L[i];
        sumR2 += static_cast<double>(R[i]) * R[i];
    }
    const double denom = std::sqrt(sumL2 * sumR2);
    if (denom < 1e-12) return 1.f;
    return static_cast<float>(std::clamp(sumLR / denom, -1.0, 1.0));
}

} // anonymous namespace

AnalysisSnapshot analyseFile(const AudioFile& audio) {
    AnalysisSnapshot snap{};
    if (audio.numFrames == 0) return snap;

    const float sr      = static_cast<float>(audio.sampleRate);
    const bool  isMono  = audio.numChannels == 1;

    // IIR smoothing alphas — same formula as MixAdvice::prepare()
    const float blocksPerSec = sr / static_cast<float>(kBlockSize);
    const float rmsAlpha   = std::exp(-1.f / (0.10f * blocksPerSec));  // 100 ms
    const float corrAlpha  = std::exp(-1.f / (0.30f * blocksPerSec));  // 300 ms
    const float crestAlpha = std::exp(-1.f / (0.50f * blocksPerSec));  // 500 ms

    // Set up the LR4 cascaded crossover bank
    std::array<LR4, kNumCrossovers> lpFilters, hpFilters;
    for (int i = 0; i < kNumCrossovers; ++i) {
        const float fc = AnalysisSnapshot::kCrossoverHz[static_cast<size_t>(i)];
        lpFilters[static_cast<size_t>(i)].setLowpass(fc, sr);
        hpFilters[static_cast<size_t>(i)].setHighpass(fc, sr);
    }

    // Smoothed state (linear amplitude), peak-holds (linear amplitude)
    std::array<float, kNumBands> smoothRmsL{}, smoothRmsR{};
    std::array<float, kNumBands> smoothCrestL{}, smoothCrestR{};
    std::array<float, kNumBands> smoothCorrBand{};
    std::array<float, kNumBands> peakRmsLinL{}, peakRmsLinR{};
    smoothCorrBand.fill(1.f);
    smoothCrestL.fill(1.f);
    smoothCrestR.fill(1.f);

    float smoothOverallL = 0.f, smoothOverallR = 0.f;
    float smoothOverallCorr = 1.f;
    float peakOverallLinL = 0.f, peakOverallLinR = 0.f;

    // Long-term integrators (for long-term average RMS and Pearson correlation)
    std::array<double, kNumBands> intBandSumL2{}, intBandSumR2{};
    double intOverallSumL2 = 0.0, intOverallSumR2 = 0.0;
    double intSumLR = 0.0, intSumL2 = 0.0, intSumR2 = 0.0;
    uint64_t blockCount = 0;

    // Working buffers for one band (per-channel)
    std::vector<float> remainder0(static_cast<size_t>(kBlockSize));
    std::vector<float> remainder1(static_cast<size_t>(kBlockSize));
    std::vector<float> band0(static_cast<size_t>(kBlockSize));
    std::vector<float> band1(static_cast<size_t>(kBlockSize));

    auto toDb = [](float lin) -> float {
        return lin > 1e-7f ? 20.f * std::log10(lin) : -100.f;
    };

    std::array<std::vector<float>, kNumBands> bandRmsSamples;
    const size_t estBlocks = static_cast<size_t>((audio.numFrames + kBlockSize - 1) / kBlockSize);
    for (auto& v : bandRmsSamples) v.reserve(estBlocks);

    const int numFrames = audio.numFrames;
    const float* chL = audio.samples[0].data();
    const float* chR = isMono ? chL : audio.samples[1].data();

    for (int blockStart = 0; blockStart < numFrames; blockStart += kBlockSize) {
        const int n = std::min(kBlockSize, numFrames - blockStart);
        const float* srcL = chL + blockStart;
        const float* srcR = chR + blockStart;

        // Overall broadband level and mono compatibility (pre-filterbank)
        {
            const float rawL   = blockRmsLinear(srcL, n);
            const float rawR   = blockRmsLinear(srcR, n);
            const float rawCorr = blockCorrelation(srcL, srcR, n);

            smoothOverallL    = rmsAlpha  * smoothOverallL    + (1.f - rmsAlpha)  * rawL;
            smoothOverallR    = rmsAlpha  * smoothOverallR    + (1.f - rmsAlpha)  * rawR;
            smoothOverallCorr = corrAlpha * smoothOverallCorr + (1.f - corrAlpha) * rawCorr;

            if (smoothOverallL > peakOverallLinL) peakOverallLinL = smoothOverallL;
            if (smoothOverallR > peakOverallLinR) peakOverallLinR = smoothOverallR;

            // Integrated Pearson (whole-track)
            for (int i = 0; i < n; ++i) {
                const double l = srcL[i], r = srcR[i];
                intSumLR += l * r;
                intSumL2 += l * l;
                intSumR2 += r * r;
            }
            intOverallSumL2 += static_cast<double>(rawL) * rawL;
            intOverallSumR2 += static_cast<double>(rawR) * rawR;
        }

        // Copy block into remainder buffers for the crossover cascade
        std::copy_n(srcL, static_cast<size_t>(n), remainder0.data());
        std::copy_n(srcR, static_cast<size_t>(n), remainder1.data());

        auto storeBand = [&](size_t bandIdx, const float* bL, const float* bR) {
            const float rmsL   = blockRmsLinear(bL, n);
            const float rmsR   = blockRmsLinear(bR, n);
            const float peakL  = blockPeak(bL, n);
            const float peakR  = blockPeak(bR, n);
            const float corr   = blockCorrelation(bL, bR, n);
            const float crestL = rmsL > 1e-7f ? peakL / rmsL : 1.f;
            const float crestR = rmsR > 1e-7f ? peakR / rmsR : 1.f;

            smoothRmsL[bandIdx]    = rmsAlpha   * smoothRmsL[bandIdx]    + (1.f - rmsAlpha)   * rmsL;
            smoothRmsR[bandIdx]    = rmsAlpha   * smoothRmsR[bandIdx]    + (1.f - rmsAlpha)   * rmsR;
            smoothCorrBand[bandIdx]= corrAlpha  * smoothCorrBand[bandIdx]+ (1.f - corrAlpha)  * corr;
            smoothCrestL[bandIdx]  = crestAlpha * smoothCrestL[bandIdx]  + (1.f - crestAlpha) * crestL;
            smoothCrestR[bandIdx]  = crestAlpha * smoothCrestR[bandIdx]  + (1.f - crestAlpha) * crestR;

            if (smoothRmsL[bandIdx] > peakRmsLinL[bandIdx]) peakRmsLinL[bandIdx] = smoothRmsL[bandIdx];
            if (smoothRmsR[bandIdx] > peakRmsLinR[bandIdx]) peakRmsLinR[bandIdx] = smoothRmsR[bandIdx];

            intBandSumL2[bandIdx] += static_cast<double>(rmsL) * rmsL;
            intBandSumR2[bandIdx] += static_cast<double>(rmsR) * rmsR;
            bandRmsSamples[bandIdx].push_back((toDb(rmsL) + toDb(rmsR)) * 0.5f);
        };

        // Cascaded LR4 filterbank
        for (size_t ci = 0; ci < static_cast<size_t>(kNumCrossovers); ++ci) {
            for (int i = 0; i < n; ++i) {
                band0[static_cast<size_t>(i)] = lpFilters[ci].process(0, remainder0[static_cast<size_t>(i)]);
                band1[static_cast<size_t>(i)] = lpFilters[ci].process(1, remainder1[static_cast<size_t>(i)]);
                remainder0[static_cast<size_t>(i)] = hpFilters[ci].process(0, remainder0[static_cast<size_t>(i)]);
                remainder1[static_cast<size_t>(i)] = hpFilters[ci].process(1, remainder1[static_cast<size_t>(i)]);
            }
            storeBand(ci, band0.data(), band1.data());
        }
        storeBand(static_cast<size_t>(kNumCrossovers), remainder0.data(), remainder1.data());

        ++blockCount;
    }

    // Convert accumulators → final snapshot values
    auto toCrestDb = [](float r) -> float { return r > 1.f ? 20.f * std::log10(r) : 0.f; };

    for (size_t i = 0; i < static_cast<size_t>(kNumBands); ++i) {
        const double n = static_cast<double>(blockCount);
        const float avgL = n > 0 ? static_cast<float>(std::sqrt(intBandSumL2[i] / n)) : 0.f;
        const float avgR = n > 0 ? static_cast<float>(std::sqrt(intBandSumR2[i] / n)) : 0.f;
        const float avgRms = (toDb(avgL) + toDb(avgR)) * 0.5f;
        const float peakRms = (toDb(peakRmsLinL[i]) + toDb(peakRmsLinR[i])) * 0.5f;
        const float avgCrest = (toCrestDb(smoothCrestL[i]) + toCrestDb(smoothCrestR[i])) * 0.5f;

        snap.bands[i].avgRmsDb    = avgRms;
        snap.bands[i].peakRmsDb   = peakRms;
        snap.bands[i].correlation = smoothCorrBand[i];
        snap.bands[i].crestDb     = avgCrest;

        // Percentile descriptors — sort collected block-RMS samples and read indices
        {
            auto& v = bandRmsSamples[i];
            std::sort(v.begin(), v.end());
            auto pct = [&](float p) -> float {
                if (v.empty()) return -100.f;
                const auto idx = static_cast<size_t>(
                    std::clamp(static_cast<int>(std::floor(p * static_cast<float>(v.size()))),
                               0, static_cast<int>(v.size()) - 1));
                return v[idx];
            };
            snap.bands[i].p10RmsDb = pct(0.10f);
            snap.bands[i].p50RmsDb = pct(0.50f);
            snap.bands[i].p95RmsDb = pct(0.95f);
        }
    }

    {
        const double n = static_cast<double>(blockCount);
        const float avgL = n > 0 ? static_cast<float>(std::sqrt(intOverallSumL2 / n)) : 0.f;
        const float avgR = n > 0 ? static_cast<float>(std::sqrt(intOverallSumR2 / n)) : 0.f;
        snap.overallAvgDb  = (toDb(avgL) + toDb(avgR)) * 0.5f;
        snap.overallPeakDb = (toDb(peakOverallLinL) + toDb(peakOverallLinR)) * 0.5f;

        const double intDenom = std::sqrt(intSumL2 * intSumR2);
        snap.overallCorr = intDenom > 1e-12
            ? static_cast<float>(std::clamp(intSumLR / intDenom, -1.0, 1.0)) : 1.f;
    }

    return snap;
}

} // namespace mt
