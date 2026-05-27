#include "mastertweak/dsp/saturator.hpp"

#include <cmath>

namespace mt::dsp {

void Saturator::prepare(float /*sampleRate*/, int /*numChannels*/) {}

void Saturator::setAdvice(const mt::SaturatorParams& p) {
    // drive 0 dB = preGain 1, drive 6 dB = preGain 2
    preGain_  = std::pow(10.f, p.driveDb / 20.f);
    // Normalise output so tanh(preGain_ * 1.0) → 1.0
    postGain_ = preGain_ > 1e-6f ? 1.f / std::tanh(preGain_) : 1.f;
}

void Saturator::reset() {}

void Saturator::process(std::vector<std::vector<float>>& samples, int numFrames) {
    if (preGain_ == 1.f) return;  // no-op at 0 dB drive
    for (auto& ch : samples)
        for (int f = 0; f < numFrames; ++f) {
            const auto fs = static_cast<size_t>(f);
            ch[fs] = std::tanh(ch[fs] * preGain_) * postGain_;
        }
}

} // namespace mt::dsp
