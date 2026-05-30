#include "mastertweak/codec_correction.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace mt {

// ─── FFT helpers ──────────────────────────────────────────────────────────────

static constexpr size_t kFftN = 4096;

// In-place iterative Cooley-Tukey DIT FFT (N must be a power of 2).
static void inplaceFft(std::vector<float>& re, std::vector<float>& im) {
    const size_t N = re.size();
    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < N; ++i) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    // Butterfly stages
    for (size_t len = 2; len <= N; len <<= 1) {
        const float ang     = -2.f * std::numbers::pi_v<float> / static_cast<float>(len);
        const float wBaseRe = std::cos(ang);
        const float wBaseIm = std::sin(ang);
        for (size_t i = 0; i < N; i += len) {
            float wRe = 1.f, wIm = 0.f;
            for (size_t j = 0; j < len / 2; ++j) {
                const float uRe =  re[i + j];
                const float uIm =  im[i + j];
                const float vRe =  re[i + j + len/2] * wRe - im[i + j + len/2] * wIm;
                const float vIm =  re[i + j + len/2] * wIm + im[i + j + len/2] * wRe;
                re[i + j]          = uRe + vRe;
                im[i + j]          = uIm + vIm;
                re[i + j + len/2]  = uRe - vRe;
                im[i + j + len/2]  = uIm - vIm;
                const float nwRe = wRe * wBaseRe - wIm * wBaseIm;
                wIm = wRe * wBaseIm + wIm * wBaseRe;
                wRe = nwRe;
            }
        }
    }
}

// Hann-window a signal, run FFT, return magnitude for bins [0, N/2).
static std::vector<float> magnitudeSpectrum(const float* signal, size_t N) {
    std::vector<float> re(N), im(N, 0.f);
    for (size_t i = 0; i < N; ++i) {
        const float w = 0.5f * (1.f - std::cos(2.f * std::numbers::pi_v<float>
                                                * static_cast<float>(i)
                                                / static_cast<float>(N - 1)));
        re[i] = signal[i] * w;
    }
    inplaceFft(re, im);
    std::vector<float> mag(N / 2);
    for (size_t k = 0; k < N / 2; ++k)
        mag[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
    return mag;
}

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

    const int    nch  = audio.numChannels;
    const size_t hop  = kFftN / 2;  // 50 % overlap

    // Accumulate averaged magnitude spectrum over all Hann windows
    std::vector<float> avgMag(kFftN / 2, 0.f);
    int nWindows = 0;

    for (size_t start = 0;
         start + kFftN <= static_cast<size_t>(audio.numFrames);
         start += hop)
    {
        // Mono-sum frame
        std::vector<float> frame(kFftN);
        for (size_t i = 0; i < kFftN; ++i) {
            float mono = 0.f;
            for (int c = 0; c < nch; ++c)
                mono += audio.samples[static_cast<size_t>(c)][start + i];
            frame[i] = mono / static_cast<float>(nch);
        }

        const auto mag = magnitudeSpectrum(frame.data(), kFftN);
        for (size_t k = 0; k < kFftN / 2; ++k)
            avgMag[k] += mag[k];
        ++nWindows;
    }

    if (nWindows == 0) return corr;
    for (float& m : avgMag) m /= static_cast<float>(nWindows);

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
