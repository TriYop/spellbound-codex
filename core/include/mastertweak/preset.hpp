#pragma once

#include "mastertweak/analysis.hpp"  // for kNumBands

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace mt {

struct PresetData {
    std::string name;
    std::string description;

    // Band order: Sub, Lows, Lo-Mid, Mids, Hi-Mid, Highs, Air  (matches MixAdvice)
    static constexpr int kNumBands = AnalysisSnapshot::kNumBands;

    std::array<float, kNumBands> bandRmsDb       {};   // target per-band RMS (dBFS)
    std::array<float, kNumBands> bandMinCorr      {};   // minimum acceptable L/R correlation
    std::array<float, kNumBands> bandTransientDb  {};   // expected crest factor (dB)
    float overallRmsDb   = -18.f;
    float overallMinCorr =   0.6f;
};

// Load a single preset from an XML file (MixAdvice schema).
std::optional<PresetData> loadPreset(const std::string& xmlPath,
                                      std::string* errOut = nullptr);

// Scan a directory for *.xml presets, return all successfully parsed ones.
std::vector<PresetData> loadPresetsFromDir(const std::string& dirPath);

// Resolve a preset by name or path:
//   1. If name contains '/' or ends in '.xml' → treat as literal path
//   2. Search executableDir/presets/ (bundled)
//   3. Search ~/.config/MixAdvice/Presets/   (user / preset-builder output)
// Returns nullopt if not found.
std::optional<PresetData> resolvePreset(const std::string& nameOrPath,
                                         const std::string& executableDir = "");

// Enumerate all available preset names (from bundled + user dirs, deduplicated).
std::vector<PresetData> enumeratePresets(const std::string& executableDir = "");

} // namespace mt
