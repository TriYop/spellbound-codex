#pragma once

#include "mastertweak/analysis.hpp"
#include "mastertweak/preset.hpp"

#include <array>

namespace mt {

// Per-band EQ correction (one entry per AnalysisSnapshot band)
struct BandEq {
    float gainDb = 0.f;   // 0 = no correction
    float q      = 1.f;
    bool  isShelf = false; // true for Sub and Air bands
    float freqHz  = 0.f;  // characteristic EQ frequency for this band
};

// Per-band multiband compressor settings
struct BandComp {
    float thresholdDb = -20.f;
    float ratio       =   1.f;  // 1:1 = no compression
    float attackMs    =  10.f;
    float releaseMs   = 100.f;
};

// Mixbus (broadband) compressor settings
struct MixbusComp {
    float thresholdDb = -20.f;
    float ratio       =   2.f;
    float attackMs    =  15.f;
    float releaseMs   = 100.f;
    float makeupDb    =   0.f;
};

// Saturation drive derived from crest-factor deficit
struct SaturatorParams {
    float driveDb = 0.f;  // [0, 6] dB
};

// Per-band stereo width adjustment
struct BandWidth {
    float width = 1.f;  // 0 = full mono, 1 = no change, >1 = wider
};

// Limiter target
struct LimiterParams {
    float targetLufsApprox = -14.f;  // approximate LUFS target
    float ceilingDb        =  -1.f;  // true-peak ceiling (dBTP)
};

// Full advice set derived from AnalysisSnapshot + PresetData.
// All values can be overridden by the user before rendering.
struct AdviceSet {
    static constexpr int kNumBands = AnalysisSnapshot::kNumBands;

    std::array<BandEq,    kNumBands> eq      {};
    std::array<BandComp,  kNumBands> mbComp  {};
    std::array<BandWidth, kNumBands> width   {};
    MixbusComp  mixbusComp  {};
    SaturatorParams saturator {};
    LimiterParams   limiter   {};
};

// Derive an AdviceSet from analysis + preset.
// This is a pure function — no side effects, no state.
// Algorithm matches MixAdvice/Source/PluginEditor.cpp:530-665.
AdviceSet deriveAdvice(const AnalysisSnapshot& snap, const PresetData& preset);

} // namespace mt
