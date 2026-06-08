#pragma once

#include "mastertweak/analysis.hpp"
#include "mastertweak/dsp/biquad.hpp"
#include <vector>

namespace mt::dsp {

// Applies narrow bell corrections for each enabled ResonancePeak.
// Reuses BiquadCoeffs::bell() with negative gainDb and high Q.
class ResonanceEq {
public:
    void prepare(float sampleRate, int numChannels);
    void setResonances(const std::vector<mt::ResonancePeak>& peaks);
    void reset();
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    struct Band {
        BiquadCoeffs                coeffs;
        std::vector<BiquadState>    state;  // one per channel
    };
    std::vector<Band> bands_;
    float sampleRate_  = 44100.f;
    int   numChannels_ = 2;
};

} // namespace mt::dsp
