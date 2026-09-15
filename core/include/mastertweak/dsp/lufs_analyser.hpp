#pragma once

#include "audioplugins/common/analysis/LoudnessAnalyser.h"

#include <vector>

namespace mt::dsp {

using LoudnessMetrics = audioplugins::common::analysis::LoudnessMetrics;

// Thin wrapper: forwards to AudioPluginsCommon::analysis::LoudnessAnalyser,
// which is itself ported from this file's original K-weighting/gating math
// (see Common's design spec). Kept as a distinct mt::dsp class -- rather
// than a bare alias -- only because this class historically took a float
// sampleRate in prepare() while Common's takes double; the wrapper absorbs
// that conversion so no call site needs to change.
class LufsAnalyser {
public:
    void prepare(float sampleRate, int numChannels) {
        impl_.prepare(static_cast<double>(sampleRate), numChannels);
    }

    float measure(const std::vector<std::vector<float>>& samples, int numFrames) {
        return impl_.measure(samples, numFrames);
    }

    LoudnessMetrics measureWithLra(const std::vector<std::vector<float>>& samples, int numFrames) {
        return impl_.measureWithLra(samples, numFrames);
    }

private:
    audioplugins::common::analysis::LoudnessAnalyser impl_;
};

} // namespace mt::dsp
