#include "preset_builder/adapters/database.hpp"

#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace pb {

static void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("Database: SQL error: " + msg);
    }
}

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string msg = db_ ? sqlite3_errmsg(db_) : "cannot open";
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Database: cannot open '" + path + "': " + msg);
    }
    createSchema();
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

sqlite3* Database::handle() const { return db_; }

void Database::createSchema() {
    exec(db_, "PRAGMA foreign_keys = ON;");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id        TEXT PRIMARY KEY,
            path      TEXT,
            file_size INTEGER DEFAULT 0,
            added_at  TEXT
        );
    )");
    // Migration: add file_size to databases created before this column existed.
    // sqlite3_exec returns SQLITE_ERROR with "duplicate column name" if it exists; ignore that.
    sqlite3_exec(db_,
        "ALTER TABLE tracks ADD COLUMN file_size INTEGER DEFAULT 0;",
        nullptr, nullptr, nullptr);
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS track_metadata (
            track_id TEXT PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
            title    TEXT,
            artist   TEXT,
            album    TEXT,
            genre    TEXT,
            year     INTEGER,
            source   TEXT
        );
    )");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS track_analysis (
            track_id           TEXT PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
            band_rms_db        TEXT,
            band_corr          TEXT,
            band_transient_db  TEXT,
            overall_rms_db     REAL,
            overall_corr       REAL
        );
    )");
    // Migration: add lra column to databases created before this column existed.
    sqlite3_exec(db_,
        "ALTER TABLE track_analysis ADD COLUMN lra REAL DEFAULT 0;",
        nullptr, nullptr, nullptr);
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS presets (
            id          TEXT PRIMARY KEY,
            name        TEXT,
            description TEXT,
            created_at  TEXT
        );
    )");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS preset_tracks (
            preset_id TEXT REFERENCES presets(id) ON DELETE CASCADE,
            track_id  TEXT REFERENCES tracks(id)  ON DELETE CASCADE,
            PRIMARY KEY (preset_id, track_id)
        );
    )");
}

} // namespace pb
