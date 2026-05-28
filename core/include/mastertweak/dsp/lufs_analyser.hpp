#pragma once

#include "mastertweak/dsp/biquad.hpp"

#include <vector>

namespace mt::dsp {

// EBU R128 integrated loudness measurement (ITU-R BS.1770-4).
// K-weighting: Stage 1 high-shelf (1682 Hz, +4 dB) + Stage 2 high-pass (38 Hz).
// 400 ms blocks, 75% overlap, absolute gate -70 LUFS, relative gate -10 LU.
// Returns: L = -0.691 + 10*log10(gated_mean_power).
// Returns -70.f when no blocks survive gating (silence or sub-block buffers).
class LufsAnalyser {
public:
    void prepare(float sampleRate, int numChannels);

    // Measure integrated LUFS of the full buffer [numChannels × numFrames].
    float measure(const std::vector<std::vector<float>>& samples, int numFrames);

private:
    static BiquadCoeffs kWeightingStage1(double sr);
    static BiquadCoeffs kWeightingStage2(double sr);

    float sampleRate_  = 48000.f;
    int   numChannels_ = 2;
    int   blockSize_   = 0;   // 400 ms in samples
    int   hopSize_     = 0;   // 100 ms in samples (75% overlap)
    BiquadCoeffs stage1_{};
    BiquadCoeffs stage2_{};
};

} // namespace mt::dsp
