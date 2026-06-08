#include "mastertweak/pipeline.hpp"

#include "mastertweak/dsp/resonance_eq.hpp"
#include "mastertweak/dsp/parametric_eq.hpp"
#include "mastertweak/dsp/multiband_comp.hpp"
#include "mastertweak/dsp/mixbus_comp.hpp"
#include "mastertweak/dsp/saturator.hpp"
#include "mastertweak/dsp/stereo_width.hpp"
#include "mastertweak/dsp/limiter.hpp"
#include "mastertweak/dsp/dither.hpp"
#include "mastertweak/dsp/lufs_analyser.hpp"
#include "mastertweak/dsp/gain_stager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Normalize audio in-place to targetRmsDb (broadband RMS across all channels).
// Returns the gain applied in dB; returns 0 if silent.
static float applyPreGain(mt::AudioFile& audio, float targetRmsDb) {
    double sumSq = 0.0;
    long   count = 0;
    for (const auto& ch : audio.samples)
        for (float s : ch) { sumSq += static_cast<double>(s) * s; ++count; }

    if (count == 0) return 0.f;
    const float rmsLin = static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
    if (rmsLin < 1e-7f) return 0.f;

    const float measuredDb = 20.f * std::log10(rmsLin);
    const float gainDb     = std::clamp(targetRmsDb - measuredDb, -24.f, 24.f);
    const float gainLin    = std::pow(10.f, gainDb / 20.f);

    for (auto& ch : audio.samples)
        for (float& s : ch)
            s *= gainLin;

    return gainDb;
}

} // anonymous namespace

namespace mt {

MasterResult analyseOnly(const std::string& inputPath,
                         const PresetData&  preset,
                         std::string*       errOut) {
    MasterResult result;

    auto audio = readAudioFile(inputPath, errOut);
    if (!audio) return result;

    result.preGainDb = applyPreGain(*audio, preset.overallRmsDb);
    result.analysis  = analyseFile(*audio);
    result.advice   = deriveAdvice(result.analysis, preset);
    result.advice.resonances = detectResonances(*audio);
    result.ok       = true;
    return result;
}

MasterResult renderFile(const std::string&     inputPath,
                        const std::string&     outputPath,
                        const PresetData&      preset,
                        const RenderOptions&   opts,
                        const AdviceSet*       adviceOverride,
                        const ProgressCallback& progress,
                        std::string*           errOut) {
    MasterResult result;

    auto report = [&](float frac, const std::string& stage) {
        if (progress) progress(frac, stage);
    };

    // ── Load ─────────────────────────────────────────────────────────────────
    report(0.00f, "Loading");
    auto audio = readAudioFile(inputPath, errOut);
    if (!audio) return result;

    // ── Pre-analysis gain staging ─────────────────────────────────────────────
    // Normalize to the preset's target RMS so the analyser sees spectral
    // imbalance rather than overall loudness offset (quiet tracks would
    // otherwise receive +12 dB on every band uniformly).
    report(0.05f, "Pre-gain staging");
    result.preGainDb = applyPreGain(*audio, preset.overallRmsDb);

    // ── Analyse ───────────────────────────────────────────────────────────────
    report(0.10f, "Analysing");
    result.analysis = analyseFile(*audio);

    // ── Derive advice ─────────────────────────────────────────────────────────
    report(0.20f, "Deriving advice");
    result.advice = adviceOverride ? *adviceOverride : deriveAdvice(result.analysis, preset);
    if (!adviceOverride)
        result.advice.resonances = detectResonances(*audio);

    const AdviceSet& adv = result.advice;
    const float sr = static_cast<float>(audio->sampleRate);
    const int   nch = audio->numChannels;
    auto& buf = audio->samples;
    const int nf = audio->numFrames;

    // Snapshot reference RMS (post-pre-gain, pre-DSP) for post-multiband restore.
    dsp::GainStager gs;
    const float rmsDbBeforeChain = dsp::GainStager::measureRmsDb(buf, nf);

    // ── Resonance EQ ──────────────────────────────────────────────────────────────
    if (!opts.bypassResonanceEq && !adv.resonances.empty()) {
        report(0.23f, "Resonance EQ");
        dsp::ResonanceEq resEq;
        resEq.prepare(sr, nch);
        resEq.setResonances(adv.resonances);
        resEq.process(buf, nf);
    }

    // ── EQ ───────────────────────────────────────────────────────────────────
    if (!opts.bypassEq) {
        report(0.25f, "EQ");
        dsp::ParametricEq eq;
        eq.prepare(sr, nch);
        eq.setAdvice(adv.eq);
        eq.process(buf, nf);
    }

    // ── Multiband compressor ─────────────────────────────────────────────────
    if (!opts.bypassMbComp) {
        report(0.35f, "Multiband compression");
        dsp::MultibandComp mbComp;
        mbComp.prepare(sr, nch);
        mbComp.setAdvice(adv.mbComp);
        mbComp.process(buf, nf);
    }

    // ── Post-multiband gain staging ──────────────────────────────────────────
    // Restore RMS to pre-chain reference so saturator and mixbus comp see
    // the level their advice was calibrated for. Skip when bypassed — nothing
    // to compensate.
    if (!opts.bypassMbComp) {
        report(0.40f, "Gain staging (post-multiband)");
        result.gainStages.postMbComp = gs.restoreRms(buf, nf, rmsDbBeforeChain);
    }

    // ── Saturator ────────────────────────────────────────────────────────────
    if (!opts.bypassSaturator) {
        report(0.50f, "Saturation");
        dsp::Saturator sat;
        sat.prepare(sr, nch);
        sat.setAdvice(adv.saturator);
        sat.process(buf, nf);
    }

    // ── Stereo width ─────────────────────────────────────────────────────────
    if (!opts.bypassWidth) {
        report(0.60f, "Stereo width");
        dsp::StereoWidth width;
        width.prepare(sr, nch);
        width.setAdvice(adv.width);
        width.process(buf, nf);
    }

    // ── Mixbus compressor ─────────────────────────────────────────────────────
    if (!opts.bypassMixbusComp) {
        report(0.70f, "Mixbus compression");
        dsp::MixbusComp mbusComp;
        mbusComp.prepare(sr, nch);
        mbusComp.setAdvice(adv.mixbusComp);
        mbusComp.process(buf, nf);
    }

    // ── Target-level normalisation (EBU R128) ────────────────────────────────
    if (opts.targetLevel.has_value()) {
        report(0.75f, "Normalizing to target");
        dsp::LufsAnalyser la;
        la.prepare(sr, nch);
        const float measuredLufs = la.measure(buf, nf);
        float trimDb = opts.targetLevel->lufs - measuredLufs;
        // Clamp to ±24 dB to guard against silence or very short inputs.
        trimDb = std::clamp(trimDb, -24.f, 24.f);
        const float gain = std::pow(10.f, trimDb / 20.f);
        for (auto& ch : buf)
            for (int f = 0; f < nf; ++f)
                ch[static_cast<size_t>(f)] *= gain;
        // Override ceiling and LUFS target in the returned advice (GUI display).
        // Warn if an explicit ceiling override was also supplied — target-level wins.
        if (adviceOverride
                && adviceOverride->limiter.ceilingDb != opts.targetLevel->peakCeiling)
            std::fprintf(stderr, "warning: --target-level overrides --override-limiter-ceiling"
                                 " (ceiling set to %.1f dBTP)\n",
                         static_cast<double>(opts.targetLevel->peakCeiling));
        result.advice.limiter.ceilingDb        = opts.targetLevel->peakCeiling;
        result.advice.limiter.targetLufsApprox = opts.targetLevel->lufs;
    }

    // ── Pre-limiter peak trim ────────────────────────────────────────────────
    // Final safety: trim peaks > -3 dBFS so the limiter operates in its clean
    // range regardless of what target-level normalisation may have added.
    report(0.78f, "Gain staging (pre-limiter)");
    result.gainStages.postMixbus = gs.trimPeak(buf, nf, -3.f);

    // ── Limiter ───────────────────────────────────────────────────────────────
    // Note: when bypassLimiter=true the pre-limiter peak trim above still runs;
    // the caller opted out of the brickwall ceiling only.
    if (!opts.bypassLimiter) {
        report(0.80f, "Limiting");
        dsp::Limiter lim;
        lim.prepare(sr, nch);
        lim.setAdvice(result.advice.limiter);
        lim.process(buf, nf);
    }

    // ── Dither ────────────────────────────────────────────────────────────────
    if (!opts.bypassDither) {
        report(0.90f, "Dithering");
        dsp::Dither dither;
        dither.prepare(opts.outputBitDepth, nch);
        dither.process(buf, nf);
    }

    // ── Write ─────────────────────────────────────────────────────────────────
    report(0.95f, "Writing");
    WriteOptions wo;
    wo.bitDepth = opts.outputBitDepth;
    wo.flac     = opts.outputFlac;
    if (!writeAudioFile(outputPath, *audio, wo, errOut)) return result;

    report(1.00f, "Done");
    result.ok = true;
    return result;
}

} // namespace mt
