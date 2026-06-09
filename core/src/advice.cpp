#include "mastertweak/advice.hpp"

#include <algorithm>
#include <cmath>

namespace mt {

// Per-band release times (ms) — lower bands need longer release.
// Matches the kRelMs[] array in MixAdvice/Source/PluginEditor.cpp:516.
static constexpr float kRelMs[AdviceSet::kNumBands] = {
    250.f, 160.f, 120.f, 100.f, 70.f, 50.f, 30.f
};

AdviceSet deriveAdvice(const AnalysisSnapshot& snap, const PresetData& preset) {
    AdviceSet out;

    // ── Per-band EQ + multiband compression ──────────────────────────────────
    // "Characteristic level" = mean of long-term average and peak-hold,
    // then averaged L/R — exactly as MixAdvice/PluginEditor.cpp:532-534.
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto bi = static_cast<size_t>(i);
        const float refDb = (snap.bands[bi].p50RmsDb + snap.bands[bi].p95RmsDb) * 0.5f;

        // ── EQ ────────────────────────────────────────────────────────────────
        float eqGain = std::clamp(preset.bandRmsDb[bi] - refDb, -12.f, 12.f);
        if (std::abs(eqGain) < 0.5f) eqGain = 0.f;

        const float absGain = std::abs(eqGain);
        float q = absGain < 3.f ? 0.7f
                : absGain < 6.f ? 1.0f
                : absGain < 9.f ? 1.4f : 2.0f;

        out.eq[bi].gainDb  = eqGain;
        out.eq[bi].q       = q;
        out.eq[bi].isShelf = AnalysisSnapshot::kBandIsShelf[bi];
        out.eq[bi].freqHz  = AnalysisSnapshot::kBandCenterHz[bi];

        // ── Multiband compression ─────────────────────────────────────────────
        const float excess = std::max(0.f, refDb - preset.bandRmsDb[bi]);
        out.mbComp[bi].ratio     = std::clamp(1.f + excess * 0.25f, 1.1f, 8.f);
        out.mbComp[bi].thresholdDb = preset.bandRmsDb[bi] - 3.f;

        const float targetCrest  = preset.bandTransientDb[bi];
        out.mbComp[bi].attackMs  = targetCrest > 16.f ? 20.f
                                 : targetCrest > 12.f ? 10.f
                                 : targetCrest > 8.f  ?  5.f : 2.f;
        out.mbComp[bi].releaseMs = kRelMs[i];

        // ── Stereo width ──────────────────────────────────────────────────────
        const float corr      = snap.bands[bi].correlation;
        const float minCorr   = preset.bandMinCorr[bi];
        // Width = 1 when correlation is exactly at the floor (no change).
        // If correlation > floor: room to widen (width > 1, up to ~1.3).
        // If correlation < floor: pull in (width < 1, down to 0.7).
        const float corrDelta = corr - minCorr;
        out.width[bi].width = std::clamp(1.f + corrDelta * 0.5f, 0.7f, 1.3f);
    }

    // ── Mixbus compression ────────────────────────────────────────────────────
    const float overallDb  = (snap.overallAvgDb + snap.overallPeakDb) * 0.5f;
    const float overallExcess = std::max(0.f, overallDb - preset.overallRmsDb);

    out.mixbusComp.ratio       = std::clamp(2.f + overallExcess * 0.15f, 1.5f, 6.f);
    out.mixbusComp.thresholdDb = preset.overallRmsDb - 6.f;
    out.mixbusComp.attackMs    = 15.f;
    out.mixbusComp.releaseMs   = std::clamp(100.f + overallExcess * 5.f, 80.f, 300.f);
    // Expected GR → makeup compensates
    const float expGR = std::max(0.f, overallDb - out.mixbusComp.thresholdDb)
                        * (1.f - 1.f / out.mixbusComp.ratio);
    out.mixbusComp.makeupDb = std::clamp(expGR + std::max(0.f, preset.overallRmsDb - overallDb),
                                          0.f, 18.f);

    // ── Saturator drive ───────────────────────────────────────────────────────
    // Drive = average crest deficit across bands, clamped to [0, 6 dB].
    float crestDeficit = 0.f;
    for (size_t i = 0; i < static_cast<size_t>(AdviceSet::kNumBands); ++i)
        crestDeficit += snap.bands[i].crestDb - preset.bandTransientDb[i];
    crestDeficit /= static_cast<float>(AdviceSet::kNumBands);
    // Negative deficit = we need more transient / saturation
    out.saturator.driveDb = std::clamp(-crestDeficit * 0.4f, 0.f, 6.f);

    // ── Limiter target ────────────────────────────────────────────────────────
    // Approximate LUFS from overallRmsDb (RMS dBFS ≈ LUFS − 3 dB heuristic)
    out.limiter.targetLufsApprox = preset.overallRmsDb + 3.f;
    out.limiter.ceilingDb        = -1.f;

    return out;
}

} // namespace mt
