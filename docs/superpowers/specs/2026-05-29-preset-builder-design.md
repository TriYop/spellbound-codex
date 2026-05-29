# Preset Builder — Design Spec

**Date:** 2026-05-29
**Feature:** Integrated preset builder that ingests audio tracks, stores analysis in SQLite, and exports MixAdvice-compatible preset XMLs

---

## Overview

Two sub-projects, built in order:

- **Sub-project A — `preset_builder_core`:** Static C++ library. Domain model, ports, services, SQLite adapters. No Qt.
- **Sub-project B — Qt6 Preset Builder dialog:** Standalone window opened from a "Manage presets…" button in `PresetSelector`. Three screens: Ingest, Browse/Tag, Create Preset.

The SQLite database lives at `~/.config/MixAdvice/preset_builder.db` and shares its schema with the existing Python preset-builder tool.

---

## Sub-project A: Core Library

### Domain Model

**`Track`** (entity, identity = SHA-256 content hash)

| Field | Type | Notes |
|---|---|---|
| `id` | `TrackId` | Value object wrapping hex SHA-256 |
| `path` | `std::string` | Filesystem path at ingest time; informational |
| `metadata` | `TrackMetadata` | See below |
| `analysis` | `TrackAnalysis` | See below |
| `addedAt` | `std::string` | ISO 8601 timestamp |

**`TrackMetadata`** (value object)

| Field | Type |
|---|---|
| `title` | `optional<string>` |
| `artist` | `optional<string>` |
| `album` | `optional<string>` |
| `genre` | `optional<string>` |
| `year` | `optional<int>` |
| `source` | `enum { acoustid, filename }` |

**`TrackAnalysis`** (value object — same field names as MixAdvice preset schema)

| Field | Type |
|---|---|
| `bandRmsDb` | `float[7]` |
| `bandCorrMin` | `float[7]` |
| `bandTransientDb` | `float[7]` |
| `overallRmsDb` | `float` |
| `overallCorr` | `float` |

**`TrackFilter`** (value object — used for repository search)

| Field | Type |
|---|---|
| `title` | `optional<string>` — case-insensitive LIKE |
| `artist` | `optional<string>` — case-insensitive LIKE |
| `genre` | `optional<string>` — case-insensitive LIKE |

All fields are optional and are combined with AND. An empty `TrackFilter` returns all tracks.

**`Preset`** (entity, identity = UUID string)

| Field | Type |
|---|---|
| `id` | `PresetId` |
| `name` | `string` |
| `description` | `string` |
| `trackIds` | `vector<TrackId>` |
| `createdAt` | `string` — ISO 8601 |

**`PresetStats`** (value object — computed on demand, never persisted)

| Field | Type | Aggregation |
|---|---|---|
| `bandRmsDb` | `float[7]` | mean across tracks |
| `bandCorrMin` | `float[7]` | 10th percentile (p10) |
| `bandTransientDb` | `float[7]` | median |
| `overallRmsDb` | `float` | mean |

---

### Ports (Interfaces)

**`TrackRepository`**
```cpp
optional<Track> find(TrackId) const;
optional<Track> findByPath(const string& path) const;
vector<Track>   search(const TrackFilter&) const;   // empty filter = all tracks
void            save(const Track&);                  // insert or update
void            remove(TrackId);
```

**`PresetRepository`**
```cpp
optional<Preset> find(PresetId) const;
vector<Preset>   listAll() const;
vector<Track>    tracksFor(PresetId) const;   // join, returns full Track objects
void             save(const Preset&);
void             remove(PresetId);
```

**`MetadataProvider`** (port; implemented in the GUI layer to keep Qt out of the core)
```cpp
optional<TrackMetadata> lookup(const string& audioFilePath);
```

---

### Services

**`IngestService`**

```cpp
IngestReport ingest(const string& path,
                    TrackRepository&,
                    MetadataProvider&,
                    mt::ProgressCallback progress = {});
// mt::ProgressCallback = std::function<void(float fraction, const std::string& stage)>
// — same type as the render pipeline's progress callback (pipeline.hpp)
```

Steps per file:
1. Discover audio files recursively (WAV, FLAC, AIFF) if `path` is a directory.
2. Compute SHA-256 content hash.
3. Skip if `TrackRepository::find(hash)` succeeds.
4. Run `SevenBandAnalyser` (from `mastertweak_core`) to produce `TrackAnalysis`.
5. Call `MetadataProvider::lookup(path)`. On failure, fall back to filename parsing: strip extension, split on ` – ` or ` - ` for artist/title; genre left empty.
6. Build `Track`, call `TrackRepository::save()`.

`IngestReport` value object:
```cpp
struct IngestReport {
    int added   = 0;
    int skipped = 0;   // already in DB
    int failed  = 0;
    vector<pair<string, string>> errors;   // {path, message}
};
```

**`StatsService`**

```cpp
PresetStats compute(const vector<Track>& tracks);
```

Pure computation, no I/O. Empty input returns zero-filled `PresetStats`.

Aggregation per band:
- `bandRmsDb[i]` — arithmetic mean
- `bandCorrMin[i]` — 10th percentile (sort + index at `floor(0.1 * n)`)
- `bandTransientDb[i]` — median (sort + middle element; average of two middles for even n)
- `overallRmsDb` — arithmetic mean

**`ExportService`**

```cpp
void exportXml(const Preset&, const PresetStats&, const string& outputPath);
```

Writes MixAdvice-compatible XML via pugixml (already vendored). Output path is caller-chosen; typical destinations are `~/.config/MixAdvice/Presets/<name>.xml` or a user-selected path.

XML schema (matches MixAdvice preset contract):
```xml
<preset name="..." description="...">
  <bandRmsDb>...</bandRmsDb>       <!-- 7 space-separated floats -->
  <bandMinCorr>...</bandMinCorr>   <!-- bandCorrMin → bandMinCorr in XML -->
  <bandTransientDb>...</bandTransientDb>
  <overallRmsDb>...</overallRmsDb>
  <overallMinCorr>...</overallMinCorr>
</preset>
```

---

### Infrastructure

**SQLite schema** (`~/.config/MixAdvice/preset_builder.db`)

Compatible with the Python preset-builder tool — same table names and column layout.

```sql
CREATE TABLE IF NOT EXISTS tracks (
    id       TEXT PRIMARY KEY,   -- SHA-256 hex
    path     TEXT,
    added_at TEXT                -- ISO 8601
);

CREATE TABLE IF NOT EXISTS track_metadata (
    track_id TEXT PRIMARY KEY REFERENCES tracks(id),
    title    TEXT,
    artist   TEXT,
    album    TEXT,
    genre    TEXT,
    year     INTEGER,
    source   TEXT                -- 'acoustid' | 'filename'
);

CREATE TABLE IF NOT EXISTS track_analysis (
    track_id           TEXT PRIMARY KEY REFERENCES tracks(id),
    band_rms_db        TEXT,     -- JSON array [7]
    band_corr_min      TEXT,     -- JSON array [7]
    band_transient_db  TEXT,     -- JSON array [7]
    overall_rms_db     REAL,
    overall_corr       REAL
);

CREATE TABLE IF NOT EXISTS presets (
    id          TEXT PRIMARY KEY,  -- UUID
    name        TEXT,
    description TEXT,
    created_at  TEXT
);

CREATE TABLE IF NOT EXISTS preset_tracks (
    preset_id TEXT REFERENCES presets(id),
    track_id  TEXT REFERENCES tracks(id),
    PRIMARY KEY (preset_id, track_id)
);
```

Schema is created (and future migrations applied) by `db_schema.cpp` on first open. Version tracked via SQLite `user_version` pragma.

**Adapters**

- `SqliteTrackRepository` — thin RAII wrapper around the SQLite3 C API (vendored amalgamation under `third_party/sqlite3.h/.c`). `search(TrackFilter)` builds the WHERE clause from whichever optional fields are set.
- `SqlitePresetRepository` — same pattern. `tracksFor(PresetId)` runs a three-table join.
- `AcoustIdMetadataProvider` — lives in `gui/`, not the core lib. Runs `fpcalc` via `QProcess` (`fpcalc` is a runtime dependency; if not on `PATH`, lookup silently returns `nullopt`), calls the AcoustID API via `QNetworkAccessManager`. Returns `nullopt` on any failure; caller falls back to filename parsing.

---

### Directory Layout

```
preset_builder/
  include/preset_builder/
    domain/
      track.hpp          # Track, TrackId, TrackMetadata, TrackAnalysis, TrackFilter
      preset.hpp         # Preset, PresetId, PresetStats
    ports/
      track_repository.hpp
      preset_repository.hpp
      metadata_provider.hpp
    services/
      ingest_service.hpp
      stats_service.hpp
      export_service.hpp
  src/
    adapters/
      sqlite_track_repository.cpp
      sqlite_preset_repository.cpp
      db_schema.cpp      # CREATE TABLE IF NOT EXISTS + user_version migrations
    services/
      ingest_service.cpp
      stats_service.cpp
      export_service.cpp
  CMakeLists.txt         # static lib preset_builder_core
                         # links: mastertweak_core, sqlite3
tests/
  test_preset_builder.cpp   # doctest: StatsService (pure), IngestService with stub repos

gui/
  AcoustIdMetadataProvider.h
  AcoustIdMetadataProvider.cpp
```

`preset_builder_core` depends on `mastertweak_core` (for `SevenBandAnalyser`) and the vendored SQLite3 amalgamation. It has no Qt dependency.

---

## Sub-project B: Qt6 Preset Builder Dialog

Opened via a **"Manage presets…" button** in `PresetSelector`. Launches as a standalone `QDialog` (resizable, non-modal).

Three screens presented as a `QTabWidget`:

| Tab | Purpose |
|---|---|
| **Ingest** | Drop zone + directory picker; progress bar; `IngestReport` summary |
| **Browse / Tag** | Searchable track list filtered by title/artist/genre; inline metadata edit |
| **Create Preset** | Name + description fields; track multi-select from search results; stats preview; Export button |

The dialog owns `SqliteTrackRepository`, `SqlitePresetRepository`, and `AcoustIdMetadataProvider` instances. Worker threads run `IngestService` and `StatsService` (same `QThread` pattern as `AnalysisWorker`/`RenderWorker` in `MainWindow`). `PresetId` UUIDs are generated via `QUuid::createUuid().toString()` at save time in the dialog layer; the core domain model treats them as opaque strings.

Sub-project B design will be written as a separate spec once Sub-project A is implemented and passing tests.

---

## Testing

Unit tests in `tests/test_preset_builder.cpp` (doctest):
- `StatsService::compute()` — known input tracks, verify mean/p10/median per band
- `IngestService` — stub `TrackRepository` and `MetadataProvider`; verify skip-on-duplicate, fallback metadata parsing, `IngestReport` counts
- `ExportService` — verify XML output matches expected schema

No tests for SQLite adapters or the Qt metadata provider (integration concerns; covered by manual testing of the dialog).
