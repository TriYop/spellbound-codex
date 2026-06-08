#pragma once

#include <vector>

namespace mt::dsp {

/// Radix-2 Cooley-Tukey DIT FFT, in-place. N must be a power of 2.
void inplaceFft(std::vector<float>& re, std::vector<float>& im);

/// Returns averaged magnitude spectrum (bins 0..fftN/2-1) from a mono buffer,
/// using Hann window and 50% overlap across all frames.
std::vector<float> averagedMagnitudeSpectrum(const float* samples,
                                              int          numFrames,
                                              int          fftN,
                                              float        sampleRate);

} // namespace mt::dsp
