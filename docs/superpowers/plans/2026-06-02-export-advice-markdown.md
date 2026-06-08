# Export Advice to Markdown — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a core `formatAdviceMarkdown()` function and an "Export Advice…" button in the main window render row that saves a human-readable Markdown file of measured analysis values plus mastering parameters.

**Architecture:** A pure free function `mt::formatAdviceMarkdown()` lives in `core/` (no Qt, no side effects). The GUI button calls it after file dialog confirmation and writes the result to disk. `MainWindow` stores `lastSnap_` populated from `MasterResult::analysis` on every analysis/render completion.

**Tech Stack:** C++20, `<sstream>`, `<iomanip>`, `<ctime>`, Qt6 (`QFileDialog`, `QFile`).

---

## File Map

| Action | Path |
|--------|------|
| Create | `core/include/mastertweak/report.hpp` |
| Create | `core/src/report.cpp` |
| Modify | `core/CMakeLists.txt` |
| Modify | `gui/MainWindow.h` |
| Modify | `gui/MainWindow.cpp` |

---

### Task 1: Core formatter — header

**Files:**
- Create: `core/include/mastertweak/report.hpp`

- [ ] **Step 1: Create the header**

```cpp
// core/include/mastertweak/report.hpp
#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/preset.hpp"

#include <string>

namespace mt {

// Format a human-readable Markdown document from analysis + advice + preset.
// inputFilename is only used as a display label in the document header.
std::string formatAdviceMarkdown(
    const AnalysisSnapshot& snap,
    const AdviceSet&        advice,
    const PresetData&       preset,
    const std::string&      inputFilename);

} // namespace mt
```

- [ ] **Step 2: Add `report.cpp` to `core/CMakeLists.txt`**

In `core/CMakeLists.txt`, add `src/report.cpp` after `src/codec_correction.cpp`:

```cmake
add_library(mastertweak_core STATIC
    src/version.cpp
    src/io.cpp
    src/analysis.cpp
    src/preset.cpp
    src/advice.cpp
    src/pipeline.cpp
    src/target_level.cpp
    src/codec_correction.cpp
    src/report.cpp
    src/dsp/linkwitz_riley.cpp
    src/dsp/parametric_eq.cpp
    src/dsp/compressor.cpp
    src/dsp/multiband_comp.cpp
    src/dsp/mixbus_comp.cpp
    src/dsp/saturator.cpp
    src/dsp/stereo_width.cpp
    src/dsp/limiter.cpp
    src/dsp/dither.cpp
    src/dsp/lufs_analyser.cpp
    src/dsp/gain_stager.cpp
)
```

---

### Task 2: Core formatter — implementation

**Files:**
- Create: `core/src/report.cpp`

- [ ] **Step 1: Create `report.cpp`**

```cpp
#include "mastertweak/report.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace mt {

namespace {

std::string today() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d");
    return oss.str();
}

// Format a float with explicit +/- sign (for gain/threshold values).
std::string fmtDb(float v, int prec = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec);
    if (v >= 0.f) oss << '+';
    oss << v;
    return oss.str();
}

// Format a plain float (no sign prefix).
std::string fmtF(float v, int prec = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

} // namespace

std::string formatAdviceMarkdown(
    const AnalysisSnapshot& snap,
    const AdviceSet&        advice,
    const PresetData&       preset,
    const std::string&      inputFilename)
{
    std::ostringstream md;

    // ── Header ────────────────────────────────────────────────────────────────
    md << "# MasterTweak Advice Export\n\n";
    md << "**File:** " << inputFilename << "  \n";
    md << "**Preset:** " << preset.name << "  \n";
    md << "**Date:** " << today() << "\n\n";
    if (!preset.description.empty())
        md << "> " << preset.description << "\n\n";

    // ── Analysis ──────────────────────────────────────────────────────────────
    md << "## Analysis — Measured Values\n\n";
    md << "| Band    | RMS (dBFS) | Crest (dB) | L/R Corr |\n";
    md << "| ------- | ---------- | ---------- | -------- |\n";
    for (int i = 0; i < AnalysisSnapshot::kNumBands; ++i) {
        const auto& b = snap.bands[i];
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(10) << fmtDb(b.avgRmsDb)
           << " | " << std::setw(10) << fmtF(b.crestDb)
           << " | " << std::setw(8)  << fmtF(b.correlation, 2) << " |\n";
    }
    md << "\n**Overall:** RMS " << fmtDb(snap.overallAvgDb)
       << " dBFS · Correlation " << fmtF(snap.overallCorr, 2) << "\n\n";

    // ── EQ ────────────────────────────────────────────────────────────────────
    md << "## 7-Band EQ\n\n";
    md << "| Band    | Freq (Hz) | Gain (dB) | Q    | Type  |\n";
    md << "| ------- | --------- | --------- | ---- | ----- |\n";
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto& eq = advice.eq[i];
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(9) << fmtF(eq.freqHz, 0)
           << " | " << std::setw(9) << fmtDb(eq.gainDb)
           << " | " << std::setw(4) << fmtF(eq.q, 2)
           << " | " << std::left  << std::setw(5) << (eq.isShelf ? "Shelf" : "Bell") << " |\n";
    }
    md << "\n";

    // ── Multiband comp ────────────────────────────────────────────────────────
    md << "## Multiband Compression\n\n";
    md << "| Band    | Threshold (dB) | Ratio | Attack (ms) | Release (ms) |\n";
    md << "| ------- | -------------- | ----- | ----------- | ------------ |\n";
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto& c = advice.mbComp[i];
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(14) << fmtDb(c.thresholdDb)
           << " | " << std::setw(5)  << fmtF(c.ratio, 1)
           << " | " << std::setw(11) << fmtF(c.attackMs, 0)
           << " | " << std::setw(12) << fmtF(c.releaseMs, 0) << " |\n";
    }
    md << "\n";

    // ── Stereo width ──────────────────────────────────────────────────────────
    md << "## Stereo Width\n\n";
    md << "| Band    | Width |\n";
    md << "| ------- | ----- |\n";
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
           << " | " << std::right << std::setw(5) << fmtF(advice.width[i].width, 2) << " |\n";
    }
    md << "\n";

    // ── Mixbus comp ───────────────────────────────────────────────────────────
    const auto& mb = advice.mixbusComp;
    md << "## Mixbus Compressor\n\n";
    md << "| Threshold (dB) | Ratio | Attack (ms) | Release (ms) | Makeup (dB) |\n";
    md << "| -------------- | ----- | ----------- | ------------ | ----------- |\n";
    md << "| " << std::right << std::setw(14) << fmtDb(mb.thresholdDb)
       << " | " << std::setw(5)  << fmtF(mb.ratio, 1)
       << " | " << std::setw(11) << fmtF(mb.attackMs, 0)
       << " | " << std::setw(12) << fmtF(mb.releaseMs, 0)
       << " | " << std::setw(11) << fmtDb(mb.makeupDb) << " |\n\n";

    // ── Saturator ─────────────────────────────────────────────────────────────
    md << "## Saturation\n\n";
    md << "**Drive:** " << fmtF(advice.saturator.driveDb) << " dB\n\n";

    // ── Limiter ───────────────────────────────────────────────────────────────
    md << "## Limiter\n\n";
    md << "**Target:** " << fmtF(advice.limiter.targetLufsApprox) << " LUFS"
       << " · **True-Peak Ceiling:** " << fmtDb(advice.limiter.ceilingDb) << " dBTP\n";

    return md.str();
}

} // namespace mt
```

- [ ] **Step 2: Build to verify core compiles**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
cmake --build build --parallel 2>&1 | tail -10
```

Expected: no errors, `mastertweak_core` rebuilt with `report.cpp.o`.

- [ ] **Step 3: Commit**

```bash
git add core/include/mastertweak/report.hpp core/src/report.cpp core/CMakeLists.txt
git commit -m "feat: formatAdviceMarkdown() in core — analysis + advice → markdown"
```

---

### Task 3: GUI — MainWindow wiring

**Files:**
- Modify: `gui/MainWindow.h`
- Modify: `gui/MainWindow.cpp`

- [ ] **Step 1: Add members and slot to `MainWindow.h`**

Add `#include "mastertweak/analysis.hpp"` is already present (line 5). Add to the `private slots:` block (after `onManagePresets`):

```cpp
    void onExportAdvice();
```

Add to the `// ── State ──` block (after `currentPreset_`):

```cpp
    mt::AnalysisSnapshot          lastSnap_;
```

Add to the `// ── Widgets ──` block (after `saveButton_`):

```cpp
    QPushButton*      exportAdviceBtn_ = nullptr;
```

- [ ] **Step 2: Add `#include` and button construction in `MainWindow.cpp`**

At the top of `MainWindow.cpp`, add after the existing includes:

```cpp
#include "mastertweak/report.hpp"
```

Also add `<QFile>` to the Qt includes block:

```cpp
#include <QFile>
```

In `buildUi()`, inside the render row block, add `exportAdviceBtn_` after `saveButton_` is constructed and before `connect(renderButton_…)`:

```cpp
        exportAdviceBtn_ = new QPushButton(QString::fromUtf8("Export Advice\xe2\x80\xa6"), central);
        exportAdviceBtn_->setEnabled(false);
```

Add its connection just after the existing `connect(saveButton_…)` line:

```cpp
        connect(exportAdviceBtn_, &QPushButton::clicked, this, &MainWindow::onExportAdvice);
```

Replace the existing render-row widget additions with:

```cpp
        row->addWidget(renderButton_);
        row->addWidget(saveButton_);
        row->addWidget(exportAdviceBtn_);
        row->addSpacing(16);
        row->addWidget(depthLbl);
        row->addWidget(bitDepthCombo_);
        row->addWidget(flacCheck_);
        row->addSpacing(8);
        row->addWidget(targetLbl);
        row->addWidget(targetCombo_);
        row->addWidget(statusLabel_, 1);
```

(The only change from the existing code is inserting `row->addWidget(exportAdviceBtn_)` between `saveButton_` and the existing `addSpacing(16)`.)

- [ ] **Step 3: Save `lastSnap_` and enable button in `onAnalysisFinished()`**

In `onAnalysisFinished()`, after `chainPanel_->setAdvice(…)`:

```cpp
    lastSnap_ = result.analysis;
    exportAdviceBtn_->setEnabled(true);
```

- [ ] **Step 4: Update `lastSnap_` in `onRenderFinished()`**

In `onRenderFinished()`, after `chainPanel_->setAdvice(result.advice, result.analysis, *currentPreset_)`:

```cpp
    lastSnap_ = result.analysis;
```

- [ ] **Step 5: Disable button in `setInputFile()`**

In `setInputFile()`, add alongside the other `setEnabled(false)` calls:

```cpp
    exportAdviceBtn_->setEnabled(false);
```

- [ ] **Step 6: Implement `onExportAdvice()`**

Add at the end of `MainWindow.cpp` (before the closing `} // namespace gui`):

```cpp
void MainWindow::onExportAdvice() {
    if (!currentPreset_) return;

    const QString defaultPath = [this]() -> QString {
        if (inputPath_.isEmpty()) return "advice.md";
        const fs::path p{inputPath_.toStdString()};
        const std::string stem = p.stem().string() + "_advice";
        return QString::fromStdString(
            (p.parent_path() / (stem + ".md")).string());
    }();

    const QString dest = QFileDialog::getSaveFileName(
        this, "Export Advice", defaultPath,
        "Markdown Files (*.md);;All Files (*)");
    if (dest.isEmpty()) return;

    const std::string content = mt::formatAdviceMarkdown(
        lastSnap_,
        chainPanel_->currentAdvice(),
        *currentPreset_,
        inputPath_.toStdString());

    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export failed",
            QString("Could not write to %1").arg(dest));
        return;
    }
    f.write(QByteArray::fromStdString(content));
    statusLabel_->setText(QString("Advice exported \xe2\x86\x92 %1").arg(dest));
}
```

- [ ] **Step 7: Build everything**

```bash
cmake --build build --parallel 2>&1 | tail -15
```

Expected: clean build, `MasterTweak` binary updated.

- [ ] **Step 8: Commit**

```bash
git add gui/MainWindow.h gui/MainWindow.cpp
git commit -m "feat: Export Advice button — saves mastering params to markdown"
```

---

### Task 4: Update TODO

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Mark item as done**

In `TODO.md`, replace:

```
- On the render row, a button should allow exporting the advices parameters to an markdown file in order to apply them in any other audio software such as individual DAW plugins and compare/preview/tweak the results. 
```

with:

```
- ~~On the render row, a button should allow exporting the advices parameters to an markdown file in order to apply them in any other audio software such as individual DAW plugins and compare/preview/tweak the results.~~ **DONE** — "Export Advice…" button in the render row calls `mt::formatAdviceMarkdown()` (core) and saves a `.md` file with measured analysis values and all derived mastering parameters.
```

- [ ] **Step 2: Commit**

```bash
git add TODO.md
git commit -m "docs: mark export advice feature as done in TODO"
```
