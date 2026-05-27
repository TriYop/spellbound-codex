#pragma once

#include "mastertweak/advice.hpp"

#include <vector>

namespace mt::dsp {

// Soft-clip saturator using a tanh transfer function.
// Drive increases the pre-gain (waveshaping input), with matching output trim
// so unity gain is preserved at 0 dB drive.
class Saturator {
public:
    void prepare(float sampleRate, int numChannels);
    void setAdvice(const mt::SaturatorParams& p);
    void reset();
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    float preGain_   = 1.f;
    float postGain_  = 1.f;
};

} // namespace mt::dsp
