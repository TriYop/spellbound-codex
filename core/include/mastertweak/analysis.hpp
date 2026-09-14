#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "audioplugins/common/analysis/BandConfig.h"

namespace mt {

struct AudioFile;  // forward declare — analysis.hpp doesn't need io.hpp internals

struct BandStats {
    float avgRmsDb    = -100.f;  // long-term RMS over the whole file (dBFS)
    float peakRmsDb   = -100.f;  // peak-hold of the 100ms-smoothed RMS (dBFS)
    float p10RmsDb    = -100.f;  // 10th-percentile of per-block RMS (dBFS)
    float p50RmsDb    = -100.f;  // upper-median of per-block RMS (dBFS); floor(0.5·N) index
    float p95RmsDb    = -100.f;  // 95th-percentile of per-block RMS (dBFS); raw blocks, cf. peakRmsDb which uses 100ms-smoothed RMS
    float correlation =    1.f;  // integrated Pearson L/R correlation [-1, 1]
    float crestDb     =    0.f;  // average crest factor (peak/RMS) in dB
};

struct AnalysisSnapshot {
    static constexpr int kNumBands = audioplugins::common::analysis::BandConfig::numBands;
    static constexpr auto kCrossoverHz  = audioplugins::common::analysis::BandConfig::crossoverHz;
    static constexpr auto kBandNames    = audioplugins::common::analysis::BandConfig::bandNames;
    static constexpr auto kBandIsShelf  = audioplugins::common::analysis::BandConfig::bandIsShelf;
    static constexpr auto kBandCenterHz = audioplugins::common::analysis::BandConfig::bandCenterHz;

    std::array<BandStats, kNumBands> bands{};
    float overallAvgDb  = -100.f;  // broadband L+R average long-term RMS
    float overallPeakDb = -100.f;  // broadband peak-hold smoothed RMS
    float overallCorr   =    1.f;  // broadband integrated Pearson L/R
    float lraLu         =    0.f;  // EBU R128 Loudness Range (LU); 0 = unknown/silence
};

// Analyse a whole audio file offline.
// If the file is mono it is treated as dual-mono (correlation = 1 everywhere).
AnalysisSnapshot analyseFile(const AudioFile& audio);

struct ResonancePeak {
    float freqHz  = 0.f;
    float q       = 1.f;
    float gainDb  = 0.f;   // negative = attenuation
    bool  enabled = true;
};

// Detects narrow resonances in the full audio signal.
// Returns at most kMaxResonances peaks sorted by prominence (highest first).
std::vector<ResonancePeak> detectResonances(const AudioFile& audio);

} // namespace mt
