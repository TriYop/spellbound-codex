#pragma once

#include "preset_builder/domain/preset.hpp"
#include "preset_builder/domain/track.hpp"

#include <optional>
#include <vector>

namespace pb {

class PresetRepository {
public:
    virtual ~PresetRepository() = default;

    virtual std::optional<Preset> find(const PresetId& id) const = 0;
    virtual std::vector<Preset>   listAll() const = 0;

    // JOIN across preset_tracks + tracks — returns full Track objects.
    virtual std::vector<Track>    tracksFor(const PresetId& id) const = 0;

    virtual void save(const Preset& preset) = 0;     // insert or update
    virtual void remove(const PresetId& id) = 0;
};

} // namespace pb
