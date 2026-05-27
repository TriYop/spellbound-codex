#pragma once

#include <cmath>
#include <vector>

namespace mt::dsp {

// Simple RMS-detection compressor with attack / release smoothing.
// Per-channel, single-band. Does NOT apply makeup gain (caller adds it).
class Compressor {
public:
    struct Params {
        float thresholdDb = -20.f;
        float ratio       =   2.f;   // e.g. 2 = 2:1
        float attackMs    =  10.f;
        float releaseMs   = 100.f;
        float kneeDb      =   2.f;   // soft-knee width (0 = hard knee)
    };

    void prepare(float sampleRate, int numChannels);
    void setParams(const Params& p);
    void reset();

    // Process one stereo frame (sidechain RMS over both channels → shared GR).
    void processStereoFrame(float& l, float& r) noexcept;

    // Process a full deinterleaved stereo buffer.
    void process(std::vector<std::vector<float>>& samples, int numFrames);

    // Instantaneous gain reduction (dB), useful for metering.
    float gainReductionDb() const noexcept { return gainReductionDb_; }

private:
    float sampleRate_  = 44100.f;
    float threshold_   = 0.f;    // threshold in dBFS
    float ratio_       = 2.f;
    float kneeDb_      = 2.f;
    float attackCoef_  = 0.f;
    float releaseCoef_ = 0.f;

    float envelopeRms_ = 0.f;   // smoothed RMS envelope (linear)
    float gainReductionDb_ = 0.f;

    float computeGainDb(float rmsDb) const noexcept;
    static float dbToLinear(float db) noexcept { return std::pow(10.f, db / 20.f); }
    static float linearToDb(float lin) noexcept {
        return lin > 1e-7f ? 20.f * std::log10(lin) : -100.f;
    }
};

} // namespace mt::dsp
