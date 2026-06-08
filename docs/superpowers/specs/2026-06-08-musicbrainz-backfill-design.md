# MusicBrainz Backfill — Design Spec

**Date:** 2026-06-08  
**File:** `tools/backfill_metadata.py`  
**Status:** Approved, ready for implementation

## Goal

Extend `backfill_metadata.py` with a second pass that queries MusicBrainz text search for rows that embedded tags could not fully populate (missing genre, year, title, artist, or album).

## Overall Structure

```
backfill(db_path, dry_run, no_online)
  ├─ Pass 1: embedded-tag sweep (existing, unchanged)
  │    query: genre IS NULL OR genre = '' OR year IS NULL
  │    → commit updates, print pass-1 summary
  │
  └─ Pass 2: MusicBrainz sweep (new, skipped with --no-online)
       query: same NULL condition (re-run after pass 1 commits)
       → for each row: resolve search key → query MB → write back missing fields
       → print pass-2 summary
```

Pass 1 commits before pass 2 runs, so pass 2 only touches rows that embedded tags genuinely could not fill. A single `--dry-run` flag covers both passes.

## MusicBrainz Lookup Logic

### Search key resolution (in order)

1. Use `(title, artist)` from DB if both are present.
2. If either is missing, parse the filename stem on ` - ` (same separator as the C++ `parseFilenameMetadata`) → `(artist, title)` or `(title, "")` if no separator found.
3. If nothing usable can be derived, skip the row.

### Query

```python
musicbrainzngs.search_recordings(recording=title, artist=artist, limit=1)
```

Take the top result unconditionally.

### Field mapping from top result

| DB field | MusicBrainz source |
|---|---|
| `title`  | `rec['title']` |
| `artist` | `rec['artist-credit'][0]['artist']['name']` |
| `year`   | `rec.get('first-release-date', '')[:4]` (integer) |
| `album`  | `rec.get('release-list', [{}])[0].get('title', '')` |
| `genre`  | highest-count entry in `rec.get('tag-list', [])` |

Only fields still NULL or empty in the DB are written — existing values are never replaced, even by the online pass.

### Rate limiting

`time.sleep(1)` between every MusicBrainz request.  
User-agent set once at module level: `"MasterTweak/0.1 (yvan.janet@gmail.com)"`.

## CLI Interface

New flag added to existing `argparse`:

```
--no-online    Skip the MusicBrainz pass (embedded tags only)
```

## Error Handling

- `musicbrainzngs` not installed → print `pip install musicbrainzngs` hint, skip pass 2 gracefully (no crash).
- Network timeout or malformed response → log row as `SKIP`, continue loop.
- Empty result list → skip row silently.
- `source` column in `track_metadata` is left untouched by both passes (reflects ingest-time origin, not enrichment).

## Reporting

Pass summaries are printed separately:

```
Pass 1 (embedded tags): updated 42, skipped 310, failed 3
Pass 2 (MusicBrainz):   updated 18, skipped 24, failed 0
```

## Dependencies

- `mutagen` — already required (pass 1)
- `musicbrainzngs` — new, optional (pass 2 skips gracefully if absent)

No new system dependencies. `musicbrainzngs` is a pure-Python package.
