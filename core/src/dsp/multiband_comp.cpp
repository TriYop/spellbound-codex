#include "mastertweak/dsp/multiband_comp.hpp"

namespace mt::dsp {

void MultibandComp::prepare(float sampleRate, int numChannels) {
    numChannels_ = numChannels;
    splitter_.prepare(sampleRate, numChannels);
    for (auto& c : comps_)
        c.prepare(sampleRate, numChannels);
}

void MultibandComp::setAdvice(const std::array<mt::BandComp, kNumBands>& bands) {
    for (int i = 0; i < kNumBands; ++i) {
        Compressor::Params p;
        p.thresholdDb = bands[static_cast<size_t>(i)].thresholdDb;
        p.ratio       = bands[static_cast<size_t>(i)].ratio;
        p.attackMs    = bands[static_cast<size_t>(i)].attackMs;
        p.releaseMs   = bands[static_cast<size_t>(i)].releaseMs;
        comps_[static_cast<size_t>(i)].setParams(p);
    }
}

void MultibandComp::reset() {
    splitter_.reset();
    for (auto& c : comps_) c.reset();
}

void MultibandComp::process(std::vector<std::vector<float>>& samples, int numFrames) {
    // Allocate per-band buffers
    std::vector<std::vector<std::vector<float>>> bands(
        static_cast<size_t>(kNumBands),
        std::vector<std::vector<float>>(
            static_cast<size_t>(numChannels_),
            std::vector<float>(static_cast<size_t>(numFrames), 0.f)));

    splitter_.process(samples, bands, numFrames);

    // Compress each band in-place
    for (int b = 0; b < kNumBands; ++b)
        comps_[static_cast<size_t>(b)].process(bands[static_cast<size_t>(b)], numFrames);

    // Sum bands back
    for (int ch = 0; ch < numChannels_; ++ch) {
        const auto chs = static_cast<size_t>(ch);
        auto& out = samples[chs];
        std::fill(out.begin(), out.begin() + numFrames, 0.f);
        for (int b = 0; b < kNumBands; ++b)
            for (int f = 0; f < numFrames; ++f)
                out[static_cast<size_t>(f)] += bands[static_cast<size_t>(b)][chs][static_cast<size_t>(f)];
    }
}

} // namespace mt::dsp
