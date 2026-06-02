# Export Advice to Markdown — Design Spec

**Date:** 2026-06-02

## Goal

Add an "Export Advice…" button to the render row. Clicking it saves a human-readable Markdown file containing both the measured analysis values and the derived (or user-overridden) mastering parameters, so the user can apply equivalent settings manually in a DAW.

## New files

| File | Purpose |
|------|---------|
| `core/include/mastertweak/report.hpp` | Declares `formatAdviceMarkdown()` |
| `core/src/report.cpp` | Implements the formatter |

## `formatAdviceMarkdown()` signature

```cpp
// core/include/mastertweak/report.hpp
#pragma once
#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/preset.hpp"
#include <string>

namespace mt {

std::string formatAdviceMarkdown(
    const AnalysisSnapshot& snap,
    const AdviceSet&        advice,
    const PresetData&       preset,
    const std::string&      inputFilename);

} // namespace mt
```

Pure function, no side effects. Returns the full Markdown document as a `std::string`.

## Output document structure

```
# MasterTweak Advice Export

**File:** <inputFilename>
**Preset:** <preset.name>
**Date:** <YYYY-MM-DD>

> <preset.description>

## Analysis — Measured Values

| Band    | RMS (dBFS) | Crest (dB) | L/R Corr |
| ------- | ---------- | ---------- | -------- |
| Sub     | -24.5      | 8.2        | 0.92     |
| …       |            |            |          |

**Overall:** RMS -22.1 dBFS · Correlation 0.88

## 7-Band EQ

| Band    | Freq (Hz) | Gain (dB) | Q    | Type  |
| ------- | --------- | --------- | ---- | ----- |
| Sub     | 50        | +2.5      | 0.71 | Shelf |
| …       |           |           |      |       |

## Multiband Compression

| Band    | Threshold (dB) | Ratio | Attack (ms) | Release (ms) |
| ------- | -------------- | ----- | ----------- | ------------ |
| Sub     | -27.0          | 2.0   | 30          | 200          |
| …       |                |       |             |              |

## Stereo Width

| Band    | Width |
| ------- | ----- |
| Sub     | 0.80  |
| …       |       |

## Mixbus Compressor

| Threshold (dB) | Ratio | Attack (ms) | Release (ms) | Makeup (dB) |
| -------------- | ----- | ----------- | ------------ | ----------- |
| -18.0          | 2.5   | 15          | 120          | +3.2        |

## Saturation

**Drive:** 2.1 dB

## Limiter

**Target:** -14.0 LUFS · **True-Peak Ceiling:** -1.0 dBTP
```

Numbers are formatted to one decimal place. Gain signs are explicit (`+` / `-`).

## GUI changes (`gui/MainWindow.cpp` / `gui/MainWindow.h`)

- Add `QPushButton* exportAdviceBtn_` member.
- Insert button into the render row between Save As and the bit-depth controls.
- `exportAdviceBtn_->setEnabled(false)` initially; enabled at the same time as `renderButton_` (i.e., in `onAnalysisFinished()` on success).
- `onExportAdvice()` slot:
  1. Build default path: `<input-stem>_advice.md` in the same directory as the input file.
  2. Open `QFileDialog::getSaveFileName` filtered to `*.md`.
  3. Call `mt::formatAdviceMarkdown(snap_, chainPanel_->currentAdvice(), *currentPreset_, inputPath_.toStdString())`.
  4. Write the returned string to the chosen path; show `QMessageBox::warning` on failure.
- Add `mt::AnalysisSnapshot lastSnap_` member to `MainWindow`. Both `onAnalysisFinished()` and `onRenderFinished()` already receive a `MasterResult` (which carries `.analysis`); save `result.analysis` into `lastSnap_` there.

## Build changes

- `core/CMakeLists.txt`: add `src/report.cpp` to `mastertweak_core` sources.

## Scope

- No CLI changes.
- No test additions (pure string formatting; the output is visually verified).
