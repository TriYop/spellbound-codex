#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace mt {

struct AudioFile;  // forward declare — analysis.hpp doesn't need io.hpp internals

struct BandStats {
    float avgRmsDb    = -100.f;  // long-term RMS over the whole file (dBFS)
    float peakRmsDb   = -100.f;  // peak-hold of the 100ms-smoothed RMS (dBFS)
    float correlation =    1.f;  // integrated Pearson L/R correlation [-1, 1]
    float crestDb     =    0.f;  // average crest factor (peak/RMS) in dB
};

struct AnalysisSnapshot {
    static constexpr int kNumBands = 7;
    static constexpr std::array<float, kNumBands - 1> kCrossoverHz = {
        80.f, 250.f, 500.f, 2000.f, 6000.f, 16000.f
    };
    static constexpr std::array<const char*, kNumBands> kBandNames = {
        "Sub", "Lows", "Lo-Mid", "Mids", "Hi-Mid", "Highs", "Air"
    };
    // true = this band should be advised as a shelf filter, not a bell
    static constexpr std::array<bool, kNumBands> kBandIsShelf = {
        true, false, false, false, false, false, true
    };
    // Representative EQ frequencies per band (matches MixAdvice BandConfig)
    static constexpr std::array<float, kNumBands> kBandCenterHz = {
        50.f, 160.f, 375.f, 1000.f, 3500.f, 10000.f, 16000.f
    };

    std::array<BandStats, kNumBands> bands{};
    float overallAvgDb  = -100.f;  // broadband L+R average long-term RMS
    float overallPeakDb = -100.f;  // broadband peak-hold smoothed RMS
    float overallCorr   =    1.f;  // broadband integrated Pearson L/R
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
