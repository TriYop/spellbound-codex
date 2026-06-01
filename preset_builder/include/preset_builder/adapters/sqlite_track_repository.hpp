#pragma once

#include "preset_builder/adapters/database.hpp"
#include "preset_builder/ports/track_repository.hpp"

namespace pb {

class SqliteTrackRepository : public TrackRepository {
public:
    explicit SqliteTrackRepository(Database& db);

    std::optional<Track> find(const TrackId& id) const override;
    std::optional<Track> findByPath(const std::string& path) const override;
    std::vector<Track>   search(const TrackFilter& filter) const override;
    bool                 existsByBasenameAndSize(const std::string& basename,
                                                 int64_t size) const override;
    void                 save(const Track& track) override;
    void                 remove(const TrackId& id) override;

private:
    Database& db_;
};

} // namespace pb
