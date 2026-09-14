#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/dsp/fft.hpp"
#include "audioplugins/common/analysis/ResonancePeakPicker.h"

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

    // Step 3: convert to dB
    std::vector<float> magDb(static_cast<std::size_t>(halfN));
    for (int k = 0; k < halfN; ++k)
        magDb[static_cast<std::size_t>(k)] = 20.f * std::log10(mag[static_cast<std::size_t>(k)] + 1e-9f);

    // Step 4-7: peak-pick via Common's O(1) prefix-sum background estimate
    // (supersedes this file's former ±1-octave moving-average approach).
    const auto peaks = audioplugins::common::analysis::pickResonancePeaks(
        magDb.data(), halfN, static_cast<float>(audio.sampleRate), kFftN,
        kMinFreqHz, kMaxFreqHz, kMinQ, kProminenceDb, kMaxResults);

    // Step 8: map Common's {freqHz,q,prominenceDb} onto mt::ResonancePeak's
    // {freqHz,q,gainDb,enabled} -- gainDb reproduces this file's original
    // formula (negative = attenuation), enabled defaults to true, matching
    // today's behavior exactly.
    std::vector<ResonancePeak> results;
    results.reserve(peaks.size());
    for (const auto& p : peaks) {
        ResonancePeak peak;
        peak.freqHz  = p.freqHz;
        peak.q       = p.q;
        peak.gainDb  = -std::clamp(p.prominenceDb, kMinGainDb, kMaxGainDb);
        peak.enabled = true;
        results.push_back(peak);
    }

    return results;
}

} // namespace mt
