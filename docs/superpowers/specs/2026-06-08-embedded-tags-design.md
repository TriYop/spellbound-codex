# Embedded Tag Metadata Enrichment

**Date:** 2026-06-08
**Status:** Approved

## Context

MasterTweak's preset builder currently ignores embedded audio tags (ID3, Vorbis comments, FLAC metadata). The `track_metadata` schema has `genre` and `year` columns but they are never populated — AcoustID cannot provide them, and filename parsing doesn't produce them. With a 22k-track library already ingested, empty genre/year labels are a blocker for a future MLP-based preset recommendation system.

## Goal

Add TagLib as a second metadata source that fills any field AcoustID left empty. A separate Python backfill script patches the existing DB without re-ingesting.

## Merge strategy

```
AcoustID lookup  →  TagLib read  →  per-field merge  →  filename parse (last resort)  →  persist
```

Merge rule: keep AcoustID value when non-empty; otherwise use embedded tag value.
- Genre and year: always from TagLib (AcoustID never provides them).
- Title / artist / album: AcoustID wins; embedded tags fill in for unrecognized tracks.
- Filename parse: last resort for title/artist only, unchanged.

`MetadataSource` semantics after merge:
- `acoustid` — AcoustID recognized the track (TagLib may have added genre/year on top)
- `embedded_tags` — AcoustID found nothing; embedded tags had title/artist
- `filename` — both sources failed to identify the track

## Components

### 1. EmbeddedTagMetadataProvider (new adapter)

Implements the existing `MetadataProvider` port (no interface change).

Files:
- `preset_builder/include/preset_builder/adapters/embedded_tag_metadata_provider.hpp`
- `preset_builder/src/adapters/embedded_tag_metadata_provider.cpp`

Uses `TagLib::FileRef` to read all five fields. Returns `std::nullopt` only if TagLib cannot open the file. Returns populated `TrackMetadata` with empty-string fields for missing tags.

### 2. IngestService merge logic

File: `preset_builder/src/services/ingest_service.cpp`

After the existing `metaProvider.lookup(filePath)` call, call `EmbeddedTagMetadataProvider` and merge per-field. Update `MetadataSource` based on which source provided title/artist.

### 3. MetadataSource enum

File: `preset_builder/include/preset_builder/domain/track.hpp`

Add `embedded_tags` value. No DB schema change — the column is TEXT.

### 4. Build / CMake

File: `preset_builder/CMakeLists.txt`

Add TagLib via pkg-config (`pkg_check_modules(TAGLIB REQUIRED taglib)`). Link only into `preset_builder` target. System package: `libtag1-dev`.

### 5. Backfill script

File: `tools/backfill_metadata.py`

Python 3, uses `mutagen` + `sqlite3`. Reads DB paths, updates only empty fields per-track. `--dry-run` flag. Prints summary at end.

## Verification

1. `cmake --build build --parallel` — clean build with TagLib linked
2. Ingest a well-tagged FLAC or MP3, verify genre/year in DB
3. Ingest an AcoustID-recognized track with embedded tags → `source='acoustid'`, genre/year populated
4. Ingest an unrecognized track with embedded title/artist → `source='embedded_tags'`
5. Ingest a bare file → `source='filename'`, genre/year NULL
6. `python tools/backfill_metadata.py --dry-run` → shows changes, no DB writes
7. `python tools/backfill_metadata.py` → previously empty fields now filled
8. `ctest --test-dir build --output-on-failure` — existing tests still pass

## Future work

MLP Option 2 (deferred): once genre/year labels are in place and tracks are assigned to presets, train an MLP on 23 acoustic features to predict PresetStats. Minimum ~100 labelled tracks across ≥3 presets.
