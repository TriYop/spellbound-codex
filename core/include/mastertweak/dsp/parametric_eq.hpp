#pragma once

#include "biquad.hpp"
#include "mastertweak/advice.hpp"

#include <array>
#include <vector>

namespace mt::dsp {

// 7-band parametric EQ driven directly by AdviceSet::eq[].
// Processes offline (no block-size constraints, double-precision state).
class ParametricEq {
public:
    static constexpr int kNumBands = 7;

    void prepare(float sampleRate, int numChannels);
    void setAdvice(const std::array<mt::BandEq, kNumBands>& bands);
    void reset();

    // Process a single sample on channel `ch`, in-place.
    float process(int ch, float x) noexcept;

    // Process a full deinterleaved buffer in-place.
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    float sampleRate_  = 44100.f;
    int   numChannels_ = 2;

    std::array<BiquadCoeffs, kNumBands> coeffs_{};
    // state[band][channel]
    std::array<std::vector<BiquadState>, kNumBands> state_{};
};

} // namespace mt::dsp
