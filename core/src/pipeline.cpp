#include "mastertweak/pipeline.hpp"

#include "mastertweak/dsp/parametric_eq.hpp"
#include "mastertweak/dsp/multiband_comp.hpp"
#include "mastertweak/dsp/mixbus_comp.hpp"
#include "mastertweak/dsp/saturator.hpp"
#include "mastertweak/dsp/stereo_width.hpp"
#include "mastertweak/dsp/limiter.hpp"
#include "mastertweak/dsp/dither.hpp"
#include "mastertweak/dsp/lufs_analyser.hpp"

#include <algorithm>
#include <cmath>

namespace mt {

MasterResult analyseOnly(const std::string& inputPath,
                         const PresetData&  preset,
                         std::string*       errOut) {
    MasterResult result;

    auto audio = readAudioFile(inputPath, errOut);
    if (!audio) return result;

    result.analysis = analyseFile(*audio);
    result.advice   = deriveAdvice(result.analysis, preset);
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

    // ── Analyse ───────────────────────────────────────────────────────────────
    report(0.10f, "Analysing");
    result.analysis = analyseFile(*audio);

    // ── Derive advice ─────────────────────────────────────────────────────────
    report(0.20f, "Deriving advice");
    result.advice = adviceOverride ? *adviceOverride : deriveAdvice(result.analysis, preset);

    const AdviceSet& adv = result.advice;
    const float sr = static_cast<float>(audio->sampleRate);
    const int   nch = audio->numChannels;
    auto& buf = audio->samples;
    const int nf = audio->numFrames;

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
        report(0.75f, "Normalising to target");
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
        // Override the advice ceiling with the target's true-peak ceiling.
        result.advice.limiter.ceilingDb = opts.targetLevel->peakCeiling;
    }

    // ── Limiter ───────────────────────────────────────────────────────────────
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
