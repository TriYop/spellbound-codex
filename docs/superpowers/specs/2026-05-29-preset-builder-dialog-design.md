# Preset Builder Dialog — Design Spec (Sub-project B)

**Date:** 2026-05-29
**Feature:** Qt6 Preset Builder dialog — ingests audio tracks into the shared SQLite library, browses/tags them, and creates + exports MixAdvice-compatible preset XMLs.

---

## Overview

A non-modal `QDialog` opened via a **"Manage presets…" button** added to `PresetSelector`. It contains a `QTabWidget` with three tabs: **Ingest**, **Browse / Tag**, and **Create Preset**.

The dialog depends on `preset_builder_core` (Sub-project A, already implemented). The only new Qt-specific piece is `AcoustIdMetadataProvider`, which implements the `pb::MetadataProvider` port using `QProcess` (fpcalc) and `QNetworkAccessManager` (AcoustID API).

---

## Architecture

### Shared context

```cpp
// gui/PresetBuilderCtx.h  — plain struct, no QObject
struct PresetBuilderCtx {
    pb::Database               db;       // ~/.config/MixAdvice/preset_builder.db
    pb::SqliteTrackRepository  tracks;
    pb::SqlitePresetRepository presets;
    AcoustIdMetadataProvider   metadata; // best-effort; silent fallback if fpcalc absent
};
```

`PresetBuilderCtx` is constructed once by `PresetBuilderDialog`, which passes the DB path (`QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)` resolved to `~/.config/MixAdvice/preset_builder.db`) at construction time. All three tab widgets receive a `PresetBuilderCtx&` in their constructor.

### Worker threads

Follow the existing `QThread` subclass pattern (`AnalysisWorker` / `RenderWorker` in `MainWindow`):

- **`IngestWorker`** — runs `pb::IngestService::ingest()` off the UI thread. Emits `progress(float, QString)` and `finished(pb::IngestReport)`.
- **`StatsWorker`** — runs `pb::StatsService::compute()` off the UI thread. Emits `finished(pb::PresetStats)`. Triggered whenever the Create Preset tab's track selection changes (300 ms debounce).

### Cross-tab communication

`PresetBuilderDialog` owns the signal routing:

- `IngestTab::libraryChanged()` → `BrowseTab::refresh()`, `CreatePresetTab::refreshTrackList()`
- `BrowseTab::libraryChanged()` (emitted after delete) → `CreatePresetTab::refreshTrackList()`
- `CreatePresetTab::presetExported(QString path)` → `PresetBuilderDialog::onPresetExported()` → emits `PresetBuilderDialog::presetExported(QString path)` → `MainWindow` refreshes `PresetSelector`

### PresetSelector integration

`PresetSelector` gains a **"Manage presets…" `QPushButton`**. `PresetBuilderDialog` is lazy-constructed on first click and reused on subsequent clicks (`show()` / `raise()`). `MainWindow` connects `PresetBuilderDialog::presetExported` to a slot that calls `presetSelector_->populate(execDir)`.

---

## IngestTab

**Top — input:**
- **"Add folder…" button** opens `QFileDialog::getExistingDirectory`; passes the chosen path to `IngestWorker`.
- **Drag-and-drop zone** (`QLabel` styled as a bordered drop area). Accepts `dragEnterEvent` for URLs; `dropEvent` extracts the first dropped path (file or directory) and passes it to `IngestWorker`. Displays "Drop a folder or files here" when idle.

**Middle — progress:**
- `QProgressBar` (0–100, hidden when idle).
- `QLabel` stage description (e.g. "Ingesting: track.wav").
- "Cancel" `QPushButton` calls `IngestWorker::requestInterruption()`. `IngestService::ingest()` checks `QThread::currentThread()->isInterruptionRequested()` between files.

**Bottom — last report:**
- Three `QLabel` counters: "Added: N", "Skipped: N", "Failed: N".
- `QListWidget` of failed paths + messages (hidden when empty).

When `IngestWorker::finished()` fires, `IngestTab` updates the report labels and emits `libraryChanged()`.

---

## BrowseTab

**Top — search:**
Three `QLineEdit` fields: Title, Artist, Genre. Each `textChanged` triggers a 200 ms `QTimer` debounce; on timeout, calls `ctx_.tracks.search(filter)` and repopulates the table.

**Center — track table (`QTableWidget`):**

| Column | Editable | Notes |
|--------|----------|-------|
| Title | Yes | double-click to edit |
| Artist | Yes | |
| Album | Yes | |
| Genre | Yes | |
| Year | Yes | integer |
| Source | No | "AcoustID" or "filename" badge |
| Path | No | truncated; full path in tooltip |

`itemChanged` fires `SqliteTrackRepository::save()` with the updated `Track` (only the changed row is re-read from the table and saved — no full reload).

**Bottom toolbar:**
- "Delete selected" button — `QMessageBox` confirmation; removes checked rows via `SqliteTrackRepository::remove()`; emits `libraryChanged()`.
- Row count label: "N tracks".

`BrowseTab::refresh()` re-runs the current filter and repopulates the table. Called after ingest, after delete, and on dialog open.

---

## CreatePresetTab

**Top — preset identity:**
- `QLineEdit` Name (required — Export buttons disabled until non-empty).
- `QLineEdit` Description (optional).

**Center — track selection:**
- Three `QLineEdit` search fields (Title/Artist/Genre), same debounce pattern as BrowseTab.
- `QTableWidget` with a checkbox column prepended to the same columns as BrowseTab (minus Source/Path for brevity). "Select All" / "Deselect All" buttons above.
- Selection state is maintained in a `std::set<pb::TrackId>` — checked tracks that scroll out of view or don't match the current filter are remembered and re-checked when they return.

**Right panel — stats preview:**
- Displays `pb::PresetStats` for the currently checked tracks.
- 7 band rows (Sub → Air) showing `bandRmsDb`, `bandCorrMin`, `bandTransientDb`.
- Overall RMS and overall correlation min.
- Updated by `StatsWorker` with a 300 ms debounce on selection change. Shows "—" while computing or when no tracks are checked.

**Bottom — export:**

| Button | Behaviour |
|--------|-----------|
| "Export to Presets folder" | Writes XML to `~/.config/MixAdvice/Presets/<sanitised-name>.xml`; emits `presetExported(path)`; `PresetBuilderDialog` relays to `MainWindow` → `PresetSelector::populate()` |
| "Save As…" | `QFileDialog::getSaveFileName`; writes XML to chosen path; no auto-refresh |

Both buttons disabled until Name is non-empty and at least one track is checked. Name is sanitised to a filename-safe stem using a free function `sanitizePresetName(std::string)` extracted from `MainWindow` into a new `gui/utils.h` header (shared by `MainWindow` and `CreatePresetTab`).

After export, a `QLabel` status line shows "Exported to: \<path\>".

---

## AcoustIdMetadataProvider

Implements `pb::MetadataProvider` in the GUI layer.

`lookup(audioFilePath)` runs on the `IngestWorker` thread (synchronous from its perspective):

1. **Fingerprint:** `QProcess::execute("fpcalc", {"-json", path})`. If `fpcalc` is not on `PATH` or exits non-zero → return `std::nullopt` silently.
2. **Parse fingerprint:** `QJsonDocument::fromJson(stdout)` → extract `fingerprint` and `duration`.
3. **AcoustID lookup:** Create a fresh `QNetworkAccessManager` on the calling thread. POST to `https://api.acoustid.org/v2/lookup` with parameters `client=<API_KEY>`, `fingerprint=<fp>`, `duration=<dur>`, `meta=recordings+releasegroups`. The API key is a compile-time constant defined in `AcoustIdMetadataProvider.cpp` — a free application key registered at acoustid.org. Spin a `QEventLoop` until `QNetworkReply::finished` or a 5 s `QTimer` timeout. On any error → return `std::nullopt` silently.
4. **Parse response:** Take the first result's first recording. Populate `TrackMetadata` (title, artist, year if present). Set `source = pb::MetadataSource::acoustid`. Return it.

A fresh `QNetworkAccessManager` per call avoids thread-affinity issues. Network calls are infrequent (one per new track) so the overhead is negligible.

---

## File Layout

```
gui/
  PresetBuilderCtx.h             — plain struct (no .cpp)
  PresetBuilderDialog.h/.cpp     — QDialog; owns ctx; wires tabs and signals
  IngestTab.h/.cpp               — IngestTab widget + IngestWorker QThread subclass
  BrowseTab.h/.cpp               — BrowseTab widget (search, table, inline edit, delete)
  CreatePresetTab.h/.cpp         — CreatePresetTab widget + StatsWorker QThread subclass
  AcoustIdMetadataProvider.h/.cpp — MetadataProvider impl (fpcalc + Qt HTTP)
```

`gui/CMakeLists.txt` adds the 7 new source files and links `preset_builder_core` to the `MasterTweak` target.

`PresetSelector.h/.cpp` gains a "Manage presets…" `QPushButton` member and a `manageRequested()` signal (or direct slot connection to `PresetBuilderDialog::show()`).

`MainWindow.h/.cpp` lazy-constructs `PresetBuilderDialog` and connects `presetExported` → `presetSelector_->populate(execDir)`.

---

## Testing

No new unit tests — `preset_builder_core` logic is already covered by doctest tests. Qt layer verified by manual testing:

- Ingest a folder (drag-and-drop and button) → tracks appear in Browse tab
- Edit a title inline → persists after closing and reopening the dialog
- Check tracks in Create Preset → stats preview updates
- Export → preset XML appears in `~/.config/MixAdvice/Presets/` and PresetSelector dropdown refreshes
- fpcalc absent → ingest completes silently with filename metadata (no error shown)
- Cancel ingest mid-way → progress stops, partial results reported
