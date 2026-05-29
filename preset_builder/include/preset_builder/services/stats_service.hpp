#pragma once

#include "preset_builder/domain/preset.hpp"
#include "preset_builder/domain/track.hpp"

#include <vector>

namespace pb {

class StatsService {
public:
    // Returns zero-filled PresetStats for an empty collection.
    PresetStats compute(const std::vector<Track>& tracks) const;
};

} // namespace pb
