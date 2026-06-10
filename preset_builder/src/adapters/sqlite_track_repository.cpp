#include "preset_builder/adapters/sqlite_track_repository.hpp"

#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace pb {

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string floatArrayToJson(const std::array<float, 7>& a) {
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < 7; ++i) {
        if (i > 0) oss << ",";
        oss << a[static_cast<size_t>(i)];
    }
    oss << "]";
    return oss.str();
}

static std::array<float, 7> jsonToFloatArray(const std::string& s) {
    std::array<float, 7> a{};
    if (s.size() < 2) return a;
    std::string inner = s.substr(1, s.size() - 2);
    std::istringstream iss(inner);
    std::string tok;
    int i = 0;
    while (std::getline(iss, tok, ',') && i < 7)
        a[static_cast<size_t>(i++)] = std::stof(tok);
    return a;
}

static std::string optStr(const std::optional<std::string>& o) {
    return o ? *o : "";
}

static std::string sourceStr(MetadataSource s) {
    if (s == MetadataSource::acoustid)     return "acoustid";
    if (s == MetadataSource::embedded_tags) return "embedded_tags";
    return "filename";
}

static MetadataSource sourceFrom(const std::string& s) {
    if (s == "acoustid")      return MetadataSource::acoustid;
    if (s == "embedded_tags") return MetadataSource::embedded_tags;
    return MetadataSource::filename;
}

// ── row builder ───────────────────────────────────────────────────────────────

static const char* kSelectJoin =
    "SELECT t.id, t.path, t.file_size, t.added_at,"
    "       m.title, m.artist, m.album, m.genre, m.year, m.source,"
    "       a.band_rms_db, a.band_corr, a.band_transient_db,"
    "       a.overall_rms_db, a.overall_corr, a.lra"
    "  FROM tracks t"
    "  LEFT JOIN track_metadata m ON m.track_id = t.id"
    "  LEFT JOIN track_analysis a ON a.track_id = t.id";

static Track stmtToTrack(sqlite3_stmt* st) {
    Track t;
    t.id.hash  = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    t.path     = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    t.fileSize = sqlite3_column_int64(st, 2);
    t.addedAt  = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));

    auto optCol = [&](int i) -> std::optional<std::string> {
        const unsigned char* v = sqlite3_column_text(st, i);
        if (!v || *v == '\0') return std::nullopt;
        return std::string(reinterpret_cast<const char*>(v));
    };
    auto col = [&](int i) -> std::string {
        const unsigned char* v = sqlite3_column_text(st, i);
        return v ? reinterpret_cast<const char*>(v) : "";
    };

    t.metadata.title  = optCol(4);
    t.metadata.artist = optCol(5);
    t.metadata.album  = optCol(6);
    t.metadata.genre  = optCol(7);
    if (sqlite3_column_type(st, 8) != SQLITE_NULL)
        t.metadata.year = sqlite3_column_int(st, 8);
    t.metadata.source = sourceFrom(col(9));

    t.analysis.bandRmsDb       = jsonToFloatArray(col(10));
    t.analysis.bandCorr        = jsonToFloatArray(col(11));
    t.analysis.bandTransientDb = jsonToFloatArray(col(12));
    t.analysis.overallRmsDb    = static_cast<float>(sqlite3_column_double(st, 13));
    t.analysis.overallCorr     = static_cast<float>(sqlite3_column_double(st, 14));
    t.analysis.lra             = static_cast<float>(sqlite3_column_double(st, 15));
    return t;
}

// ── SqliteTrackRepository ────────────────────────────────────────────────────

SqliteTrackRepository::SqliteTrackRepository(Database& db) : db_(db) {}

void SqliteTrackRepository::save(const Track& t) {
    sqlite3* db = db_.handle();
    sqlite3_stmt* st = nullptr;

    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO tracks(id, path, file_size, added_at) VALUES(?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, t.id.hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, t.path.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, t.fileSize);
    sqlite3_bind_text(st, 4, t.addedAt.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO track_metadata"
        "(track_id,title,artist,album,genre,year,source) VALUES(?,?,?,?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, t.id.hash.c_str(),                -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, optStr(t.metadata.title).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, optStr(t.metadata.artist).c_str(),-1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, optStr(t.metadata.album).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, optStr(t.metadata.genre).c_str(), -1, SQLITE_TRANSIENT);
    if (t.metadata.year)
        sqlite3_bind_int(st, 6, *t.metadata.year);
    else
        sqlite3_bind_null(st, 6);
    sqlite3_bind_text(st, 7, sourceStr(t.metadata.source).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    const auto rmsJson   = floatArrayToJson(t.analysis.bandRmsDb);
    const auto corrJson  = floatArrayToJson(t.analysis.bandCorr);
    const auto transJson = floatArrayToJson(t.analysis.bandTransientDb);
    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO track_analysis"
        "(track_id,band_rms_db,band_corr,band_transient_db,overall_rms_db,overall_corr,lra)"
        " VALUES(?,?,?,?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, t.id.hash.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, rmsJson.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, corrJson.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, transJson.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 5, static_cast<double>(t.analysis.overallRmsDb));
    sqlite3_bind_double(st, 6, static_cast<double>(t.analysis.overallCorr));
    sqlite3_bind_double(st, 7, static_cast<double>(t.analysis.lra));
    sqlite3_step(st);
    sqlite3_finalize(st);
}

std::optional<Track> SqliteTrackRepository::find(const TrackId& id) const {
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kSelectJoin) + " WHERE t.id = ?;";
    sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.hash.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<Track> result;
    if (sqlite3_step(st) == SQLITE_ROW) result = stmtToTrack(st);
    sqlite3_finalize(st);
    return result;
}

std::optional<Track> SqliteTrackRepository::findByPath(const std::string& path) const {
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kSelectJoin) + " WHERE t.path = ?;";
    sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_text(st, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<Track> result;
    if (sqlite3_step(st) == SQLITE_ROW) result = stmtToTrack(st);
    sqlite3_finalize(st);
    return result;
}

std::vector<Track> SqliteTrackRepository::search(const TrackFilter& filter) const {
    std::string sql = std::string(kSelectJoin);
    std::vector<std::string> clauses;
    if (filter.title)  clauses.push_back("LOWER(m.title)  LIKE '%' || LOWER(?) || '%'");
    if (filter.artist) clauses.push_back("LOWER(m.artist) LIKE '%' || LOWER(?) || '%'");
    if (filter.genre)  clauses.push_back("LOWER(m.genre)  LIKE '%' || LOWER(?) || '%'");

    if (!clauses.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < clauses.size(); ++i) {
            if (i > 0) sql += " AND ";
            sql += clauses[i];
        }
    }
    sql += ";";

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &st, nullptr);

    int bindIdx = 1;
    if (filter.title)  sqlite3_bind_text(st, bindIdx++, filter.title->c_str(),  -1, SQLITE_TRANSIENT);
    if (filter.artist) sqlite3_bind_text(st, bindIdx++, filter.artist->c_str(), -1, SQLITE_TRANSIENT);
    if (filter.genre)  sqlite3_bind_text(st, bindIdx++, filter.genre->c_str(),  -1, SQLITE_TRANSIENT);

    std::vector<Track> results;
    while (sqlite3_step(st) == SQLITE_ROW) results.push_back(stmtToTrack(st));
    sqlite3_finalize(st);
    return results;
}

bool SqliteTrackRepository::existsByBasenameAndSize(const std::string& basename,
                                                      int64_t            size) const {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "SELECT 1 FROM tracks"
        " WHERE file_size = ? AND path LIKE '%' || ? LIMIT 1;",
        -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, size);
    // Match any path whose last component equals basename.
    // We use '/' || basename as suffix; on Windows paths use '\' — acceptable
    // since MasterTweak targets Linux.
    const std::string suffix = "/" + basename;
    sqlite3_bind_text(st, 2, suffix.c_str(), -1, SQLITE_TRANSIENT);
    const bool found = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return found;
}

void SqliteTrackRepository::remove(const TrackId& id) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "DELETE FROM tracks WHERE id = ?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

} // namespace pb
