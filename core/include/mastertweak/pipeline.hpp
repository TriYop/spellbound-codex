#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/dsp/gain_stager.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/preset.hpp"
#include "mastertweak/target_level.hpp"

#include <functional>
#include <optional>
#include <string>

namespace mt {

struct RenderOptions {
    int  outputBitDepth   = 24;
    bool outputFlac       = false;
    bool bypassResonanceEq = false;
    bool bypassEq         = false;
    bool bypassMbComp     = false;
    bool bypassSaturator  = false;
    bool bypassWidth      = false;
    bool bypassMixbusComp = false;
    bool bypassLimiter    = false;
    bool bypassDither     = false;
    std::optional<TargetLevelProfile> targetLevel;
};

struct GainStageReport {
    dsp::StageLevelReport postMbComp;   // RMS restore after multiband compressor
    dsp::StageLevelReport postMixbus;   // peak trim after mixbus compressor
};

// Full mastering result for one file.
struct MasterResult {
    AnalysisSnapshot  analysis;
    AdviceSet         advice;
    float             preGainDb  = 0.f;   // gain applied before analysis (dB)
    GainStageReport   gainStages;
    bool              ok         = false;
    std::string       errMsg;
};

// Progress callback: fraction in [0, 1], description string.
using ProgressCallback = std::function<void(float fraction, const std::string& stage)>;

// Analyse + derive advice without rendering.
MasterResult analyseOnly(const std::string& inputPath,
                         const PresetData&  preset,
                         std::string*       errOut = nullptr);

// Full mastering render: load → analyse → derive → apply DSP chain → write.
// adviceOverride: if provided, uses those values instead of deriving from analysis.
MasterResult renderFile(const std::string&      inputPath,
                        const std::string&      outputPath,
                        const PresetData&       preset,
                        const RenderOptions&    opts           = {},
                        const AdviceSet*        adviceOverride = nullptr,
                        const ProgressCallback& progress       = {},
                        std::string*            errOut         = nullptr);

} // namespace mt
