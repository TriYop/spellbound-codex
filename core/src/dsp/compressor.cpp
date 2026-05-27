#include "mastertweak/dsp/compressor.hpp"

#include <algorithm>
#include <cmath>

namespace mt::dsp {

void Compressor::prepare(float sampleRate, int /*numChannels*/) {
    sampleRate_ = sampleRate;
    reset();
}

void Compressor::setParams(const Params& p) {
    threshold_    = p.thresholdDb;
    ratio_        = std::max(1.f, p.ratio);
    kneeDb_       = p.kneeDb;
    const float sr = sampleRate_;
    attackCoef_   = std::exp(-1.f / (p.attackMs  * 0.001f * sr));
    releaseCoef_  = std::exp(-1.f / (p.releaseMs * 0.001f * sr));
}

void Compressor::reset() {
    envelopeRms_      = 0.f;
    gainReductionDb_  = 0.f;
}

float Compressor::computeGainDb(float rmsDb) const noexcept {
    const float half = kneeDb_ * 0.5f;
    if (kneeDb_ > 0.f && rmsDb >= threshold_ - half && rmsDb <= threshold_ + half) {
        // Soft knee
        const float x = rmsDb - threshold_ + half;
        return (1.f / ratio_ - 1.f) * (x * x) / (2.f * kneeDb_);
    }
    if (rmsDb > threshold_ + (kneeDb_ > 0.f ? half : 0.f)) {
        return (rmsDb - threshold_) * (1.f / ratio_ - 1.f);
    }
    return 0.f;
}

void Compressor::processStereoFrame(float& l, float& r) noexcept {
    // Sidechain RMS from stereo pair
    const float rmsLin = std::sqrt(0.5f * (l * l + r * r));
    // Smooth envelope
    const float coef   = rmsLin > envelopeRms_ ? attackCoef_ : releaseCoef_;
    envelopeRms_       = coef * envelopeRms_ + (1.f - coef) * rmsLin;

    const float envDb  = linearToDb(envelopeRms_);
    gainReductionDb_   = computeGainDb(envDb);  // ≤ 0 (gain reduction)
    const float gainLin = dbToLinear(gainReductionDb_);

    l *= gainLin;
    r *= gainLin;
}

void Compressor::process(std::vector<std::vector<float>>& samples, int numFrames) {
    if (samples.size() < 2) return;
    auto& L = samples[0];
    auto& R = samples[1];
    for (int f = 0; f < numFrames; ++f)
        processStereoFrame(L[static_cast<size_t>(f)], R[static_cast<size_t>(f)]);
}

} // namespace mt::dsp
