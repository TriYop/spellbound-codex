#pragma once

#include "mastertweak/advice.hpp"

#include <vector>

namespace mt::dsp {

// Lookahead brickwall limiter with guaranteed ceiling enforcement.
//
// Architecture (offline two-pass):
//   1. Per-frame peak → required gain (ceiling / peak).
//   2. Sliding-window minimum over [f, f+lookahead-1]: ensures gain is already
//      at its required level when the peak sample arrives (lookahead = 5 ms).
//   3. Forward release smoothing (50 ms) + multiply.
//
// Designed for offline use: processes the entire buffer at once.
// The ceiling is guaranteed never to be exceeded (unlike simple single-pass
// delay-buffer limiters where release recovery during the lookahead period
// allows peaks through).
class Limiter {
public:
    void prepare(float sampleRate, int numChannels, int maxBlockSize = 65536);
    void setAdvice(const mt::LimiterParams& p);
    void reset();
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    static float dbToLinear(float db) noexcept;
    static float linearToDb(float lin) noexcept;

    float sampleRate_  = 44100.f;
    int   numChannels_ = 2;
    float ceiling_     = 0.89125f;  // -1 dBTP in linear
    float releaseCoef_ = 0.f;       // per-sample release coefficient
    int   delayLen_    = 0;         // lookahead window in samples (5 ms)
    float gainEnv_     = 1.f;       // carry-over gain state across calls
};

} // namespace mt::dsp
