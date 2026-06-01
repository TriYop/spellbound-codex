#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace pb {

struct TrackId {
    std::string hash;  // SHA-256 hex (64 chars)
    bool operator==(const TrackId&) const = default;
};

enum class MetadataSource { acoustid, filename };

struct TrackMetadata {
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> genre;
    std::optional<int>         year;
    MetadataSource             source = MetadataSource::filename;
};

struct TrackAnalysis {
    std::array<float, 7> bandRmsDb{};       // per-band long-term RMS (dBFS)
    std::array<float, 7> bandCorr{};        // per-band L/R Pearson correlation
    std::array<float, 7> bandTransientDb{}; // per-band crest factor (dB)
    float                overallRmsDb = 0.f;
    float                overallCorr  = 1.f;
};

// All fields optional; only provided fields become WHERE clauses (AND).
// Empty TrackFilter means "all tracks".
struct TrackFilter {
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> genre;
};

struct Track {
    TrackId       id;
    std::string   path;
    int64_t       fileSize  = 0;  // bytes, from stat at ingest time
    TrackMetadata metadata;
    TrackAnalysis analysis;
    std::string   addedAt;  // ISO 8601 (UTC)
};

} // namespace pb
