#!/usr/bin/env python3
"""Backfill genre/year (and missing title/artist/album) from embedded audio tags."""

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


def backfill_online(conn: sqlite3.Connection, dry_run: bool) -> tuple[int, int, int]:
    """Pass 2: query MusicBrainz for rows still missing fields.

    Returns (updated, skipped, failed). failed is always 0 — search errors are counted as skipped.
    """
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


def extract_tags(path: str) -> dict:
    """Read embedded tags with mutagen. Returns {} if file unreadable."""
    try:
        import mutagen
        f = mutagen.File(path, easy=True)
        if f is None:
            return {}
        tags = {}
        for key in ("title", "artist", "album", "genre"):
            val = f.get(key)
            if val:
                tags[key] = val[0]  # EasyID3/EasyMP3 returns lists
        date = f.get("date")
        if date:
            try:
                tags["year"] = int(str(date[0])[:4])
            except (ValueError, IndexError):
                pass
        return tags
    except Exception:
        return {}


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
