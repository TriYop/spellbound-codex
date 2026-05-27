#include "mastertweak/dsp/stereo_width.hpp"

namespace mt::dsp {

void StereoWidth::prepare(float sampleRate, int numChannels) {
    numChannels_ = numChannels;
    splitter_.prepare(sampleRate, numChannels);
    widths_.fill(1.f);
}

void StereoWidth::setAdvice(const std::array<mt::BandWidth, kNumBands>& bands) {
    for (int i = 0; i < kNumBands; ++i)
        widths_[static_cast<size_t>(i)] = bands[static_cast<size_t>(i)].width;
}

void StereoWidth::reset() {
    splitter_.reset();
}

void StereoWidth::process(std::vector<std::vector<float>>& samples, int numFrames) {
    if (numChannels_ < 2) return;

    std::vector<std::vector<std::vector<float>>> bands(
        static_cast<size_t>(kNumBands),
        std::vector<std::vector<float>>(
            static_cast<size_t>(numChannels_),
            std::vector<float>(static_cast<size_t>(numFrames), 0.f)));

    splitter_.process(samples, bands, numFrames);

    // M/S width per band
    for (int b = 0; b < kNumBands; ++b) {
        const float w = widths_[static_cast<size_t>(b)];
        if (w == 1.f) continue;

        auto& bL = bands[static_cast<size_t>(b)][0];
        auto& bR = bands[static_cast<size_t>(b)][1];

        for (int f = 0; f < numFrames; ++f) {
            const auto fs = static_cast<size_t>(f);
            const float M  = (bL[fs] + bR[fs]) * 0.5f;
            const float S  = (bL[fs] - bR[fs]) * 0.5f * w;
            bL[fs] = M + S;
            bR[fs] = M - S;
        }
    }

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
