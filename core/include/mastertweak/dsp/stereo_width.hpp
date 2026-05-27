#pragma once

#include "linkwitz_riley.hpp"
#include "mastertweak/advice.hpp"

#include <array>
#include <vector>

namespace mt::dsp {

// Per-band stereo width processor using M/S.
// For each band: decode to M/S, scale S by width factor, re-encode.
// Width = 1.0 → unchanged; < 1.0 → narrower; > 1.0 → wider.
class StereoWidth {
public:
    static constexpr int kNumBands = SevenBandSplitter::kNumBands;

    void prepare(float sampleRate, int numChannels);
    void setAdvice(const std::array<mt::BandWidth, kNumBands>& bands);
    void reset();
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    SevenBandSplitter                  splitter_;
    std::array<float, kNumBands>       widths_{};
    int numChannels_ = 2;
};

} // namespace mt::dsp
