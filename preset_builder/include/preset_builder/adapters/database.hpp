#pragma once

#include <string>

struct sqlite3;  // forward declaration

namespace pb {

// RAII wrapper for a SQLite3 database connection.
// Opens the connection and creates all tables (CREATE TABLE IF NOT EXISTS).
// Use ":memory:" as path for in-memory databases in tests.
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const;

private:
    sqlite3* db_ = nullptr;

    void createSchema();
};

} // namespace pb
