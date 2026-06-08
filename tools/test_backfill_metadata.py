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
