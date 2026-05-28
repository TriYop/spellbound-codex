#pragma once

#include <array>
#include <string>
#include <string_view>

namespace mt {

struct TargetLevelProfile {
    std::string name;
    float lufs        = 0.f;  // integrated loudness target (LUFS)
    float peakCeiling = 0.f;  // true-peak ceiling (dBTP)
};

extern const std::array<TargetLevelProfile, 8> kTargetLevelProfiles;

// Case-insensitive lookup. Returns nullptr if not found.
const TargetLevelProfile* findTargetLevel(std::string_view name);

} // namespace mt
