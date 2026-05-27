#pragma once

#include "linkwitz_riley.hpp"
#include "compressor.hpp"
#include "mastertweak/advice.hpp"

#include <array>
#include <vector>

namespace mt::dsp {

// 7-band multiband compressor: split → per-band compression → sum.
// Band split uses the same LR4 crossover topology as the analyser so
// the advice-derived thresholds / ratios are meaningful.
class MultibandComp {
public:
    static constexpr int kNumBands = SevenBandSplitter::kNumBands;

    void prepare(float sampleRate, int numChannels);
    void setAdvice(const std::array<mt::BandComp, kNumBands>& bands);
    void reset();

    // Process a full deinterleaved stereo buffer in-place.
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    SevenBandSplitter         splitter_;
    std::array<Compressor, kNumBands> comps_;
    int numChannels_ = 2;
};

} // namespace mt::dsp
