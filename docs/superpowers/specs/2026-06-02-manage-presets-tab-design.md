# Manage Presets Tab — Design Spec

**Date:** 2026-06-02
**Feature:** 4th tab in `PresetBuilderDialog` for reviewing, auditing, and cleaning up existing presets.

---

## Purpose

Users need to inspect previously created presets, see which tracks belong to each one, spot outlier tracks (via per-band deviation colouring), remove wrongly grouped tracks, and delete entire presets. This is the feedback loop that precedes future auto-discover metric calibration.

---

## Layout

Horizontal splitter inside the tab (left 1/3 / right 2/3):

```
Left: preset list                 Right: track table
──────────────────────────────────────────────────────────────────────
Rock 2010s       12 tracks   ◀    Title   Artist  Genre  Sub  Lows  Lo-Mid  Mids  Hi-Mid  Highs  Air  [✕]
Hip-Hop 90s       8 tracks        Track1  ArtistA  Rock  +0.3  -1.2  +0.1   -0.4   +0.2   +0.8  +1.1  [✕]
Electronic        5 tracks        Track2  ArtistB  Rock  +4.1  +0.8  -3.2   +2.1   -0.3   -0.1  +0.2  [✕]

[Delete Preset]
```

**Left panel:** `QListWidget` — each item shows `{preset name} ({N} tracks)`. Selecting an item populates the right panel. "Delete Preset" button below the list, disabled when nothing is selected.

**Right panel:** `QTableWidget` with 11 columns:

| # | Column | Width |
|---|--------|-------|
| 0 | Title | Stretch |
| 1 | Artist | Stretch |
| 2 | Genre | Fixed |
| 3–9 | Sub / Lows / Lo-Mid / Mids / Hi-Mid / Highs / Air | Fixed (55 px each) |
| 10 | [✕] | Fixed (30 px) |

All cells read-only; [✕] column contains a `QPushButton` per row.

---

## Deviation Metric and Colouring

Each of the 7 band cells shows the **signed spectral shape deviation** of that track from the preset mean:

```
track_shape[i]  = track.analysis.bandRmsDb[i]  − track.analysis.overallRmsDb
preset_shape[i] = presetStats.bandRmsDb[i]      − presetStats.overallRmsDb
deviation[i]    = track_shape[i] − preset_shape[i]          (signed, dB)
```

Cell text: signed value to 1 decimal place (e.g. `+3.2`, `-1.5`).
Cell background colour by `|deviation|`:

| Range | Colour | RGB |
|-------|--------|-----|
| < 2 dB | Green | (100, 200, 100) |
| 2–5 dB | Yellow | (220, 200, 80) |
| ≥ 5 dB | Red | (220, 80, 80) |

---

## New Files

- `gui/ManagePresetsTab.h`
- `gui/ManagePresetsTab.cpp`

## Modified Files

- `gui/PresetBuilderDialog.h` / `.cpp` — add 4th tab; connect incoming `presetExported` signals from `CreatePresetTab` and `AutoDiscoverDialog` to `ManagePresetsTab::refresh()`
- `gui/CMakeLists.txt` — add new sources

---

## Class Interface

```cpp
class ManagePresetsTab : public QWidget {
    Q_OBJECT
public:
    explicit ManagePresetsTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

public slots:
    void refresh();   // repopulate preset list; called on showEvent + after external saves

signals:
    void presetExported();   // wired to PresetBuilderDialog::presetExported()

protected:
    void showEvent(QShowEvent*) override;

private slots:
    void onPresetSelected(int row);
    void onRemoveTrack(int trackRow);
    void onDeletePreset();

private:
    void populateTrackTable(const pb::Preset& preset,
                            const pb::PresetStats& stats,
                            const std::vector<pb::Track>& tracks);
    QColor deviationColor(float deviationDb) const;

    PresetBuilderCtx& ctx_;
    QListWidget*  presetList_ = nullptr;
    QPushButton*  deleteBtn_  = nullptr;
    QTableWidget* trackTable_ = nullptr;
    pb::Preset    currentPreset_;   // in-memory working copy for track removal

    static constexpr int kColTitle  = 0;
    static constexpr int kColArtist = 1;
    static constexpr int kColGenre  = 2;
    static constexpr int kColSub    = 3;   // bands occupy columns 3–9
    static constexpr int kColRemove = 10;
};
```

---

## Data Flow

### Tab shown / `refresh()` called
1. `ctx_.presets.listAll()` → repopulate left list (preserve selection if still valid)
2. Auto-select first item → triggers `onPresetSelected(0)`

### Preset selected
1. `ctx_.presets.tracksFor(preset.id)` → `std::vector<Track>`
2. `StatsService::compute(tracks)` → `PresetStats`
3. `populateTrackTable(preset, stats, tracks)` — fills cells with deviation values and colours

### [✕] — Remove track from preset
1. Remove `trackId` from `currentPreset_.trackIds`
2. `ctx_.presets.save(currentPreset_)` → DB update
3. Reload tracks: `ctx_.presets.tracksFor(currentPreset_.id)`
4. Recompute stats: `StatsService::compute(tracks)`
5. Re-export XML: `ExportService::exportXml(currentPreset_, stats, defaultPath(currentPreset_.name))`
   — path = `~/.config/MixAdvice/Presets/{sanitizePresetName(name)}.xml`
6. `populateTrackTable(...)` with updated deviation colours
7. Update left-list item text (track count changed)
8. Emit `presetExported()` so `MainWindow` refreshes its preset selector

### Delete Preset
1. `QMessageBox::question` — "Delete preset '{name}'? This cannot be undone."
2. `ctx_.presets.remove(preset.id)` → cascading DB delete
3. Remove item from left list, clear right panel, disable delete button
4. Emit `presetExported()`

---

## Refresh Coordination

`ManagePresetsTab::refresh()` is called when:
- The tab becomes visible (`showEvent`)
- `CreatePresetTab::presetExported` fires (new preset created)
- `AutoDiscoverDialog::presetSaved` fires (preset saved from auto-discover)

Wired in `PresetBuilderDialog::buildUi()`.

---

## Auto-Save Path Note

Re-export on track removal always writes to `~/.config/MixAdvice/Presets/{sanitized_name}.xml` — the same default path as `CreatePresetTab`. Presets originally exported via "Save As…" to a custom path are not re-exported there; the custom path is not persisted in the DB schema.

---

## No New Tests

The tab is pure UI glue — it delegates to `StatsService`, `ExportService`, and the repositories which are already tested. No new unit tests required.
