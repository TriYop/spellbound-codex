#pragma once

#include "preset_builder/domain/track.hpp"

#include <array>
#include <string>
#include <vector>

namespace pb {

struct PresetId {
    std::string uuid;
    bool operator==(const PresetId&) const = default;
};

// Statistics computed on demand from a Preset's Track collection — never persisted.
struct PresetStats {
    std::array<float, 7> bandRmsDb{};       // mean of bandRmsDb across tracks
    std::array<float, 7> bandCorrMin{};     // p10  of bandCorr   across tracks
    std::array<float, 7> bandTransientDb{}; // median of bandTransientDb across tracks
    float                overallRmsDb   = 0.f;  // mean   of overallRmsDb
    float                overallCorrMin = 0.f;  // p10    of overallCorr
};

struct Preset {
    PresetId             id;
    std::string          name;
    std::string          description;
    std::vector<TrackId> trackIds;
    std::string          createdAt;  // ISO 8601 (UTC)
};

} // namespace pb
