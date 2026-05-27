#pragma once

#include "biquad.hpp"

#include <array>
#include <vector>

namespace mt::dsp {

// 4th-order Linkwitz-Riley 7-band filterbank.
// LP[i] × LP[i+1] × … peels off one band at a time from the bottom up;
// the remainder after all 6 crossovers is the 7th band (Air, 16 kHz+).
//
// Crossovers: 80 / 250 / 500 / 2000 / 6000 / 16000 Hz (matches MixAdvice BandConfig).
class SevenBandSplitter {
public:
    static constexpr int kNumBands      = 7;
    static constexpr int kNumCrossovers = kNumBands - 1;
    static constexpr float kCrossoverHz[kNumCrossovers] = {
        80.f, 250.f, 500.f, 2000.f, 6000.f, 16000.f
    };

    void prepare(float sampleRate, int numChannels);
    void reset();

    // Split `numFrames` interleaved stereo samples (L0 R0 L1 R1 …) into
    // per-band deinterleaved buffers. `bands[b]` has shape [numChannels][numFrames].
    void process(const std::vector<std::vector<float>>& input,
                 std::vector<std::vector<std::vector<float>>>& bands,
                 int numFrames);

private:
    float sampleRate_ = 44100.f;
    int   numChannels_ = 2;

    struct LR4 {
        BiquadCoeffs c{};
        std::vector<std::array<BiquadState, 2>> st;  // [ch][stage]
        void setLowpass(float fc, float sr, int nch);
        void setHighpass(float fc, float sr, int nch);
        void reset(int nch);
        float process(int ch, float x) noexcept;
    };

    std::array<LR4, kNumCrossovers> lpFilters_;
    std::array<LR4, kNumCrossovers> hpFilters_;
};

} // namespace mt::dsp
