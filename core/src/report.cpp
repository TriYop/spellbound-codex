#include "mastertweak/report.hpp"

#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace mt {

namespace {

std::string today() {
    const auto now = std::chrono::system_clock::now();
    const auto dp  = std::chrono::floor<std::chrono::days>(now);
    const std::chrono::year_month_day ymd{dp};
    std::ostringstream oss;
    oss << std::setfill('0')
        << static_cast<int>(ymd.year())            << '-'
        << std::setw(2) << static_cast<unsigned>(ymd.month()) << '-'
        << std::setw(2) << static_cast<unsigned>(ymd.day());
    return oss.str();
}

// Format a float with explicit +/- sign (for gain/threshold values).
std::string fmtDb(float v, int prec = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec);
    if (v >= 0.f && !std::signbit(v)) oss << '+';
    oss << v;
    return oss.str();
}

// Format a plain float (no sign prefix).
std::string fmtF(float v, int prec = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

} // namespace

std::string formatAdviceMarkdown(
    const AnalysisSnapshot& snap,
    const AdviceSet&        advice,
    const PresetData&       preset,
    const std::string&      inputFilename)
{
    std::ostringstream md;

    // ── Header ────────────────────────────────────────────────────────────────
    md << "# MasterTweak Advice Export\n\n";
    md << "**File:** " << inputFilename << "  \n";
    md << "**Preset:** " << preset.name << "  \n";
    md << "**Date:** " << today() << "\n\n";
    if (!preset.description.empty())
        md << "> " << preset.description << "\n\n";

    // ── Analysis ──────────────────────────────────────────────────────────────
    md << "## Analysis — Measured Values\n\n";
    md << "| Band    | Avg (dBFS) | P10 (dBFS) | P50 (dBFS) | P95 (dBFS) | Crest (dB) | L/R Corr |\n";
    md << "| ------- | ---------- | ---------- | ---------- | ---------- | ---------- | -------- |\n";
    for (int i = 0; i < AnalysisSnapshot::kNumBands; ++i) {
        const auto& b = snap.bands[i];
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(10) << fmtDb(b.avgRmsDb)
           << " | " << std::setw(10) << fmtDb(b.p10RmsDb)
           << " | " << std::setw(10) << fmtDb(b.p50RmsDb)
           << " | " << std::setw(10) << fmtDb(b.p95RmsDb)
           << " | " << std::setw(10) << fmtF(b.crestDb)
           << " | " << std::setw(8)  << fmtF(b.correlation, 2) << " |\n";
    }
    md << "\n**Overall:** RMS " << fmtDb(snap.overallAvgDb)
       << " dBFS · Correlation " << fmtF(snap.overallCorr, 2) << "\n\n";

    // ── Resonance EQ ─────────────────────────────────────────────────────────
    md << "## Resonance EQ\n\n";
    if (advice.resonances.empty()) {
        md << "_No resonances detected._\n\n";
    } else {
        md << "| #  | Freq (Hz) | Q    | Gain (dB) | Active |\n";
        md << "| -- | --------- | ---- | --------- | ------ |\n";
        for (int i = 0; i < static_cast<int>(advice.resonances.size()); ++i) {
            const auto& peak = advice.resonances[i];
            md << "| " << std::right << std::setw(2) << (i + 1)
               << " | " << std::setw(9) << fmtF(peak.freqHz, 0)
               << " | " << std::setw(4) << fmtF(peak.q, 1)
               << " | " << std::setw(9) << fmtDb(peak.gainDb)
               << " | " << std::left  << std::setw(6) << (peak.enabled ? "yes" : "no") << " |\n";
        }
        md << "\n";
    }

    // ── EQ ────────────────────────────────────────────────────────────────────
    md << "## 7-Band EQ\n\n";
    md << "| Band    | Freq (Hz) | Gain (dB) | Q    | Type  |\n";
    md << "| ------- | --------- | --------- | ---- | ----- |\n";
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto& eq = advice.eq[i];
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(9) << fmtF(eq.freqHz, 0)
           << " | " << std::setw(9) << fmtDb(eq.gainDb)
           << " | " << std::setw(4) << fmtF(eq.q, 2)
           << " | " << std::left  << std::setw(5) << (eq.isShelf ? "Shelf" : "Bell") << " |\n";
    }
    md << "\n";

    // ── Multiband comp ────────────────────────────────────────────────────────
    md << "## Multiband Compression\n\n";
    md << "| Band    | Threshold (dB) | Ratio | Attack (ms) | Release (ms) |\n";
    md << "| ------- | -------------- | ----- | ----------- | ------------ |\n";
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto& c = advice.mbComp[i];
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(14) << fmtDb(c.thresholdDb)
           << " | " << std::setw(5)  << fmtF(c.ratio, 1)
           << " | " << std::setw(11) << fmtF(c.attackMs, 0)
           << " | " << std::setw(12) << fmtF(c.releaseMs, 0) << " |\n";
    }
    md << "\n";

    // ── Stereo width ──────────────────────────────────────────────────────────
    md << "## Stereo Width\n\n";
    md << "| Band    | Width |\n";
    md << "| ------- | ----- |\n";
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(5) << fmtF(advice.width[i].width, 2) << " |\n";
    }
    md << "\n";

    // ── Mixbus comp ───────────────────────────────────────────────────────────
    const auto& mb = advice.mixbusComp;
    md << "## Mixbus Compressor\n\n";
    md << "| Threshold (dB) | Ratio | Attack (ms) | Release (ms) | Makeup (dB) |\n";
    md << "| -------------- | ----- | ----------- | ------------ | ----------- |\n";
    md << "| " << std::right << std::setw(14) << fmtDb(mb.thresholdDb)
       << " | " << std::setw(5)  << fmtF(mb.ratio, 1)
       << " | " << std::setw(11) << fmtF(mb.attackMs, 0)
       << " | " << std::setw(12) << fmtF(mb.releaseMs, 0)
       << " | " << std::setw(11) << fmtDb(mb.makeupDb) << " |\n\n";

    // ── Saturator ─────────────────────────────────────────────────────────────
    md << "## Saturation\n\n";
    md << "**Drive:** " << fmtF(advice.saturator.driveDb) << " dB\n\n";

    // ── Limiter ───────────────────────────────────────────────────────────────
    md << "## Limiter\n\n";
    md << "**Target:** " << fmtF(advice.limiter.targetLufsApprox) << " LUFS"
       << " · **True-Peak Ceiling:** " << fmtDb(advice.limiter.ceilingDb) << " dBTP\n";

    return md.str();
}

} // namespace mt
