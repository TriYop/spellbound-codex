#include "mastertweak/advice.hpp"

#include "audioplugins/common/analysis/AdviceSet.h"

namespace mt {

namespace {

namespace ca = audioplugins::common::analysis;

ca::AnalysisSnapshot toCommonSnapshot(const AnalysisSnapshot& snap) {
    ca::AnalysisSnapshot out;
    for (int i = 0; i < AnalysisSnapshot::kNumBands; ++i) {
        const auto& b = snap.bands[static_cast<size_t>(i)];
        out.bands[static_cast<size_t>(i)] = ca::BandStats{
            b.avgRmsDb, b.peakRmsDb, b.p10RmsDb, b.p50RmsDb, b.p95RmsDb, b.correlation, b.crestDb
        };
    }
    out.overallAvgDb  = snap.overallAvgDb;
    out.overallPeakDb = snap.overallPeakDb;
    out.overallCorr   = snap.overallCorr;
    out.lraLu         = snap.lraLu;
    return out;
}

ca::PresetData toCommonPreset(const PresetData& preset) {
    ca::PresetData out;
    out.name             = preset.name;
    out.description       = preset.description;
    out.bandRmsDb         = preset.bandRmsDb;
    out.bandMinCorr       = preset.bandMinCorr;
    out.bandTransientDb   = preset.bandTransientDb;
    out.overallRmsDb      = preset.overallRmsDb;
    out.overallMinCorr    = preset.overallMinCorr;
    return out;
}

AdviceSet fromCommonAdvice(const ca::AdviceSet& advice) {
    AdviceSet out;
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto is = static_cast<size_t>(i);
        out.eq[is]     = { advice.eq[is].gainDb, advice.eq[is].q, advice.eq[is].isShelf, advice.eq[is].freqHz };
        out.mbComp[is] = { advice.mbComp[is].thresholdDb, advice.mbComp[is].ratio,
                            advice.mbComp[is].attackMs, advice.mbComp[is].releaseMs };
        out.width[is]  = { advice.width[is].width };
    }
    out.mixbusComp = { advice.mixbusComp.thresholdDb, advice.mixbusComp.ratio,
                        advice.mixbusComp.attackMs, advice.mixbusComp.releaseMs, advice.mixbusComp.makeupDb };
    out.saturator  = { advice.saturator.driveDb };
    out.limiter    = { advice.limiter.targetLufsApprox, advice.limiter.ceilingDb };
    // advice.resonances is intentionally not copied -- Common's deriveAdvice()
    // never populates it either (see AdviceSet.h), matching this file's
    // existing behavior where resonances is set separately by pipeline.cpp.
    return out;
}

} // namespace

AdviceSet deriveAdvice(const AnalysisSnapshot& snap, const PresetData& preset) {
    const auto commonAdvice = ca::deriveAdvice(toCommonSnapshot(snap), toCommonPreset(preset));
    return fromCommonAdvice(commonAdvice);
}

} // namespace mt
