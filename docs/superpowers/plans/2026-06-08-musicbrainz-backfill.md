# MusicBrainz Backfill Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `tools/backfill_metadata.py` with a second pass that queries MusicBrainz text search for rows still missing fields after the embedded-tag pass.

**Architecture:** The existing `backfill()` is split into `_backfill_embedded(conn, dry_run)` (pass 1, unchanged logic) and a new `backfill_online(conn, dry_run)` (pass 2). The outer `backfill(db_path, dry_run, online)` opens the connection, calls both, then closes and prints per-pass summaries. A `--no-online` CLI flag skips pass 2.

**Tech Stack:** Python 3.12+, `sqlite3` (stdlib), `mutagen` (existing), `musicbrainzngs` (new optional dep), `pytest`, `unittest.mock`

---

## File Map

- **Modify:** `tools/backfill_metadata.py` — add `parse_filename_stem`, `search_musicbrainz`, `backfill_online`; refactor `backfill` → `_backfill_embedded` + new `backfill`; update `main`
- **Create:** `tools/test_backfill_metadata.py` — all tests

---

### Task 1: Add `parse_filename_stem()` with tests

**Files:**
- Modify: `tools/backfill_metadata.py`
- Create: `tools/test_backfill_metadata.py`

- [ ] **Step 1: Create the test file with failing tests for `parse_filename_stem`**

Create `tools/test_backfill_metadata.py`:

```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from backfill_metadata import parse_filename_stem


def test_hyphen_separator():
    assert parse_filename_stem("/music/Artist - Title.mp3") == ("Artist", "Title")


def test_endash_separator():
    assert parse_filename_stem("/music/Artist – Title.flac") == ("Artist", "Title")


def test_no_separator_returns_empty_artist():
    assert parse_filename_stem("/music/Title Only.wav") == ("", "Title Only")


def test_multiple_separators_splits_on_first():
    assert parse_filename_stem("/music/Artist - Title - Remix.mp3") == ("Artist", "Title - Remix")


def test_strips_whitespace():
    assert parse_filename_stem("/music/  Artist  -  Title  .aiff") == ("Artist", "Title")


def test_deep_path_ignored():
    assert parse_filename_stem("/deep/path/to/Artist - Title.flac") == ("Artist", "Title")
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
python -m pytest tools/test_backfill_metadata.py -v
```

Expected: `ImportError` or `AttributeError` — `parse_filename_stem` not yet defined.

- [ ] **Step 3: Add `parse_filename_stem` to `backfill_metadata.py`**

In `tools/backfill_metadata.py`, add `import time` to the existing imports block, then add this function directly after `is_empty`:

```python
import argparse
import sqlite3
import time
from pathlib import Path


def is_empty(val) -> bool:
    """True if DB value is NULL or empty string."""
    return val is None or val == ""


def parse_filename_stem(path: str) -> tuple[str, str]:
    """Return (artist, title) parsed from the filename stem.

    Splits on ' - ' or ' – ' (en-dash), mirroring C++ parseFilenameMetadata.
    Returns ('', stem) when no separator is found.
    """
    stem = Path(path).stem
    for sep in (" - ", " – "):
        if sep in stem:
            artist, _, title = stem.partition(sep)
            return artist.strip(), title.strip()
    return "", stem.strip()
```

- [ ] **Step 4: Run tests to confirm they pass**

```bash
python -m pytest tools/test_backfill_metadata.py -v
```

Expected: 6 tests PASSED.

- [ ] **Step 5: Commit**

```bash
git add tools/backfill_metadata.py tools/test_backfill_metadata.py
git commit -m "feat: add parse_filename_stem with tests"
```

---

### Task 2: Add `search_musicbrainz()` with tests

**Files:**
- Modify: `tools/backfill_metadata.py`
- Modify: `tools/test_backfill_metadata.py`

- [ ] **Step 1: Add failing tests for `search_musicbrainz`**

Append to `tools/test_backfill_metadata.py`:

```python
import sys
from unittest.mock import MagicMock, patch

from backfill_metadata import search_musicbrainz


def _make_recording(title="Song", artist_name="Artist", date="2020-01-01",
                    album="Great Album", tags=None):
    return {
        "title": title,
        "artist-credit": [{"artist": {"name": artist_name}}],
        "first-release-date": date,
        "release-list": [{"title": album}],
        "tag-list": tags or [{"name": "rock", "count": "5"}],
    }


def _mock_mb(recording=None):
    """Return a MagicMock for musicbrainzngs with search_recordings pre-wired."""
    mb = MagicMock()
    mb.search_recordings.return_value = {
        "recording-list": [recording] if recording else []
    }
    return mb


def test_search_returns_all_fields():
    mb = _mock_mb(_make_recording())
    with patch.dict(sys.modules, {"musicbrainzngs": mb}):
        result = search_musicbrainz("Song", "Artist")
    assert result == {
        "title": "Song", "artist": "Artist",
        "year": 2020, "album": "Great Album", "genre": "rock",
    }


def test_search_empty_on_no_results():
    mb = _mock_mb()
    with patch.dict(sys.modules, {"musicbrainzngs": mb}):
        result = search_musicbrainz("Unknown", "Nobody")
    assert result == {}


def test_search_empty_on_network_error():
    mb = MagicMock()
    mb.search_recordings.side_effect = Exception("timeout")
    with patch.dict(sys.modules, {"musicbrainzngs": mb}):
        result = search_musicbrainz("Song", "Artist")
    assert result == {}


def test_search_picks_highest_count_tag():
    tags = [
        {"name": "pop", "count": "2"},
        {"name": "rock", "count": "10"},
        {"name": "indie", "count": "1"},
    ]
    mb = _mock_mb(_make_recording(tags=tags))
    with patch.dict(sys.modules, {"musicbrainzngs": mb}):
        result = search_musicbrainz("Song", "Artist")
    assert result["genre"] == "rock"


def test_search_empty_on_import_error():
    with patch.dict(sys.modules, {"musicbrainzngs": None}):
        result = search_musicbrainz("Song", "Artist")
    assert result == {}


def test_search_year_from_date_prefix():
    mb = _mock_mb(_make_recording(date="1999-07"))
    with patch.dict(sys.modules, {"musicbrainzngs": mb}):
        result = search_musicbrainz("Song", "Artist")
    assert result["year"] == 1999
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
python -m pytest tools/test_backfill_metadata.py::test_search_returns_all_fields -v
```

Expected: `ImportError` — `search_musicbrainz` not yet defined.

- [ ] **Step 3: Add `search_musicbrainz` to `backfill_metadata.py`**

Add after `parse_filename_stem`:

```python
def search_musicbrainz(title: str, artist: str) -> dict:
    """Query MusicBrainz text search. Returns dict of non-empty fields, or {} on any failure."""
    try:
        import musicbrainzngs
        musicbrainzngs.set_useragent("MasterTweak", "0.1", "yvan.janet@gmail.com")
    except ImportError:
        return {}

    try:
        result = musicbrainzngs.search_recordings(recording=title, artist=artist, limit=1)
        recordings = result.get("recording-list", [])
        if not recordings:
            return {}
        rec = recordings[0]
        fields: dict = {}

        if rec.get("title"):
            fields["title"] = rec["title"]

        credits = rec.get("artist-credit", [])
        if credits and isinstance(credits[0], dict):
            name = credits[0].get("artist", {}).get("name", "")
            if name:
                fields["artist"] = name

        date = rec.get("first-release-date", "")
        if len(date) >= 4:
            try:
                fields["year"] = int(date[:4])
            except ValueError:
                pass

        releases = rec.get("release-list", [])
        if releases and releases[0].get("title"):
            fields["album"] = releases[0]["title"]

        tags = rec.get("tag-list", [])
        if tags:
            top = max(tags, key=lambda t: int(t.get("count", 0)))
            if top.get("name"):
                fields["genre"] = top["name"]

        return fields
    except Exception:
        return {}
```

- [ ] **Step 4: Run all tests to confirm they pass**

```bash
python -m pytest tools/test_backfill_metadata.py -v
```

Expected: 12 tests PASSED.

- [ ] **Step 5: Commit**

```bash
git add tools/backfill_metadata.py tools/test_backfill_metadata.py
git commit -m "feat: add search_musicbrainz with tests"
```

---

### Task 3: Add `backfill_online()` with tests

**Files:**
- Modify: `tools/backfill_metadata.py`
- Modify: `tools/test_backfill_metadata.py`

- [ ] **Step 1: Add failing tests for `backfill_online`**

Append to `tools/test_backfill_metadata.py`:

```python
import sqlite3
from unittest.mock import patch, MagicMock

from backfill_metadata import backfill_online


def _make_test_db(title="Title", artist="Artist", genre=None, year=None, album=None):
    """In-memory DB with the preset_builder schema and one incomplete track."""
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    conn.executescript("""
        CREATE TABLE tracks (
            id TEXT PRIMARY KEY, path TEXT, added_at TEXT, file_size INTEGER DEFAULT 0
        );
        CREATE TABLE track_metadata (
            track_id TEXT PRIMARY KEY REFERENCES tracks(id),
            title TEXT, artist TEXT, album TEXT, genre TEXT, year INTEGER, source TEXT
        );
    """)
    conn.execute(
        "INSERT INTO tracks VALUES ('id1', '/music/Artist - Title.mp3', '2026-01-01', 0)"
    )
    conn.execute(
        "INSERT INTO track_metadata VALUES ('id1', ?, ?, ?, ?, ?, 'embedded_tags')",
        (title, artist, album, genre, year),
    )
    conn.commit()
    return conn


def _mb_available():
    """Patch context that makes musicbrainzngs importable inside backfill_online."""
    return patch.dict(sys.modules, {"musicbrainzngs": MagicMock()})


def test_online_fills_missing_fields():
    conn = _make_test_db()
    with _mb_available(), \
         patch("backfill_metadata.search_musicbrainz", return_value={"genre": "rock", "year": 2020, "album": "Big Album"}), \
         patch("time.sleep"):
        updated, skipped, failed = backfill_online(conn, dry_run=False)

    assert updated == 1
    assert skipped == 0
    row = conn.execute("SELECT genre, year, album FROM track_metadata WHERE track_id='id1'").fetchone()
    assert row["genre"] == "rock"
    assert row["year"] == 2020
    assert row["album"] == "Big Album"
    conn.close()


def test_online_skips_when_search_returns_nothing():
    conn = _make_test_db()
    with _mb_available(), \
         patch("backfill_metadata.search_musicbrainz", return_value={}), \
         patch("time.sleep"):
        updated, skipped, failed = backfill_online(conn, dry_run=False)

    assert updated == 0
    assert skipped == 1
    conn.close()


def test_online_dry_run_does_not_write():
    conn = _make_test_db()
    with _mb_available(), \
         patch("backfill_metadata.search_musicbrainz", return_value={"genre": "jazz"}), \
         patch("time.sleep"):
        backfill_online(conn, dry_run=True)

    row = conn.execute("SELECT genre FROM track_metadata WHERE track_id='id1'").fetchone()
    assert row["genre"] is None
    conn.close()


def test_online_does_not_overwrite_existing_fields():
    conn = _make_test_db(genre="pop", year=2015)
    # Row already has genre and year — falls outside the query filter, so backfill_online
    # won't touch it; it's returned as skipped.
    with _mb_available(), \
         patch("backfill_metadata.search_musicbrainz", return_value={"genre": "rock", "year": 2020}), \
         patch("time.sleep"):
        updated, skipped, failed = backfill_online(conn, dry_run=False)

    assert updated == 0
    row = conn.execute("SELECT genre, year FROM track_metadata WHERE track_id='id1'").fetchone()
    assert row["genre"] == "pop"
    assert row["year"] == 2015
    conn.close()


def test_online_uses_filename_stem_when_title_and_artist_missing():
    conn = _make_test_db(title=None, artist=None)
    with _mb_available(), \
         patch("backfill_metadata.search_musicbrainz", return_value={"genre": "rock"}) as mock_search, \
         patch("time.sleep"):
        backfill_online(conn, dry_run=False)

    # Path is /music/Artist - Title.mp3 → stem artist="Artist", title="Title"
    mock_search.assert_called_once_with("Title", "Artist")
    conn.close()


def test_online_skips_when_import_fails():
    conn = _make_test_db()
    with patch.dict(sys.modules, {"musicbrainzngs": None}):
        updated, skipped, failed = backfill_online(conn, dry_run=False)

    assert updated == 0
    assert skipped == 0
    assert failed == 0
    conn.close()
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
python -m pytest tools/test_backfill_metadata.py -k "online" -v
```

Expected: `ImportError` — `backfill_online` not yet defined.

- [ ] **Step 3: Add `backfill_online` to `backfill_metadata.py`**

Add after `search_musicbrainz`:

```python
def backfill_online(conn: sqlite3.Connection, dry_run: bool) -> tuple[int, int, int]:
    """Pass 2: query MusicBrainz for rows still missing fields. Returns (updated, skipped, failed)."""
    try:
        import musicbrainzngs  # noqa: F401
    except ImportError:
        print("Pass 2 skipped: musicbrainzngs not installed (pip install musicbrainzngs)")
        return 0, 0, 0

    cur = conn.cursor()
    rows = cur.execute(
        "SELECT t.id, t.path, tm.title, tm.artist, tm.album, tm.genre, tm.year "
        "FROM tracks t JOIN track_metadata tm ON t.id = tm.track_id "
        "WHERE tm.genre IS NULL OR tm.genre = '' OR tm.year IS NULL"
    ).fetchall()

    updated = skipped = failed = 0

    for row in rows:
        track_id = row["id"]
        path = row["path"]

        title = row["title"] or ""
        artist = row["artist"] or ""
        if not title or not artist:
            stem_artist, stem_title = parse_filename_stem(path)
            if not title:
                title = stem_title
            if not artist:
                artist = stem_artist

        if not title:
            skipped += 1
            continue

        fields = search_musicbrainz(title, artist)
        if not fields:
            skipped += 1
            continue

        updates = {}
        for field in ("title", "artist", "album", "genre"):
            if is_empty(row[field]) and fields.get(field):
                updates[field] = fields[field]
        if is_empty(row["year"]) and "year" in fields:
            updates["year"] = fields["year"]

        if not updates:
            skipped += 1
            continue

        if dry_run:
            print(f"  DRY-RUN  {Path(path).name}: {updates}")
        else:
            set_clause = ", ".join(f"{k} = ?" for k in updates)
            vals = list(updates.values()) + [track_id]
            cur.execute(f"UPDATE track_metadata SET {set_clause} WHERE track_id = ?", vals)
        updated += 1

        time.sleep(1)

    if not dry_run:
        conn.commit()

    return updated, skipped, failed
```

- [ ] **Step 4: Run all tests**

```bash
python -m pytest tools/test_backfill_metadata.py -v
```

Expected: all tests PASSED (18+).

- [ ] **Step 5: Commit**

```bash
git add tools/backfill_metadata.py tools/test_backfill_metadata.py
git commit -m "feat: add backfill_online (MusicBrainz pass 2) with tests"
```

---

### Task 4: Refactor `backfill()` and wire `--no-online` into `main()`

**Files:**
- Modify: `tools/backfill_metadata.py`

- [ ] **Step 1: Replace the existing `backfill()` and `main()` functions**

Replace the existing `backfill()` function (lines 37–91) and `main()` function (lines 94–106) with:

```python
def _backfill_embedded(conn: sqlite3.Connection, dry_run: bool) -> tuple[int, int, int]:
    """Pass 1: fill missing fields from embedded audio tags. Returns (updated, skipped, failed)."""
    cur = conn.cursor()
    rows = cur.execute(
        "SELECT t.id, t.path, tm.title, tm.artist, tm.album, tm.genre, tm.year "
        "FROM tracks t JOIN track_metadata tm ON t.id = tm.track_id "
        "WHERE tm.genre IS NULL OR tm.genre = '' OR tm.year IS NULL"
    ).fetchall()

    updated = skipped = failed = 0

    for row in rows:
        track_id = row["id"]
        path = row["path"]

        if not Path(path).exists():
            print(f"  MISSING  {path}")
            failed += 1
            continue

        tags = extract_tags(path)
        if not tags:
            skipped += 1
            continue

        updates = {}
        for field in ("title", "artist", "album", "genre"):
            if is_empty(row[field]) and tags.get(field):
                updates[field] = tags[field]
        if is_empty(row["year"]) and "year" in tags:
            updates["year"] = tags["year"]

        if not updates:
            skipped += 1
            continue

        if dry_run:
            print(f"  DRY-RUN  {Path(path).name}: {updates}")
        else:
            set_clause = ", ".join(f"{k} = ?" for k in updates)
            vals = list(updates.values()) + [track_id]
            cur.execute(f"UPDATE track_metadata SET {set_clause} WHERE track_id = ?", vals)
        updated += 1

    if not dry_run:
        conn.commit()

    return updated, skipped, failed


def backfill(db_path: str, dry_run: bool, online: bool = True) -> None:
    conn = sqlite3.connect(db_path)
    conn.text_factory = lambda b: b.decode("utf-8", errors="surrogateescape")
    conn.row_factory = sqlite3.Row

    p1_updated, p1_skipped, p1_failed = _backfill_embedded(conn, dry_run)

    p2_updated = p2_skipped = p2_failed = 0
    if online:
        p2_updated, p2_skipped, p2_failed = backfill_online(conn, dry_run)

    conn.close()

    suffix = " (dry run)" if dry_run else ""
    print(f"\nDone{suffix}:")
    print(f"  Pass 1 (embedded tags): updated {p1_updated}, skipped {p1_skipped}, failed {p1_failed}")
    if online:
        print(f"  Pass 2 (MusicBrainz):   updated {p2_updated}, skipped {p2_skipped}, failed {p2_failed}")


def main():
    default_db = Path.home() / ".config" / "MasterTweak" / "preset_builder.db"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", default=str(default_db), help="Path to preset_builder.db")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without writing")
    parser.add_argument("--no-online", action="store_true", help="Skip MusicBrainz pass")
    args = parser.parse_args()

    if not Path(args.db).exists():
        print(f"DB not found: {args.db}")
        raise SystemExit(1)

    print(f"DB: {args.db}" + (" [dry run]" if args.dry_run else ""))
    backfill(args.db, args.dry_run, online=not args.no_online)


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run the full test suite**

```bash
python -m pytest tools/test_backfill_metadata.py -v
```

Expected: all tests PASSED (no regressions).

- [ ] **Step 3: Smoke-test the CLI help**

```bash
python tools/backfill_metadata.py --help
```

Expected output includes `--no-online` in the help text.

- [ ] **Step 4: Commit**

```bash
git add tools/backfill_metadata.py
git commit -m "feat: wire backfill_online into backfill(), add --no-online flag"
```

---

## Self-Review

**Spec coverage:**
- ✅ Two-pass structure (embedded → MusicBrainz)
- ✅ Search key: DB title+artist, then filename stem fallback
- ✅ Top result taken unconditionally
- ✅ Only missing fields written (existing values never replaced)
- ✅ Rate limit: `time.sleep(1)` between requests
- ✅ `--no-online` flag
- ✅ `musicbrainzngs` optional — graceful skip with install hint
- ✅ Network/parse errors → row skipped, loop continues
- ✅ `source` column untouched
- ✅ Per-pass summary output

**Placeholder scan:** No TBDs, no vague steps, all code blocks complete.

**Type consistency:** `backfill_online(conn, dry_run)` signature consistent across Task 3 implementation and Task 4 call site. Return type `tuple[int, int, int]` consistent throughout.
