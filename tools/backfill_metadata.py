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


def backfill(db_path: str, dry_run: bool) -> None:
    conn = sqlite3.connect(db_path)
    # Some paths were stored with non-UTF-8 encoding (e.g. latin-1 filenames).
    # surrogateescape lets us round-trip them to mutagen without crashing.
    conn.text_factory = lambda b: b.decode("utf-8", errors="surrogateescape")
    conn.row_factory = sqlite3.Row
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
    conn.close()

    suffix = " (dry run)" if dry_run else ""
    print(f"\nDone{suffix}: updated {updated}, skipped {skipped}, failed {failed}")


def main():
    default_db = Path.home() / ".config" / "MasterTweak" / "preset_builder.db"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", default=str(default_db), help="Path to preset_builder.db")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without writing")
    args = parser.parse_args()

    if not Path(args.db).exists():
        print(f"DB not found: {args.db}")
        raise SystemExit(1)

    print(f"DB: {args.db}" + (" [dry run]" if args.dry_run else ""))
    backfill(args.db, args.dry_run)


if __name__ == "__main__":
    main()
