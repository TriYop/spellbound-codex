#include "mastertweak/dsp/resonance_eq.hpp"

namespace mt::dsp {

void ResonanceEq::prepare(float sampleRate, int numChannels) {
    sampleRate_  = sampleRate;
    numChannels_ = numChannels;
    bands_.clear();
    reset();
}

void ResonanceEq::setResonances(const std::vector<mt::ResonancePeak>& peaks) {
    bands_.clear();
    for (const auto& peak : peaks) {
        if (!peak.enabled)
            continue;
        Band band;
        band.coeffs = BiquadCoeffs::bell(peak.freqHz, peak.q, peak.gainDb, sampleRate_);
        band.state.resize(static_cast<size_t>(numChannels_), BiquadState{});
        bands_.push_back(std::move(band));
    }
}

void ResonanceEq::reset() {
    for (auto& band : bands_)
        for (auto& st : band.state)
            st.reset();
}

void ResonanceEq::process(std::vector<std::vector<float>>& samples, int numFrames) {
    for (int f = 0; f < numFrames; ++f) {
        for (auto& band : bands_) {
            for (int ch = 0; ch < numChannels_; ++ch) {
                samples[static_cast<size_t>(ch)][static_cast<size_t>(f)] =
                    biquadProcess(band.coeffs,
                                  band.state[static_cast<size_t>(ch)],
                                  samples[static_cast<size_t>(ch)][static_cast<size_t>(f)]);
            }
        }
    }
}

} // namespace mt::dsp
