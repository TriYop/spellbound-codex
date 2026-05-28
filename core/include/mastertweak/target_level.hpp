#pragma once

#include <array>
#include <string>
#include <string_view>

namespace mt {

struct TargetLevelProfile {
    std::string name;
    float lufs;         // integrated loudness target (LUFS)
    float peakCeiling;  // true-peak ceiling (dBTP)
};

extern const std::array<TargetLevelProfile, 8> kTargetLevelProfiles;

// Case-insensitive lookup. Returns nullptr if not found.
const TargetLevelProfile* findTargetLevel(std::string_view name);

} // namespace mt
