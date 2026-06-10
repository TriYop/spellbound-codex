#pragma once

#include "preset_builder/domain/track.hpp"
#include "preset_builder/ports/metadata_provider.hpp"
#include "preset_builder/ports/track_repository.hpp"
#include "mastertweak/pipeline.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace pb {

struct IngestReport {
    int  added     = 0;
    int  skipped   = 0;    // already in DB (basename+size match or same SHA-256 hash)
    int  failed    = 0;
    bool cancelled = false; // set when caller signals stop via cancel token
    std::vector<std::pair<std::string, std::string>> errors;  // {path, message}
};

struct ReanalyseReport {
    int  updated        = 0;
    int  skippedMissing = 0;  // file no longer on disk
    int  failed         = 0;
    bool cancelled      = false;
    std::vector<std::pair<std::string, std::string>> errors;
};

class IngestService {
public:
    // Ingest a single file or a directory (recursive scan for WAV/FLAC/AIFF/AIF/MP3/OGG).
    // Skips files already in the repository (by basename+size pre-check, then SHA-256).
    // Falls back to parseFilenameMetadata() when MetadataProvider returns nullopt.
    // Pass a non-null cancel pointer to support cooperative cancellation: the
    // worker threads check it between files and stop as soon as it is true.
    using ErrorCallback = std::function<void(const std::string& path, const std::string& msg)>;

    IngestReport ingest(const std::string& path,
                        TrackRepository&   repo,
                        MetadataProvider&  metadata,
                        mt::ProgressCallback progress  = {},
                        std::atomic<bool>*   cancel    = nullptr,
                        ErrorCallback        onError   = {}) const;

    // Re-analyse all tracks already in the repository using the current analyseFile()
    // implementation. Updates analysis fields in-place (needed after schema migrations
    // that add new fields like lra which default to 0 for existing rows).
    // Pass a non-null cancel pointer to support cooperative cancellation.
    ReanalyseReport reanalyse(TrackRepository&     repo,
                               mt::ProgressCallback progress = {},
                               std::atomic<bool>*   cancel   = nullptr) const;

    // Extract artist and title from a filename (stem only, path prefix stripped).
    // Splits on " - " or " \xe2\x80\x93 " (UTF-8 en-dash).
    // Returns TrackMetadata with source == MetadataSource::filename.
    static TrackMetadata parseFilenameMetadata(const std::string& filePath);
};

} // namespace pb
