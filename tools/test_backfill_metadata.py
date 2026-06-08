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
