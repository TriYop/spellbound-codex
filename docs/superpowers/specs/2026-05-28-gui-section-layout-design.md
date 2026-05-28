# GUI Section Layout Redesign

**Date:** 2026-05-28  
**Status:** Approved  
**Scope:** Layout and structure only. Custom knob/fader widgets and VU meter are separate sub-projects.

---

## Problem

The current `ParameterPanel` presents all DSP parameters in a flat layout: 7 EQ spinboxes in a row, then a line of 4 miscellaneous spinboxes (limiter ceiling, sat drive, mixbus threshold, mixbus makeup), then 7 bypass checkboxes. The mastering chain stages are not visually separated, making it hard to understand what each control belongs to or skip between stages.

---

## Solution

Replace `ParameterPanel` + `AnalysisPanel` with a new `ChainPanel` widget that:
1. Groups each DSP stage into a labelled `QGroupBox` section
2. Uses `QGroupBox::setCheckable(true)` for bypass-in-header (Qt dims/disables children when unchecked)
3. Shows relevant analysis readouts inline in each section (removing the need for the separate `AnalysisPanel`)
4. Arranges sections in a 3-tier grid

---

## Window Structure

```
┌──────────────────────────────────────────────────────────┐
│ [Open audio…] filename.wav    Preset: [Rock ▾] [Open…]   │
├──────────────────────────────────────────────────────────┤
│  ┌✔ EQ ───────────────────────────────────────────────┐  │
│  │  Sub      Lows    LMid    Mids   HMid  Highs   Air │  │
│  │ -18→-21  -14→-16  ...                             │  │
│  │  [0.0]    [0.0]   [0.0]  [0.0]  [0.0] [0.0]  [0.0]│  │
│  └────────────────────────────────────────────────────┘  │
│  ┌✔ MB Comp ───────┐ ┌✔ Width ──────┐ ┌✔ Saturator ──┐  │
│  │ Crest: 8.2 dB   │ │ Corr: 0.95  │ │ Crest: 8.2dB │  │
│  │ (advice-driven) │ │ (advice-     │ │ Drive: [0.0] │  │
│  └─────────────────┘ │  driven)     │ └──────────────┘  │
│                      └──────────────┘                    │
│  ┌✔ Mixbus Comp ─────────────────┐ ┌✔ Limiter ────────┐ │
│  │ Thr: [-20.0]  Mkup: [0.0]    │ │ Ceil: [-1.0 dBTP]│ │
│  │ rms: -18.2 dBFS               │ │ peak: -1.2 dBFS  │ │
│  └────────────────────────────── ┘ └──────────────────┘ │
├──────────────────────────────────────────────────────────┤
│ ▶ Play  [Source: Original]  Target:[Spotify▾] [Render…] │
│ 16/24/32 ▾  □ FLAC          [Save As…]  Ready           │
└──────────────────────────────────────────────────────────┘
```

---

## File Changes

| Action | File | Change |
|--------|------|--------|
| Create | `gui/ChainPanel.h` | New widget: grid + 6 sections |
| Create | `gui/ChainPanel.cpp` | Implementation |
| Delete | `gui/ParameterPanel.h` | Replaced by ChainPanel |
| Delete | `gui/ParameterPanel.cpp` | Replaced by ChainPanel |
| Delete | `gui/AnalysisPanel.h` | Inline data replaces it |
| Delete | `gui/AnalysisPanel.cpp` | Inline data replaces it |
| Modify | `gui/MainWindow.h` | Replace ParameterPanel*/AnalysisPanel* with ChainPanel* |
| Modify | `gui/MainWindow.cpp` | Wire ChainPanel in place of old widgets |
| Modify | `gui/CMakeLists.txt` | Add ChainPanel.cpp, remove ParameterPanel.cpp + AnalysisPanel.cpp |

---

## ChainPanel Interface

```cpp
// gui/ChainPanel.h
class ChainPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChainPanel(QWidget* parent = nullptr);

    // Populate controls from advice + analysis + preset.
    // adviceSet is the advised baseline; controls are initialised to these
    // values and tinted blue if user subsequently edits them.
    void setAdvice(const mt::AdviceSet& advice,
                   const mt::AnalysisSnapshot& snap,
                   const mt::PresetData& preset);

    // Returns the current AdviceSet (baseline + any user overrides).
    mt::AdviceSet currentAdvice() const;

    // Writes bypass checkbox states into opts. Call before rendering.
    void populateBypassFlags(mt::RenderOptions& opts) const;

    // Reset all overrides to the last-advised baseline.
    void resetToAdvice();

signals:
    // Emitted whenever any spinbox value OR bypass checkbox changes.
    void overrideChanged(const mt::AdviceSet& advice);
};
```

This is a drop-in replacement for `ParameterPanel`: same `setAdvice()` / `resetToAdvice()` / `overrideChanged()` pattern. `MainWindow` switches the type with no logic changes.

---

## Section Specifications

### EQ Section (full width, Tier 1)

- `QGroupBox` title: `"EQ"`, checkable (checked = EQ active)
- Unchecked maps to `RenderOptions::bypassEq = true`
- 7 vertical columns, one per band
- Per-band column (top to bottom):
  1. `QLabel` — band name (`"Sub"`, `"Lows"`, …)
  2. `QLabel` — analysis readout: `"−18→−21 dB"` (preset.bandRmsDb[i] → snap.bands[i].avgRmsDb). Format: `"%.0f→%.0f"`. Hidden / shows `"—"` before analysis.
  3. `QDoubleSpinBox` — EQ gain, range −12.0…+12.0 dB, step 0.5, 1 decimal. Tinted `#d0e8ff` when dirty.

### Multiband Comp Section (Tier 2, column 1)

- `QGroupBox` title: `"Multiband Comp"`, checkable
- Unchecked = `bypassMbComp = true`
- Content: one `QLabel` showing `"Crest: X.X dB"` (average crest factor across all 7 bands from `snap.bands[i].crestDb`). Shows `"—"` before analysis.
- Below: `QLabel "Advice-driven"` (greyed out, explains no manual params)

### Stereo Width Section (Tier 2, column 2)

- `QGroupBox` title: `"Stereo Width"`, checkable
- Unchecked = `bypassWidth = true`
- Content: `QLabel` showing `"Corr: X.XX"` (snap.overallCorr). Shows `"—"` before analysis.
- Below: `QLabel "Advice-driven"`

### Saturator Section (Tier 2, column 3)

- `QGroupBox` title: `"Saturator"`, checkable
- Unchecked = `bypassSaturator = true`
- Content:
  - `QLabel` showing `"Crest: X.X dB"` (same avg crest as MB Comp section)
  - `QLabel "Drive:"` + `QDoubleSpinBox` (range 0.0…6.0 dB, step 0.5, 1 decimal). Tinted when dirty.

### Mixbus Comp Section (Tier 3, left ~60% width)

- `QGroupBox` title: `"Mixbus Comp"`, checkable
- Unchecked = `bypassMixbusComp = true`
- Content (horizontal):
  - `QLabel "Thr:"` + `QDoubleSpinBox` (−40…0 dB, step 1.0). Tinted when dirty.
  - `QLabel "Mkup:"` + `QDoubleSpinBox` (−12…+12 dB, step 0.5). Tinted when dirty.
  - `QLabel` showing `"rms: −XX.X dBFS"` (snap.overallAvgDb). Shows `"—"` before analysis.

### Limiter Section (Tier 3, right ~40% width)

- `QGroupBox` title: `"Limiter"`, checkable
- Unchecked = `bypassLimiter = true`
- Content:
  - `QLabel "Ceil:"` + `QDoubleSpinBox` (−6…0 dBTP, step 0.5). Tinted when dirty.
  - `QLabel` showing `"peak: −X.X dBFS"` (snap.overallPeakDb). Shows `"—"` before analysis.

**Note:** Dither bypass is removed from the visible UI (it has no user-facing parameters and was a rarely-touched expert option). `RenderOptions::bypassDither` defaults to `false` and is not exposed.

---

## Dirty Tracking

Each `QDoubleSpinBox` is tinted `background: #d0e8ff` when its current value differs from the last-advised baseline (same as the current `ParameterPanel` behaviour). Tint is cleared by `resetToAdvice()` or by `setAdvice()` on re-analysis.

---

## Grid Layout

Use a `QGridLayout` for the outer 3-tier structure in `ChainPanel`:

```
Row 0: EQ section,               columnSpan=6
Row 1: MB Comp (col 0–1), Width (col 2–3), Saturator (col 4–5)
Row 2: Mixbus (col 0–3),  Limiter (col 4–5)
```

The `QGridLayout` column stretches ensure Mixbus gets ~60% and Limiter ~40% in Tier 3.

---

## MainWindow Changes

- Replace `ParameterPanel* paramPanel_` with `ChainPanel* chainPanel_`
- Replace `AnalysisPanel* analysisPanel_` with nothing (deleted)
- In `onAnalysisDone()`: call `chainPanel_->setAdvice(result.advice, result.analysis, preset_)` instead of calling both `paramPanel_->setAdvice()` and `analysisPanel_->setAnalysis()`
- In `onResetClicked()`: call `chainPanel_->resetToAdvice()`
- Connect `chainPanel_->overrideChanged` → same handler as before
- When building `RenderOptions` for rendering, call `chainPanel_->populateBypassFlags(opts)` instead of reading individual `ParameterPanel` checkbox states

---

## Testing

No unit tests for Qt widgets. Manual verification checklist:

1. Build clean: `cmake --build build --parallel`
2. Launch GUI: `./build/gui/MasterTweak`
3. Open an audio file → sections show `"—"` analysis readouts
4. Select a preset → sections still show `"—"`
5. Click Render (or re-analyse) → EQ inline labels populate with `"X→Y dB"` values; crest/corr/rms/peak readouts populate
6. Edit an EQ spinbox → it turns blue; `overrideChanged` fires
7. Click Reset → spinboxes return to advised values; blue tint clears
8. Uncheck `"✔ EQ"` → EQ section dims; render with EQ bypassed
9. Uncheck `"✔ Saturator"` → drive spinbox dims; render with saturator bypassed
10. Existing render pipeline tests still pass: `ctest --test-dir build --output-on-failure`
