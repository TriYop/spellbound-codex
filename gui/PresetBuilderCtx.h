#pragma once

#include "AcoustIdMetadataProvider.h"
#include "preset_builder/adapters/database.hpp"
#include "preset_builder/adapters/sqlite_preset_repository.hpp"
#include "preset_builder/adapters/sqlite_track_repository.hpp"

#include <string>

namespace gui {

// Plain aggregate owning all preset-builder infrastructure.
// Members are initialised in declaration order: db first, then the repositories that reference it.
struct PresetBuilderCtx {
    pb::Database               db;
    pb::SqliteTrackRepository  tracks;
    pb::SqlitePresetRepository presets;
    AcoustIdMetadataProvider   metadata;

    explicit PresetBuilderCtx(const std::string& dbPath)
        : db(dbPath), tracks(db), presets(db) {}
};

} // namespace gui
