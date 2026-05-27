#include "mastertweak/dsp/parametric_eq.hpp"

namespace mt::dsp {

void ParametricEq::prepare(float sampleRate, int numChannels) {
    sampleRate_  = sampleRate;
    numChannels_ = numChannels;
    for (auto& st : state_)
        st.assign(static_cast<size_t>(numChannels), BiquadState{});
}

void ParametricEq::setAdvice(const std::array<mt::BandEq, kNumBands>& bands) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto is = static_cast<size_t>(i);
        const auto& eq = bands[is];
        if (eq.gainDb == 0.f) {
            coeffs_[is] = BiquadCoeffs{};  // identity (b0=1, rest 0)
        } else if (eq.isShelf) {
            coeffs_[is] = (eq.freqHz < 1000.f)
                ? BiquadCoeffs::lowShelf(eq.freqHz, eq.gainDb, sampleRate_)
                : BiquadCoeffs::highShelf(eq.freqHz, eq.gainDb, sampleRate_);
        } else {
            coeffs_[is] = BiquadCoeffs::bell(eq.freqHz, eq.q, eq.gainDb, sampleRate_);
        }
    }
}

void ParametricEq::reset() {
    for (auto& st : state_)
        for (auto& s : st) s.reset();
}

float ParametricEq::process(int ch, float x) noexcept {
    for (int i = 0; i < kNumBands; ++i)
        x = biquadProcess(coeffs_[static_cast<size_t>(i)],
                          state_[static_cast<size_t>(i)][static_cast<size_t>(ch)], x);
    return x;
}

void ParametricEq::process(std::vector<std::vector<float>>& samples, int numFrames) {
    for (int ch = 0; ch < numChannels_; ++ch)
        for (int f = 0; f < numFrames; ++f)
            samples[static_cast<size_t>(ch)][static_cast<size_t>(f)] =
                process(ch, samples[static_cast<size_t>(ch)][static_cast<size_t>(f)]);
}

} // namespace mt::dsp
