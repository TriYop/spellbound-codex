#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"
#include "mastertweak/target_level.hpp"
#include "mastertweak/version.hpp"

#include <CLI11.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string defaultOutputPath(const std::string& input, bool flac) {
    fs::path p{input};
    const std::string stem = p.stem().string() + "_master";
    return (p.parent_path() / (stem + (flac ? ".flac" : ".wav"))).string();
}

static void printAnalysis(const mt::MasterResult& result, const std::string& label) {
    const auto& snap = result.analysis;
    std::printf("=== Analysis: %s ===\n", label.c_str());
    std::printf("Overall: avg=%.1f dBFS  peak=%.1f dBFS  corr=%.3f\n",
                snap.overallAvgDb, snap.overallPeakDb, snap.overallCorr);
    for (int i = 0; i < mt::AnalysisSnapshot::kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const auto& b = snap.bands[si];
        std::printf("  %-8s  avg=%6.1f dBFS  peak=%6.1f dBFS  corr=%5.3f  crest=%4.1f dB\n",
                    mt::AnalysisSnapshot::kBandNames[si],
                    b.avgRmsDb, b.peakRmsDb, b.correlation, b.crestDb);
    }
    const auto& adv = result.advice;
    std::printf("\n=== Advice ===\n");
    for (int i = 0; i < mt::AdviceSet::kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const auto& eq = adv.eq[si];
        const auto& mb = adv.mbComp[si];
        std::printf("  %-8s  EQ=%+5.1f dB (Q=%.2f)  MB: thr=%.1f dB  ratio=%.1f:1\n",
                    mt::AnalysisSnapshot::kBandNames[si],
                    eq.gainDb, eq.q, mb.thresholdDb, mb.ratio);
    }
    std::printf("  Mixbus:  thr=%.1f dB  ratio=%.1f:1  makeup=%.1f dB\n",
                adv.mixbusComp.thresholdDb, adv.mixbusComp.ratio, adv.mixbusComp.makeupDb);
    std::printf("  Saturator drive: %.1f dB\n", adv.saturator.driveDb);
    std::printf("  Limiter ceiling: %.1f dBTP\n", adv.limiter.ceilingDb);
}

// Map lowercase band name → band index (0–6). Returns -1 on unknown.
static int bandIndex(const std::string& name) {
    static const std::pair<const char*, int> kMap[] = {
        {"sub",    0}, {"lows",   1}, {"lo-mid", 2}, {"lomid",  2},
        {"mids",   3}, {"hi-mid", 4}, {"himid",  4}, {"highs",  5}, {"air", 6},
    };
    std::string lo = name;
    std::transform(lo.begin(), lo.end(), lo.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    for (const auto& [k, v] : kMap)
        if (lo == k) return v;
    return -1;
}

// Apply "band:gain[,band:gain,...]" tokens to adv. Returns false on parse error.
static bool applyEqOverrides(mt::AdviceSet& adv,
                              const std::vector<std::string>& tokens) {
    for (const auto& tok : tokens) {
        // Each token may be a comma-separated list: "sub:-2,lows:3"
        std::string cur = tok;
        while (!cur.empty()) {
            std::string pair;
            const auto comma = cur.find(',');
            if (comma == std::string::npos) { pair = cur; cur.clear(); }
            else { pair = cur.substr(0, comma); cur = cur.substr(comma + 1); }

            const auto colon = pair.find(':');
            if (colon == std::string::npos) {
                std::fprintf(stderr, "warning: --override-eq: bad token '%s' (expected band:gain)\n",
                             pair.c_str());
                continue;
            }
            const std::string bandName = pair.substr(0, colon);
            const std::string gainStr  = pair.substr(colon + 1);
            const int idx = bandIndex(bandName);
            if (idx < 0) {
                std::fprintf(stderr, "warning: --override-eq: unknown band '%s'\n",
                             bandName.c_str());
                continue;
            }
            try {
                adv.eq[static_cast<size_t>(idx)].gainDb = std::stof(gainStr);
            } catch (...) {
                std::fprintf(stderr, "warning: --override-eq: bad gain value '%s'\n",
                             gainStr.c_str());
            }
        }
    }
    return true;
}

// Returns nullopt when no overrides apply; otherwise a copy of advice with
// the specified CLI overrides applied on top of auto-derived values.
static std::optional<mt::AdviceSet> buildAdviceOverride(
        const std::string& inputPath, const mt::PresetData& preset,
        float limCeiling, float satDrive,
        const std::vector<std::string>& eqTokens, bool verbose) {
    const bool hasAny = !std::isnan(limCeiling) || !std::isnan(satDrive)
                        || !eqTokens.empty();
    if (!hasAny) return std::nullopt;

    if (verbose) std::printf("Deriving advice for override…\n");
    std::string err;
    auto r = mt::analyseOnly(inputPath, preset, &err);
    if (!r.ok) {
        std::fprintf(stderr, "error: could not analyse for override: %s\n", err.c_str());
        return std::nullopt;
    }
    mt::AdviceSet adv = r.advice;
    if (!std::isnan(limCeiling)) adv.limiter.ceilingDb = limCeiling;
    if (!std::isnan(satDrive))   adv.saturator.driveDb = satDrive;
    applyEqOverrides(adv, eqTokens);
    return adv;
}

static mt::RenderOptions buildRenderOpts(int bitDepth, bool flac,
                                          const std::vector<std::string>& bypasses) {
    mt::RenderOptions opts;
    opts.outputBitDepth = bitDepth;
    opts.outputFlac     = flac;
    for (const auto& s : bypasses) {
        if      (s == "eq")         opts.bypassEq         = true;
        else if (s == "resonance")  opts.bypassResonanceEq = true;
        else if (s == "multiband")  opts.bypassMbComp     = true;
        else if (s == "saturator")  opts.bypassSaturator  = true;
        else if (s == "width")      opts.bypassWidth      = true;
        else if (s == "mixbuscomp") opts.bypassMixbusComp = true;
        else if (s == "limiter")    opts.bypassLimiter    = true;
        else if (s == "dither")     opts.bypassDither     = true;
        else std::fprintf(stderr, "warning: unknown bypass stage '%s'\n", s.c_str());
    }
    return opts;
}

int main(int argc, char** argv) {
    CLI::App app{"mastertweak — offline auto-mastering driven by MixAdvice presets"};
    app.set_version_flag("--version", std::string{mastertweak::version()});

    // ── Single-file positional ─────────────────────────────────────────────────
    std::string inputPath;
    app.add_option("input", inputPath, "Input audio file (WAV / FLAC / AIFF)")
       ->check(CLI::ExistingFile);

    // ── Core options ─────────────────────────────────────────────────────────
    std::string presetArg;
    app.add_option("--preset,-p", presetArg, "Preset name or XML path")->required();

    std::string outputPath;
    app.add_option("--output,-o", outputPath, "Output file (default: <input>_master.wav)");

    int bitDepth = 24;
    app.add_option("--bit-depth", bitDepth, "Output bit depth: 16, 24, or 32 (default: 24)")
       ->check(CLI::IsMember({16, 24, 32}));

    std::string outputFormat = "wav";
    app.add_option("--output-format", outputFormat, "Output format: wav or flac (default: wav)")
       ->check(CLI::IsMember({"wav", "flac"}));

    // ── Bypass flags ─────────────────────────────────────────────────────────
    std::vector<std::string> bypasses;
    app.add_option("--bypass", bypasses,
                   "Stages to bypass (repeat or space-separate): "
                   "eq resonance multiband saturator width mixbuscomp limiter dither");

    // ── Target level ─────────────────────────────────────────────────────────
    std::string targetLevelName;
    app.add_option("--target-level", targetLevelName,
                   "Set output loudness target by platform name (case-insensitive).\n"
                   "  e.g. --target-level spotify\n"
                   "  Use --list-targets to see all valid names.");

    app.add_flag_callback("--list-targets",
                          [&]() {
                              std::printf("Built-in target levels:\n");
                              for (const auto& p : mt::kTargetLevelProfiles)
                                  std::printf("  %-24s  %5.1f LUFS / %4.1f dBTP\n",
                                              p.name.c_str(), p.lufs, p.peakCeiling);
                              std::exit(0);
                          },
                          "Print built-in target levels and exit.");

    // ── Advice overrides ──────────────────────────────────────────────────────
    const float kNoOverride = std::numeric_limits<float>::quiet_NaN();
    float overrideLimCeiling = kNoOverride;
    float overrideSatDrive   = kNoOverride;
    std::vector<std::string> overrideEq;
    app.add_option("--override-limiter-ceiling", overrideLimCeiling,
                   "Override limiter true-peak ceiling (dBTP, e.g. -2.0)");
    app.add_option("--override-sat-drive", overrideSatDrive,
                   "Override saturator drive (dB, 0–6)");
    app.add_option("--override-eq", overrideEq,
                   "Override per-band EQ gain(s): band:dB[,band:dB,...]\n"
                   "  bands: sub lows lo-mid mids hi-mid highs air\n"
                   "  e.g. --override-eq lows:-3,highs:+2");

    // ── Batch mode ────────────────────────────────────────────────────────────
    std::string batchGlob;
    std::string outputDir;
    app.add_option("--batch", batchGlob,
                   "Process all matching files (glob, e.g. '*.wav'). "
                   "Use with --output-dir.");
    app.add_option("--output-dir", outputDir,
                   "Destination directory for batch output (default: same as input)");

    // ── Modes ─────────────────────────────────────────────────────────────────
    bool analyseOnly = false;
    app.add_flag("--analyze-only,--analyse-only", analyseOnly,
                 "Print analysis + advice, do not render");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Print processing details");

    CLI11_PARSE(app, argc, argv);

    // ── Resolve preset ────────────────────────────────────────────────────────
    const std::string execDir = fs::path{argv[0]}.parent_path().string();
    auto preset = mt::resolvePreset(presetArg, execDir);
    if (!preset) {
        std::fprintf(stderr, "error: preset '%s' not found\n", presetArg.c_str());
        std::fprintf(stderr, "  searched: <exec>/presets/, ~/.config/MixAdvice/Presets/\n");
        return 1;
    }
    if (verbose)
        std::printf("Preset: %s\n", preset->name.c_str());

    mt::RenderOptions renderOpts = buildRenderOpts(bitDepth, outputFormat == "flac", bypasses);
    const bool useFlac = renderOpts.outputFlac;

    // ── Resolve --target-level ────────────────────────────────────────────────
    if (!targetLevelName.empty()) {
        const auto* tp = mt::findTargetLevel(targetLevelName);
        if (!tp) {
            std::fprintf(stderr, "error: unknown target level '%s'.\n",
                         targetLevelName.c_str());
            std::fprintf(stderr, "Built-in target levels:\n");
            for (const auto& p : mt::kTargetLevelProfiles)
                std::fprintf(stderr, "  %-24s  %5.1f LUFS / %4.1f dBTP\n",
                             p.name.c_str(), p.lufs, p.peakCeiling);
            return 1;
        }
        renderOpts.targetLevel = *tp;
    }

    mt::ProgressCallback progressCb;
    if (verbose) {
        progressCb = [](float frac, const std::string& stage) {
            std::printf("  [%3.0f%%] %s\n", frac * 100.f, stage.c_str());
            std::fflush(stdout);
        };
    }

    // ── Batch mode ────────────────────────────────────────────────────────────
    if (!batchGlob.empty()) {
        // Resolve glob: find parent dir and filename pattern
        const fs::path globPath{batchGlob};
        const fs::path searchDir = globPath.has_parent_path()
            ? globPath.parent_path()
            : fs::current_path();
        const std::string pattern = globPath.filename().string();

        // Simple wildcard match: '*' matches any sequence of characters
        auto wildcardMatch = [](const std::string& name, const std::string& pat) -> bool {
            size_t ni = 0, pi = 0, starPi = std::string::npos, starNi = 0;
            while (ni < name.size()) {
                if (pi < pat.size() && (pat[pi] == '?' || pat[pi] == name[ni])) {
                    ++ni; ++pi;
                } else if (pi < pat.size() && pat[pi] == '*') {
                    starPi = pi++;
                    starNi = ni;
                } else if (starPi != std::string::npos) {
                    pi = starPi + 1;
                    ni = ++starNi;
                } else {
                    return false;
                }
            }
            while (pi < pat.size() && pat[pi] == '*') ++pi;
            return pi == pat.size();
        };

        std::vector<std::string> inputs;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(searchDir, ec)) {
            if (!entry.is_regular_file()) continue;
            const std::string fname = entry.path().filename().string();
            if (wildcardMatch(fname, pattern))
                inputs.push_back(entry.path().string());
        }

        if (inputs.empty()) {
            std::fprintf(stderr, "error: no files matched '%s'\n", batchGlob.c_str());
            return 1;
        }

        const fs::path outDir = outputDir.empty() ? "" : fs::path{outputDir};
        if (!outputDir.empty()) {
            std::error_code mkec;
            fs::create_directories(outDir, mkec);
        }

        int failed = 0;
        for (const auto& in : inputs) {
            const std::string out = outputDir.empty()
                ? defaultOutputPath(in, useFlac)
                : (outDir / (fs::path{in}.stem().string() + "_master"
                             + (useFlac ? ".flac" : ".wav"))).string();

            if (verbose)
                std::printf("Rendering: %s → %s\n", in.c_str(), out.c_str());
            else
                std::printf("%s\n", in.c_str());

            auto advOpt = buildAdviceOverride(in, *preset, overrideLimCeiling,
                                              overrideSatDrive, overrideEq, verbose);
            std::string err;
            auto result = mt::renderFile(in, out, *preset, renderOpts,
                                          advOpt ? &*advOpt : nullptr, progressCb, &err);
            if (!result.ok) {
                std::fprintf(stderr, "  error: %s\n", err.c_str());
                ++failed;
            } else {
                std::printf("  → %s\n", out.c_str());
            }
        }

        std::printf("\nBatch complete: %zu files, %d failed.\n", inputs.size(), failed);
        return failed > 0 ? 1 : 0;
    }

    // ── Single-file mode ──────────────────────────────────────────────────────
    if (inputPath.empty()) {
        std::fprintf(stderr, "error: provide an input file or use --batch\n");
        std::fprintf(stderr, "Run with --help for usage.\n");
        return 1;
    }

    // ── Analyse-only mode ─────────────────────────────────────────────────────
    if (analyseOnly) {
        std::string err;
        auto result = mt::analyseOnly(inputPath, *preset, &err);
        if (!result.ok) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
        printAnalysis(result, inputPath);
        return 0;
    }

    // ── Render ────────────────────────────────────────────────────────────────
    if (outputPath.empty())
        outputPath = defaultOutputPath(inputPath, useFlac);

    if (verbose)
        std::printf("Rendering: %s → %s\n", inputPath.c_str(), outputPath.c_str());

    auto advOpt = buildAdviceOverride(inputPath, *preset, overrideLimCeiling,
                                      overrideSatDrive, overrideEq, verbose);
    std::string err;
    auto result = mt::renderFile(inputPath, outputPath, *preset,
                                 renderOpts, advOpt ? &*advOpt : nullptr, progressCb, &err);
    if (!result.ok) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }

    std::printf("Output: %s\n", outputPath.c_str());
    return 0;
}
