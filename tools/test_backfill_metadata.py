import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).parent))

from backfill_metadata import parse_filename_stem, search_musicbrainz


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

    # Row has genre and year, so it won't even be in the query results
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
