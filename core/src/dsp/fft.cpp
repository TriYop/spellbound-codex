#include "mastertweak/dsp/fft.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <vector>

namespace mt::dsp {

// In-place iterative Cooley-Tukey DIT FFT (N must be a power of 2).
void inplaceFft(std::vector<float>& re, std::vector<float>& im) {
    assert((re.size() & (re.size() - 1)) == 0 && !re.empty()); // must be power of 2
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

// Returns averaged magnitude spectrum (bins 0..fftN/2-1) from a mono buffer,
// using Hann window and 50% overlap across all frames.
std::vector<float> averagedMagnitudeSpectrum(const float* samples,
                                              int          numFrames,
                                              int          fftN) {
    const size_t N   = static_cast<size_t>(fftN);
    const size_t hop = N / 2;  // 50% overlap

    std::vector<float> avgMag(N / 2, 0.f);
    int nWindows = 0;

    for (size_t start = 0;
         start + N <= static_cast<size_t>(numFrames);
         start += hop)
    {
        std::vector<float> re(N), im(N, 0.f);
        for (size_t i = 0; i < N; ++i) {
            const float w = 0.5f * (1.f - std::cos(2.f * std::numbers::pi_v<float>
                                                    * static_cast<float>(i)
                                                    / static_cast<float>(N)));
            re[i] = samples[start + i] * w;
        }
        inplaceFft(re, im);
        for (size_t k = 0; k < N / 2; ++k)
            avgMag[k] += std::sqrt(re[k] * re[k] + im[k] * im[k]);
        ++nWindows;
    }

    if (nWindows > 0) {
        const float invN = 1.f / static_cast<float>(nWindows);
        for (float& m : avgMag) m *= invN;
    }

    return avgMag;
}

} // namespace mt::dsp
