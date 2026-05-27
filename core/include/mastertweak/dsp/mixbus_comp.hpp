#pragma once

#include "compressor.hpp"
#include "mastertweak/advice.hpp"

#include <vector>

namespace mt::dsp {

// Single-band mixbus compressor with makeup gain.
class MixbusComp {
public:
    void prepare(float sampleRate, int numChannels);
    void setAdvice(const mt::MixbusComp& params);
    void reset();
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    Compressor comp_;
    float      makeupLinear_ = 1.f;
};

} // namespace mt::dsp
