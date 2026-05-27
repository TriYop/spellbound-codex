#include "mastertweak/dsp/linkwitz_riley.hpp"

#include <stdexcept>

namespace mt::dsp {

static constexpr float kQ707 = 0.7071067811865476f;

void SevenBandSplitter::LR4::setLowpass(float fc, float sr, int nch) {
    c = BiquadCoeffs::lowpass(fc, kQ707, sr);
    st.assign(static_cast<size_t>(nch), {});
}

void SevenBandSplitter::LR4::setHighpass(float fc, float sr, int nch) {
    c = BiquadCoeffs::highpass(fc, kQ707, sr);
    st.assign(static_cast<size_t>(nch), {});
}

void SevenBandSplitter::LR4::reset(int nch) {
    st.assign(static_cast<size_t>(nch), {});
}

float SevenBandSplitter::LR4::process(int ch, float x) noexcept {
    const auto ci = static_cast<size_t>(ch);
    return biquadProcess(c, st[ci][1], biquadProcess(c, st[ci][0], x));
}

void SevenBandSplitter::prepare(float sampleRate, int numChannels) {
    sampleRate_  = sampleRate;
    numChannels_ = numChannels;
    for (int i = 0; i < kNumCrossovers; ++i) {
        lpFilters_[static_cast<size_t>(i)].setLowpass(kCrossoverHz[i], sampleRate, numChannels);
        hpFilters_[static_cast<size_t>(i)].setHighpass(kCrossoverHz[i], sampleRate, numChannels);
    }
}

void SevenBandSplitter::reset() {
    for (auto& f : lpFilters_) f.reset(numChannels_);
    for (auto& f : hpFilters_) f.reset(numChannels_);
}

void SevenBandSplitter::process(const std::vector<std::vector<float>>& input,
                                 std::vector<std::vector<std::vector<float>>>& bands,
                                 int numFrames) {
    // bands[band][channel][frame] — caller must pre-allocate
    if (static_cast<int>(bands.size()) != kNumBands) throw std::logic_error("bands size mismatch");

    // remainder[ch][frame] — copy of current remainder after each HP stage
    std::vector<std::vector<float>> remainder(input.begin(), input.end());
    // Resize each channel slice to numFrames
    for (auto& ch : remainder) ch.resize(static_cast<size_t>(numFrames));

    std::vector<std::vector<float>> bandBuf(static_cast<size_t>(numChannels_),
                                             std::vector<float>(static_cast<size_t>(numFrames)));

    for (int ci = 0; ci < kNumCrossovers; ++ci) {
        const auto cis = static_cast<size_t>(ci);
        // LP → bandBuf; HP → remainder
        for (int ch = 0; ch < numChannels_; ++ch) {
            const auto chs = static_cast<size_t>(ch);
            for (int f = 0; f < numFrames; ++f) {
                const auto fs = static_cast<size_t>(f);
                const float x        = remainder[chs][fs];
                bandBuf[chs][fs]     = lpFilters_[cis].process(ch, x);
                remainder[chs][fs]   = hpFilters_[cis].process(ch, x);
            }
        }
        // Copy bandBuf → bands[ci]
        for (int ch = 0; ch < numChannels_; ++ch) {
            const auto chs = static_cast<size_t>(ch);
            bands[cis][chs].assign(bandBuf[chs].begin(),
                                    bandBuf[chs].begin() + numFrames);
        }
    }
    // Final band = whatever remains
    for (int ch = 0; ch < numChannels_; ++ch) {
        const auto chs = static_cast<size_t>(ch);
        bands[static_cast<size_t>(kNumCrossovers)][chs]
            .assign(remainder[chs].begin(), remainder[chs].begin() + numFrames);
    }
}

} // namespace mt::dsp
