#pragma once

#include "preset_builder/domain/track.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace pb {

class TrackRepository {
public:
    virtual ~TrackRepository() = default;

    virtual std::optional<Track> find(const TrackId& id) const = 0;
    virtual std::optional<Track> findByPath(const std::string& path) const = 0;

    // Returns all tracks matching the filter (AND of provided fields).
    // An empty TrackFilter returns all tracks.
    virtual std::vector<Track>   search(const TrackFilter& filter) const = 0;

    // True if any stored track has the given filename (basename only, no
    // directory) and byte size. Used as a cheap pre-hash duplicate check.
    virtual bool existsByBasenameAndSize(const std::string& basename,
                                         int64_t            size) const = 0;

    virtual void save(const Track& track) = 0;       // insert or update
    virtual void remove(const TrackId& id) = 0;
};

} // namespace pb
