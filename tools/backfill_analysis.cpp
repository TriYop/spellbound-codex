#include "preset_builder/adapters/database.hpp"
#include "preset_builder/adapters/sqlite_track_repository.hpp"
#include "preset_builder/services/ingest_service.hpp"

#include <CLI11.hpp>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;

    CLI::App app{"mastertweak-backfill-analysis — re-analyse all tracks in the preset builder DB\n"
                 "Updates analysis fields (LRA, per-band RMS, etc.) for all existing tracks."};

    const char* home = std::getenv("HOME");
    const std::string defaultDb = home
        ? (fs::path(home) / ".config/MasterTweak/preset_builder.db").string()
        : "preset_builder.db";
    std::string dbPath = defaultDb;
    app.add_option("--db", dbPath,
                   "Path to preset_builder.db\n"
                   "  (default: ~/.config/MasterTweak/preset_builder.db)");

    bool dryRun = false;
    app.add_flag("--dry-run", dryRun,
                 "Print track count and exit without re-analysing");

    CLI11_PARSE(app, argc, argv);

    if (!fs::exists(dbPath)) {
        std::fprintf(stderr, "error: DB not found: %s\n", dbPath.c_str());
        return 1;
    }

    std::printf("DB: %s%s\n", dbPath.c_str(), dryRun ? " [dry run]" : "");
    std::fflush(stdout);

    pb::Database             db(dbPath);
    pb::SqliteTrackRepository repo(db);

    if (dryRun) {
        const auto tracks = repo.search({});
        std::printf("Would reanalyse %zu track(s).\n", tracks.size());
        return 0;
    }

    pb::IngestService service;

    const size_t totalTracks = repo.search({}).size();
    std::printf("Reanalysing %zu track(s) using %u thread(s)...\n",
                totalTracks,
                std::max(1u, std::thread::hardware_concurrency()));
    std::fflush(stdout);

    auto progress = [totalTracks](float frac, const std::string& msg) {
        const auto n = static_cast<size_t>(frac * static_cast<float>(totalTracks) + 0.5f);
        std::printf("  [%4zu/%4zu] %s\n", n, totalTracks, msg.c_str());
        std::fflush(stdout);
    };

    const auto report = service.reanalyse(repo, progress);

    std::printf("\nDone:\n");
    std::printf("  Reanalysed : %d\n", report.updated);
    std::printf("  Missing    : %d\n", report.skippedMissing);
    std::printf("  Failed     : %d\n", report.failed);
    if (report.cancelled)
        std::printf("  (cancelled)\n");
    for (const auto& [path, msg] : report.errors)
        std::fprintf(stderr, "  error: %s: %s\n", path.c_str(), msg.c_str());

    return report.failed > 0 ? 1 : 0;
}
