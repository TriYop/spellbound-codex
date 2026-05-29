#pragma once

#include "preset_builder/adapters/database.hpp"
#include "preset_builder/ports/preset_repository.hpp"

namespace pb {

class SqlitePresetRepository : public PresetRepository {
public:
    explicit SqlitePresetRepository(Database& db);

    std::optional<Preset> find(const PresetId& id) const override;
    std::vector<Preset>   listAll() const override;
    std::vector<Track>    tracksFor(const PresetId& id) const override;
    void                  save(const Preset& preset) override;
    void                  remove(const PresetId& id) override;

private:
    Database& db_;
};

} // namespace pb
