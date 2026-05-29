#include "preset_builder/adapters/sqlite_preset_repository.hpp"

#include <sqlite3.h>
#include <sstream>

namespace pb {

SqlitePresetRepository::SqlitePresetRepository(Database& db) : db_(db) {}

static Preset stmtToPreset(sqlite3_stmt* st) {
    Preset p;
    p.id.uuid     = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    p.name        = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    p.description = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
    p.createdAt   = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
    return p;
}

static void loadTrackIds(sqlite3* db, Preset& p) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT track_id FROM preset_tracks WHERE preset_id = ?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, p.id.uuid.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        TrackId tid;
        tid.hash = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        p.trackIds.push_back(tid);
    }
    sqlite3_finalize(st);
}

std::optional<Preset> SqlitePresetRepository::find(const PresetId& id) const {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "SELECT id, name, description, created_at FROM presets WHERE id = ?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.uuid.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<Preset> result;
    if (sqlite3_step(st) == SQLITE_ROW) {
        result = stmtToPreset(st);
        loadTrackIds(db_.handle(), *result);
    }
    sqlite3_finalize(st);
    return result;
}

std::vector<Preset> SqlitePresetRepository::listAll() const {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "SELECT id, name, description, created_at FROM presets;",
        -1, &st, nullptr);

    std::vector<Preset> results;
    while (sqlite3_step(st) == SQLITE_ROW) {
        auto p = stmtToPreset(st);
        loadTrackIds(db_.handle(), p);
        results.push_back(std::move(p));
    }
    sqlite3_finalize(st);
    return results;
}

std::vector<Track> SqlitePresetRepository::tracksFor(const PresetId& id) const {
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT t.id, t.path, t.added_at,"
        "       m.title, m.artist, m.album, m.genre, m.year, m.source,"
        "       a.band_rms_db, a.band_corr, a.band_transient_db,"
        "       a.overall_rms_db, a.overall_corr"
        "  FROM tracks t"
        "  INNER JOIN preset_tracks pt ON pt.track_id = t.id"
        "  LEFT JOIN track_metadata m  ON m.track_id  = t.id"
        "  LEFT JOIN track_analysis a  ON a.track_id  = t.id"
        "  WHERE pt.preset_id = ?;";

    sqlite3_prepare_v2(db_.handle(), sql, -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.uuid.c_str(), -1, SQLITE_TRANSIENT);

    auto jsonToArr = [](const std::string& s) -> std::array<float, 7> {
        std::array<float, 7> a{};
        if (s.size() < 2) return a;
        std::string inner = s.substr(1, s.size() - 2);
        std::istringstream iss(inner);
        std::string tok;
        int i = 0;
        while (std::getline(iss, tok, ',') && i < 7)
            a[static_cast<size_t>(i++)] = std::stof(tok);
        return a;
    };

    std::vector<Track> tracks;
    while (sqlite3_step(st) == SQLITE_ROW) {
        Track t;
        t.id.hash = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        t.path    = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        t.addedAt = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));

        auto col = [&](int i) -> std::string {
            const unsigned char* v = sqlite3_column_text(st, i);
            return v ? reinterpret_cast<const char*>(v) : "";
        };
        auto optCol = [&](int i) -> std::optional<std::string> {
            const unsigned char* v = sqlite3_column_text(st, i);
            if (!v || *v == '\0') return std::nullopt;
            return std::string(reinterpret_cast<const char*>(v));
        };

        t.metadata.title  = optCol(3);
        t.metadata.artist = optCol(4);
        t.metadata.album  = optCol(5);
        t.metadata.genre  = optCol(6);
        if (sqlite3_column_type(st, 7) != SQLITE_NULL)
            t.metadata.year = sqlite3_column_int(st, 7);
        t.metadata.source = col(8) == "acoustid"
            ? MetadataSource::acoustid : MetadataSource::filename;

        t.analysis.bandRmsDb       = jsonToArr(col(9));
        t.analysis.bandCorr        = jsonToArr(col(10));
        t.analysis.bandTransientDb = jsonToArr(col(11));
        t.analysis.overallRmsDb    = static_cast<float>(sqlite3_column_double(st, 12));
        t.analysis.overallCorr     = static_cast<float>(sqlite3_column_double(st, 13));

        tracks.push_back(t);
    }
    sqlite3_finalize(st);
    return tracks;
}

void SqlitePresetRepository::save(const Preset& preset) {
    sqlite3* db = db_.handle();
    sqlite3_stmt* st = nullptr;

    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO presets(id, name, description, created_at) VALUES(?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, preset.id.uuid.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, preset.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, preset.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, preset.createdAt.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    // Replace track associations
    sqlite3_prepare_v2(db,
        "DELETE FROM preset_tracks WHERE preset_id = ?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, preset.id.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    for (const auto& tid : preset.trackIds) {
        sqlite3_prepare_v2(db,
            "INSERT OR IGNORE INTO preset_tracks(preset_id, track_id) VALUES(?,?);",
            -1, &st, nullptr);
        sqlite3_bind_text(st, 1, preset.id.uuid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, tid.hash.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

void SqlitePresetRepository::remove(const PresetId& id) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "DELETE FROM presets WHERE id = ?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

} // namespace pb
