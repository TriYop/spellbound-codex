#include "mastertweak/dsp/lufs_analyser.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace mt::dsp {

// ── K-weighting coefficient factories ────────────────────────────────────────
//
// BS.1770 Stage 1: 2nd-order high-shelf, f0=1681.97 Hz, +4 dB gain, Q=0.7072.
// Bilinear transform with K = tan(pi * f0 / sr).
BiquadCoeffs LufsAnalyser::kWeightingStage1(double sr) {
    const double f0 = 1681.974450955533;
    const double G  = 3.999843853;
    const double Q  = 0.7071752369554196;
    const double K  = std::tan(std::numbers::pi_v<double> * f0 / sr);
    const double K2 = K * K;
    const double Vh = std::pow(10.0, G / 20.0);  // linear amplitude gain
    const double Vb = std::pow(10.0, G / 40.0);  // = sqrt(Vh)
    const double n  = 1.0 / (1.0 + K / Q + K2);
    return BiquadCoeffs{
        float((Vh + Vb * K / Q + K2) * n),
        float(2.0  * (K2 - Vh)       * n),
        float((Vh - Vb * K / Q + K2) * n),
        float(2.0  * (K2 - 1.0)      * n),
        float((1.0 - K / Q + K2)     * n)
    };
}

// BS.1770 Stage 2: 2nd-order high-pass, f0=38.135 Hz, Q=0.5003 (Butterworth).
BiquadCoeffs LufsAnalyser::kWeightingStage2(double sr) {
    const double f0 = 38.13547087602444;
    const double Q  = 0.5003270373238773;
    const double K  = std::tan(std::numbers::pi_v<double> * f0 / sr);
    const double K2 = K * K;
    const double n  = 1.0 / (1.0 + K / Q + K2);
    return BiquadCoeffs{
        float(1.0            * n),
        float(-2.0           * n),
        float(1.0            * n),
        float(2.0 * (K2 - 1.0) * n),
        float((1.0 - K / Q + K2) * n)
    };
}

// ── Public API ────────────────────────────────────────────────────────────────

void LufsAnalyser::prepare(float sampleRate, int numChannels) {
    sampleRate_  = sampleRate;
    numChannels_ = numChannels;
    blockSize_   = static_cast<int>(0.4  * sampleRate);  // 400 ms
    hopSize_     = static_cast<int>(0.1  * sampleRate);  // 100 ms (75% overlap)
    stage1_ = kWeightingStage1(static_cast<double>(sampleRate));
    stage2_ = kWeightingStage2(static_cast<double>(sampleRate));
}

float LufsAnalyser::measure(const std::vector<std::vector<float>>& samples,
                             int numFrames) {
    const auto nch = static_cast<size_t>(numChannels_);

    // ── Step 1: K-weight entire buffer per channel (single pass, O(n)) ───────
    // Pre-filtering the full buffer avoids restarting filter state per block
    // (which would be O(n²) in the number of blocks).
    std::vector<std::vector<float>> kw(nch, std::vector<float>(static_cast<size_t>(numFrames)));
    for (size_t ch = 0; ch < nch; ++ch) {
        BiquadState s1{}, s2{};
        for (int f = 0; f < numFrames; ++f) {
            const float x  = samples[ch][static_cast<size_t>(f)];
            const float y1 = biquadProcess(stage1_, s1, x);
            kw[ch][static_cast<size_t>(f)] = biquadProcess(stage2_, s2, y1);
        }
    }

    // ── Step 2: Compute mean-square power per 400ms block (75% overlap) ──────
    std::vector<double> blockPowers;
    for (int offset = 0; offset + blockSize_ <= numFrames; offset += hopSize_) {
        double z = 0.0;
        for (size_t ch = 0; ch < nch; ++ch) {
            double sumSq = 0.0;
            for (int f = offset; f < offset + blockSize_; ++f)
                sumSq += static_cast<double>(kw[ch][static_cast<size_t>(f)])
                       * kw[ch][static_cast<size_t>(f)];
            z += sumSq / blockSize_;  // mean square, this channel
        }
        blockPowers.push_back(z);
    }

    if (blockPowers.empty()) return -70.f;

    // ── Step 3: Absolute gate — discard blocks below -70 LUFS ────────────────
    // -70 LUFS ↔ -0.691 + 10*log10(z) = -70 → z = 10^((-70+0.691)/10)
    constexpr double kAbsGateZ = 1.1724e-7;  // 10^(-6.9309): -0.691 + 10*log10(z) = -70
    std::vector<double> gated1;
    for (double z : blockPowers)
        if (z >= kAbsGateZ) gated1.push_back(z);

    if (gated1.empty()) return -70.f;

    // ── Step 4: Relative gate — discard blocks > 10 LU below ungated mean ────
    double Jg = 0.0;
    for (double z : gated1) Jg += z;
    Jg /= static_cast<double>(gated1.size());

    const double relGateZ = Jg * 0.1;  // 10 LU = factor 10 in power
    std::vector<double> gated2;
    for (double z : gated1)
        if (z >= relGateZ) gated2.push_back(z);

    if (gated2.empty()) return -70.f;

    // ── Step 5: Gated mean and LUFS ──────────────────────────────────────────
    double mean = 0.0;
    for (double z : gated2) mean += z;
    mean /= static_cast<double>(gated2.size());

    return static_cast<float>(-0.691 + 10.0 * std::log10(mean));
}

LoudnessMetrics LufsAnalyser::measureWithLra(
        const std::vector<std::vector<float>>& samples, int numFrames) {
    const auto nch = static_cast<size_t>(numChannels_);
    const int lraBlockSize = static_cast<int>(3.0f * sampleRate_);  // 3 s in samples

    // ── K-weight entire buffer (shared pass) ──────────────────────────────────
    std::vector<std::vector<float>> kw(nch,
        std::vector<float>(static_cast<size_t>(numFrames)));
    for (size_t ch = 0; ch < nch; ++ch) {
        BiquadState s1{}, s2{};
        for (int f = 0; f < numFrames; ++f) {
            const float x  = samples[ch][static_cast<size_t>(f)];
            const float y1 = biquadProcess(stage1_, s1, x);
            kw[ch][static_cast<size_t>(f)] = biquadProcess(stage2_, s2, y1);
        }
    }

    // ── Accumulate block powers (400 ms and 3 s), same 100 ms hop ────────────
    std::vector<double> intBlocks;
    std::vector<double> lraBlocks;

    for (int offset = 0; offset + blockSize_ <= numFrames; offset += hopSize_) {
        // 400 ms block
        {
            double z = 0.0;
            for (size_t ch = 0; ch < nch; ++ch) {
                double sumSq = 0.0;
                for (int f = offset; f < offset + blockSize_; ++f)
                    sumSq += static_cast<double>(kw[ch][static_cast<size_t>(f)])
                           * kw[ch][static_cast<size_t>(f)];
                z += sumSq / blockSize_;
            }
            intBlocks.push_back(z);
        }
        // 3 s block (only when it fits)
        if (offset + lraBlockSize <= numFrames) {
            double z = 0.0;
            for (size_t ch = 0; ch < nch; ++ch) {
                double sumSq = 0.0;
                for (int f = offset; f < offset + lraBlockSize; ++f)
                    sumSq += static_cast<double>(kw[ch][static_cast<size_t>(f)])
                           * kw[ch][static_cast<size_t>(f)];
                z += sumSq / lraBlockSize;
            }
            lraBlocks.push_back(z);
        }
    }

    LoudnessMetrics result;
    constexpr double kAbsGateZ = 1.1724e-7;  // -70 LUFS threshold in power domain

    // ── Integrated LUFS (400 ms blocks, -10 LU relative gate) ────────────────
    {
        std::vector<double> g1;
        for (double z : intBlocks)
            if (z >= kAbsGateZ) g1.push_back(z);
        if (!g1.empty()) {
            double Jg = 0.0;
            for (double z : g1) Jg += z;
            Jg /= static_cast<double>(g1.size());
            const double relGateZ = Jg * 0.1;  // -10 LU = power × 0.1
            std::vector<double> g2;
            for (double z : g1)
                if (z >= relGateZ) g2.push_back(z);
            if (!g2.empty()) {
                double mean = 0.0;
                for (double z : g2) mean += z;
                mean /= static_cast<double>(g2.size());
                result.integratedLufs =
                    static_cast<float>(-0.691 + 10.0 * std::log10(mean));
            }
        }
    }

    // ── LRA (3 s blocks, -20 LU relative gate, P95 - P10) ────────────────────
    {
        std::vector<double> g1;
        for (double z : lraBlocks)
            if (z >= kAbsGateZ) g1.push_back(z);
        if (g1.size() >= 2) {
            double Jg = 0.0;
            for (double z : g1) Jg += z;
            Jg /= static_cast<double>(g1.size());
            const double relGateZ = Jg * 0.01;  // -20 LU = power × 0.01
            std::vector<float> lufsVals;
            for (double z : g1) {
                if (z >= relGateZ)
                    lufsVals.push_back(
                        static_cast<float>(-0.691 + 10.0 * std::log10(z)));
            }
            if (lufsVals.size() >= 2) {
                std::sort(lufsVals.begin(), lufsVals.end());
                const auto n = lufsVals.size();
                auto idx = [n](float p) -> size_t {
                    return static_cast<size_t>(
                        std::clamp(static_cast<int>(
                            std::floor(p * static_cast<float>(n))),
                            0, static_cast<int>(n) - 1));
                };
                result.lra = std::max(0.f, lufsVals[idx(0.95f)] - lufsVals[idx(0.10f)]);
            }
        }
    }

    return result;
}

} // namespace mt::dsp
