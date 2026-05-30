#pragma once

#include "preset_builder/domain/track.hpp"
#include "preset_builder/ports/metadata_provider.hpp"
#include "preset_builder/ports/track_repository.hpp"
#include "mastertweak/pipeline.hpp"

#include <atomic>
#include <string>
#include <vector>

namespace pb {

struct IngestReport {
    int  added     = 0;
    int  skipped   = 0;    // already in DB (same SHA-256 hash)
    int  failed    = 0;
    bool cancelled = false; // set when caller signals stop via cancel token
    std::vector<std::pair<std::string, std::string>> errors;  // {path, message}
};

class IngestService {
public:
    // Ingest a single file or a directory (recursive scan for WAV/FLAC/AIFF/AIF).
    // Skips files already in the repository (by SHA-256 hash).
    // Falls back to parseFilenameMetadata() when MetadataProvider returns nullopt.
    // Pass a non-null cancel pointer to support cooperative cancellation: the
    // worker threads check it between files and stop as soon as it is true.
    IngestReport ingest(const std::string& path,
                        TrackRepository&   repo,
                        MetadataProvider&  metadata,
                        mt::ProgressCallback  progress = {},
                        std::atomic<bool>* cancel   = nullptr) const;

    // Extract artist and title from a filename (stem only, path prefix stripped).
    // Splits on " - " or " \xe2\x80\x93 " (UTF-8 en-dash).
    // Returns TrackMetadata with source == MetadataSource::filename.
    static TrackMetadata parseFilenameMetadata(const std::string& filePath);
};

} // namespace pb
