#include "mastertweak/dsp/mixbus_comp.hpp"

#include <cmath>

namespace mt::dsp {

void MixbusComp::prepare(float sampleRate, int numChannels) {
    comp_.prepare(sampleRate, numChannels);
}

void MixbusComp::setAdvice(const mt::MixbusComp& params) {
    Compressor::Params p;
    p.thresholdDb = params.thresholdDb;
    p.ratio       = params.ratio;
    p.attackMs    = params.attackMs;
    p.releaseMs   = params.releaseMs;
    comp_.setParams(p);
    makeupLinear_ = std::pow(10.f, params.makeupDb / 20.f);
}

void MixbusComp::reset() {
    comp_.reset();
}

void MixbusComp::process(std::vector<std::vector<float>>& samples, int numFrames) {
    comp_.process(samples, numFrames);
    if (makeupLinear_ != 1.f) {
        for (auto& ch : samples)
            for (int f = 0; f < numFrames; ++f)
                ch[static_cast<size_t>(f)] *= makeupLinear_;
    }
}

} // namespace mt::dsp
