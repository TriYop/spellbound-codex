#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/dsp/fft.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace mt {

std::vector<ResonancePeak> detectResonances(const AudioFile& audio)
{
    constexpr int   kFftN         = 4096;
    constexpr float kMinFreqHz    = 80.f;
    constexpr float kMaxFreqHz    = 16000.f;
    constexpr float kMinQ         = 3.f;
    constexpr int   kMaxResults   = 8;
    constexpr float kProminenceDb = 6.f;
    constexpr float kMaxGainDb    = 12.f;
    constexpr float kMinGainDb    = 3.f;

    if (audio.numFrames == 0 || audio.numChannels == 0)
        return {};

    // Step 1: build mono mix
    std::vector<float> mono(static_cast<std::size_t>(audio.numFrames), 0.f);
    for (int ch = 0; ch < audio.numChannels; ++ch) {
        const auto& ch_samples = audio.samples[static_cast<std::size_t>(ch)];
        for (int i = 0; i < audio.numFrames; ++i)
            mono[static_cast<std::size_t>(i)] += ch_samples[static_cast<std::size_t>(i)];
    }
    const float invCh = 1.f / static_cast<float>(audio.numChannels);
    for (auto& s : mono)
        s *= invCh;

    // Step 2: compute averaged magnitude spectrum (bins 0..kFftN/2-1)
    std::vector<float> mag = mt::dsp::averagedMagnitudeSpectrum(
        mono.data(), audio.numFrames, kFftN);

    const int halfN = kFftN / 2;
    if (static_cast<int>(mag.size()) < halfN)
        return {};

    // Helper lambdas
    const float binWidth = static_cast<float>(audio.sampleRate) / static_cast<float>(kFftN);
    auto binToHz = [&](int k) { return static_cast<float>(k) * binWidth; };
    auto hzToBin = [&](float hz) { return static_cast<int>(hz / binWidth); };

    // Step 3: convert to dB
    std::vector<float> magDb(static_cast<std::size_t>(halfN));
    for (int k = 0; k < halfN; ++k)
        magDb[static_cast<std::size_t>(k)] = 20.f * std::log10(mag[static_cast<std::size_t>(k)] + 1e-9f);

    // Step 4: compute background level per bin via ±1-octave moving average
    std::vector<float> bgDb(static_cast<std::size_t>(halfN));
    for (int k = 0; k < halfN; ++k) {
        const float freqLo = binToHz(k) / std::sqrt(2.f);
        const float freqHi = binToHz(k) * std::sqrt(2.f);
        const int lo = std::max(0, hzToBin(freqLo));
        const int hi = std::min(halfN - 1, hzToBin(freqHi));

        float sum = 0.f;
        int   cnt = 0;
        for (int j = lo; j <= hi; ++j) {
            sum += magDb[static_cast<std::size_t>(j)];
            ++cnt;
        }
        bgDb[static_cast<std::size_t>(k)] = (cnt > 0) ? (sum / static_cast<float>(cnt)) : magDb[static_cast<std::size_t>(k)];
    }

    // Steps 5–6: find local maxima that are prominent enough and narrow enough
    struct Candidate {
        int   bin;
        float prominence;
        float q;
    };
    std::vector<Candidate> candidates;

    for (int k = 1; k < halfN - 1; ++k) {
        const float fk = binToHz(k);
        if (fk <= kMinFreqHz || fk >= kMaxFreqHz)
            continue;

        const float mk = magDb[static_cast<std::size_t>(k)];

        // Local maximum
        if (mk <= magDb[static_cast<std::size_t>(k - 1)] || mk <= magDb[static_cast<std::size_t>(k + 1)])
            continue;

        // Prominence check
        const float prominence = mk - bgDb[static_cast<std::size_t>(k)];
        if (prominence < kProminenceDb)
            continue;

        // Step 6a–6e: find 3 dB bandwidth
        const float threshold3dB = mk - 3.f;

        // Scan left for 3 dB point (last bin still at or above threshold)
        int left3dB = 0;  // fallback: edge of spectrum
        for (int j = k - 1; j >= 0; --j) {
            if (magDb[static_cast<std::size_t>(j)] <= threshold3dB) {
                left3dB = j + 1;  // last bin still above threshold
                break;
            }
            if (j == 0) left3dB = 0;  // never crossed: use bin 0
        }

        // Scan right for 3 dB point (last bin still at or above threshold)
        int right3dB = halfN - 1;  // fallback: edge of spectrum
        for (int j = k + 1; j < halfN; ++j) {
            if (magDb[static_cast<std::size_t>(j)] <= threshold3dB) {
                right3dB = j - 1;  // last bin still above threshold
                break;
            }
            if (j == halfN - 1) right3dB = halfN - 1;  // never crossed: use last bin
        }

        // Bandwidth in Hz (clamp to at least one bin width to avoid div/0)
        float bw3dB = binToHz(right3dB) - binToHz(left3dB);
        if (bw3dB < binWidth)
            bw3dB = binWidth;

        const float q = fk / bw3dB;

        // Step 6f: skip if not narrow enough
        if (q < kMinQ)
            continue;

        candidates.push_back({k, prominence, q});
    }

    // Step 7: sort by prominence descending, keep at most kMaxResults
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.prominence > b.prominence;
              });

    if (static_cast<int>(candidates.size()) > kMaxResults)
        candidates.resize(static_cast<std::size_t>(kMaxResults));

    // Step 8: build ResonancePeak results
    std::vector<ResonancePeak> results;
    results.reserve(candidates.size());
    for (const auto& c : candidates) {
        ResonancePeak peak;
        peak.freqHz  = binToHz(c.bin);
        peak.q       = c.q;
        peak.gainDb  = -std::clamp(c.prominence, kMinGainDb, kMaxGainDb);
        peak.enabled = true;
        results.push_back(peak);
    }

    return results;
}

} // namespace mt
