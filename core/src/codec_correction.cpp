#include "mastertweak/codec_correction.hpp"
#include "mastertweak/dsp/fft.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mt {

// ─── local helpers ────────────────────────────────────────────────────────────

static constexpr size_t kFftN = 4096;

// Mean dB across FFT bins spanning [loHz, hiHz].
static float bandDb(const std::vector<float>& mag, float sr, size_t N,
                    float loHz, float hiHz) {
    const float binWidth = sr / static_cast<float>(N);
    const size_t lo = static_cast<size_t>(std::max(1.f, loHz / binWidth));
    const size_t hi = std::min(N / 2 - 1, static_cast<size_t>(hiHz / binWidth));
    if (lo > hi) return -100.f;
    float sumSq = 0.f;
    for (size_t k = lo; k <= hi; ++k) sumSq += mag[k] * mag[k];
    const float rms = std::sqrt(sumSq / static_cast<float>(hi - lo + 1));
    return rms > 1e-7f ? 20.f * std::log10(rms) : -100.f;
}

// ─── public API ───────────────────────────────────────────────────────────────

std::array<float, 7> computeCodecCorrection(const AudioFile& audio) {
    std::array<float, 7> corr{};

    if (audio.numFrames < static_cast<int>(kFftN)) return corr;  // too short

    const float sr      = static_cast<float>(audio.sampleRate);
    const float nyquist = sr / 2.f;
    if (nyquist < 16000.f) return corr;  // insufficient HF range for correction

    const int nch = audio.numChannels;

    // Build mono mix
    std::vector<float> mono(static_cast<size_t>(audio.numFrames));
    for (size_t i = 0; i < static_cast<size_t>(audio.numFrames); ++i) {
        float sum = 0.f;
        for (int c = 0; c < nch; ++c)
            sum += audio.samples[static_cast<size_t>(c)][i];
        mono[i] = sum / static_cast<float>(nch);
    }

    const auto avgMag = dsp::averagedMagnitudeSpectrum(
        mono.data(), audio.numFrames, static_cast<int>(kFftN), sr);

    if (avgMag.empty()) return corr;

    // Baseline: mean dB in 8–12 kHz — typically unaffected by codec rolloff
    const float baselineDb = bandDb(avgMag, sr, kFftN, 8000.f, 12000.f);
    if (baselineDb < -50.f) return corr;  // no significant HF content → skip

    // Band 6 — Air (>16 kHz): full rolloff correction
    const float airDb   = bandDb(avgMag, sr, kFftN, 16000.f, std::min(20000.f, nyquist));
    const float airCorr = std::clamp(baselineDb - airDb, 0.f, 6.f);
    corr[6] = airCorr;

    // Band 5 — Highs upper half (12–16 kHz): half-weight correction
    const float hi2Db = bandDb(avgMag, sr, kFftN, 12000.f, 16000.f);
    corr[5] = std::clamp((baselineDb - hi2Db) * 0.5f, 0.f, 1.5f);

    // Band 4 — HiMids (2–6 kHz): psychoacoustic smear, proportional to Air correction
    corr[4] = std::clamp(airCorr * 0.08f, 0.f, 0.5f);

    return corr;
}

} // namespace mt
