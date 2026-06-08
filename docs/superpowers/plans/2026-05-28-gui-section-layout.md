# GUI Section Layout — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the flat `ParameterPanel` + `AnalysisPanel` with a new `ChainPanel` widget that groups DSP stages into labelled, bypassable sections with inline analysis readouts.

**Architecture:** `ChainPanel` (new) owns a `QGridLayout` with 3 tiers: EQ (full width), MB Comp/Width/Saturator (3 columns), Mixbus/Limiter (2 columns). Each section is a checkable `QGroupBox` — unchecked = stage bypassed. Analysis readouts (crest, corr, RMS, per-band targets) are shown inline, removing the need for a separate `AnalysisPanel`. `ParameterPanel` and `AnalysisPanel` are deleted.

**Tech Stack:** C++20, Qt6 (`QGroupBox`, `QGridLayout`, `QDoubleSpinBox`), CMake/Ninja.

---

## File Map

| Action | Path | Purpose |
|--------|------|---------|
| Create | `gui/ChainPanel.h` | New widget declaration |
| Create | `gui/ChainPanel.cpp` | New widget implementation |
| Modify | `gui/MainWindow.h` | Replace ParameterPanel*/AnalysisPanel* with ChainPanel* |
| Modify | `gui/MainWindow.cpp` | Wire ChainPanel, remove old widget usage |
| Modify | `gui/CMakeLists.txt` | Add ChainPanel.cpp, remove ParameterPanel.cpp + AnalysisPanel.cpp |
| Delete | `gui/ParameterPanel.h` | Replaced |
| Delete | `gui/ParameterPanel.cpp` | Replaced |
| Delete | `gui/AnalysisPanel.h` | Replaced by inline readouts |
| Delete | `gui/AnalysisPanel.cpp` | Replaced by inline readouts |

---

### Task 1: Create `ChainPanel` widget

**Files:**
- Create: `gui/ChainPanel.h`
- Create: `gui/ChainPanel.cpp`
- Modify: `gui/CMakeLists.txt` (add source, keep old files for now)

- [ ] **Step 1: Write `gui/ChainPanel.h`**

```cpp
#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
QT_END_NAMESPACE

namespace gui {

class ChainPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChainPanel(QWidget* parent = nullptr);

    // Populate all controls and inline readouts from advice + analysis + preset.
    // Stores advice as the clean baseline; clears all dirty tints.
    void setAdvice(const mt::AdviceSet& advice,
                   const mt::AnalysisSnapshot& snap,
                   const mt::PresetData& preset);

    // Returns current AdviceSet: non-editable fields from stored baseline,
    // editable fields from current spinbox values.
    mt::AdviceSet currentAdvice() const;

    // Writes bypass states into opts (one flag per checkable QGroupBox).
    void populateBypassFlags(mt::RenderOptions& opts) const;

    // Restore spinboxes to last-advised baseline and clear dirty tints.
    void resetToAdvice();

    // Reset to pre-analysis state: default spinbox values, "—" readouts.
    void clear();

signals:
    // Emitted when any spinbox value or bypass checkbox changes.
    void overrideChanged(const mt::AdviceSet& advice);

private:
    void buildUi();
    void applyToSpins(const mt::AdviceSet& adv);
    void updateTint(QDoubleSpinBox* sp, double cleanVal);
    void clearAllTints();
    void emitOverride();

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    struct CleanValues {
        double eqGain[kNumBands]{};
        double limCeiling   = -1.0;
        double satDrive     =  0.0;
        double mixbusThresh = -20.0;
        double mixbusMakeup =  0.0;
    } cleanValues_;

    mt::AdviceSet autoAdvice_;
    bool          hasAdvice_ = false;

    // EQ section (Tier 1)
    QGroupBox*      eqBox_               = nullptr;
    QLabel*         eqReadouts_[kNumBands]{};
    QDoubleSpinBox* eqGainSpins_[kNumBands]{};

    // Multiband Comp section (Tier 2, col 0-1)
    QGroupBox* mbBox_      = nullptr;
    QLabel*    mbCrestLbl_ = nullptr;

    // Stereo Width section (Tier 2, col 2-3)
    QGroupBox* widthBox_     = nullptr;
    QLabel*    widthCorrLbl_ = nullptr;

    // Saturator section (Tier 2, col 4-5)
    QGroupBox*      satBox_      = nullptr;
    QLabel*         satCrestLbl_ = nullptr;
    QDoubleSpinBox* satDriveSpin_ = nullptr;

    // Mixbus Comp section (Tier 3, col 0-3)
    QGroupBox*      mixbusBox_        = nullptr;
    QDoubleSpinBox* mixbusThreshSpin_ = nullptr;
    QDoubleSpinBox* mixbusMakeupSpin_ = nullptr;
    QLabel*         mixbusRmsLbl_     = nullptr;

    // Limiter section (Tier 3, col 4-5)
    QGroupBox*      limBox_        = nullptr;
    QDoubleSpinBox* limCeilingSpin_ = nullptr;
    QLabel*         limPeakLbl_    = nullptr;
};

} // namespace gui
```

- [ ] **Step 2: Write `gui/ChainPanel.cpp`**

```cpp
#include "ChainPanel.h"

#include "mastertweak/analysis.hpp"

#include <cmath>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace gui {

ChainPanel::ChainPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ChainPanel::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* grid = new QGridLayout;
    // Equal column weights; Mixbus spans cols 0-3, Limiter spans cols 4-5.
    for (int c = 0; c < 6; ++c)
        grid->setColumnStretch(c, 1);

    // ── Tier 1: EQ (full width) ───────────────────────────────────────────────
    {
        eqBox_ = new QGroupBox("EQ", this);
        eqBox_->setCheckable(true);
        eqBox_->setChecked(true);

        auto* hbox = new QHBoxLayout(eqBox_);
        for (int i = 0; i < kNumBands; ++i) {
            const auto si = static_cast<size_t>(i);
            auto* col = new QVBoxLayout;
            col->setSpacing(2);

            auto* nameLbl = new QLabel(mt::AnalysisSnapshot::kBandNames[si], eqBox_);
            nameLbl->setAlignment(Qt::AlignHCenter);
            col->addWidget(nameLbl);

            auto* readout = new QLabel(QString::fromUtf8("—"), eqBox_);
            readout->setAlignment(Qt::AlignHCenter);
            readout->setStyleSheet("font-size: 9pt; color: #555;");
            eqReadouts_[i] = readout;
            col->addWidget(readout);

            auto* sp = new QDoubleSpinBox(eqBox_);
            sp->setRange(-12.0, 12.0);
            sp->setSingleStep(0.5);
            sp->setDecimals(1);
            sp->setSuffix(" dB");
            sp->setValue(0.0);
            sp->setAlignment(Qt::AlignRight);
            eqGainSpins_[i] = sp;
            col->addWidget(sp);

            hbox->addLayout(col);

            const int ci = i;
            connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this, ci](double) {
                        updateTint(eqGainSpins_[ci], cleanValues_.eqGain[ci]);
                        emitOverride();
                    });
        }
        connect(eqBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(eqBox_, 0, 0, 1, 6);
    }

    // ── Tier 2 col 0-1: Multiband Comp ───────────────────────────────────────
    {
        mbBox_ = new QGroupBox("Multiband Comp", this);
        mbBox_->setCheckable(true);
        mbBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(mbBox_);
        mbCrestLbl_ = new QLabel(QString::fromUtf8("—"), mbBox_);
        vbox->addWidget(mbCrestLbl_);
        auto* adv = new QLabel("Advice-driven", mbBox_);
        adv->setStyleSheet("color: #888; font-style: italic;");
        vbox->addWidget(adv);
        vbox->addStretch();

        connect(mbBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(mbBox_, 1, 0, 1, 2);
    }

    // ── Tier 2 col 2-3: Stereo Width ─────────────────────────────────────────
    {
        widthBox_ = new QGroupBox("Stereo Width", this);
        widthBox_->setCheckable(true);
        widthBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(widthBox_);
        widthCorrLbl_ = new QLabel(QString::fromUtf8("—"), widthBox_);
        vbox->addWidget(widthCorrLbl_);
        auto* adv = new QLabel("Advice-driven", widthBox_);
        adv->setStyleSheet("color: #888; font-style: italic;");
        vbox->addWidget(adv);
        vbox->addStretch();

        connect(widthBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(widthBox_, 1, 2, 1, 2);
    }

    // ── Tier 2 col 4-5: Saturator ────────────────────────────────────────────
    {
        satBox_ = new QGroupBox("Saturator", this);
        satBox_->setCheckable(true);
        satBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(satBox_);
        satCrestLbl_ = new QLabel(QString::fromUtf8("—"), satBox_);
        vbox->addWidget(satCrestLbl_);

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Drive:", satBox_));
        satDriveSpin_ = new QDoubleSpinBox(satBox_);
        satDriveSpin_->setRange(0.0, 6.0);
        satDriveSpin_->setSingleStep(0.5);
        satDriveSpin_->setDecimals(1);
        satDriveSpin_->setSuffix(" dB");
        satDriveSpin_->setValue(0.0);
        row->addWidget(satDriveSpin_);
        row->addStretch();
        vbox->addLayout(row);
        vbox->addStretch();

        connect(satDriveSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(satDriveSpin_, cleanValues_.satDrive);
                    emitOverride();
                });
        connect(satBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(satBox_, 1, 4, 1, 2);
    }

    // ── Tier 3 col 0-3: Mixbus Comp ──────────────────────────────────────────
    {
        mixbusBox_ = new QGroupBox("Mixbus Comp", this);
        mixbusBox_->setCheckable(true);
        mixbusBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(mixbusBox_);

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Thr:", mixbusBox_));
        mixbusThreshSpin_ = new QDoubleSpinBox(mixbusBox_);
        mixbusThreshSpin_->setRange(-40.0, 0.0);
        mixbusThreshSpin_->setSingleStep(1.0);
        mixbusThreshSpin_->setDecimals(1);
        mixbusThreshSpin_->setSuffix(" dB");
        mixbusThreshSpin_->setValue(-20.0);
        row->addWidget(mixbusThreshSpin_);
        row->addSpacing(8);
        row->addWidget(new QLabel("Mkup:", mixbusBox_));
        mixbusMakeupSpin_ = new QDoubleSpinBox(mixbusBox_);
        mixbusMakeupSpin_->setRange(-12.0, 12.0);
        mixbusMakeupSpin_->setSingleStep(0.5);
        mixbusMakeupSpin_->setDecimals(1);
        mixbusMakeupSpin_->setSuffix(" dB");
        mixbusMakeupSpin_->setValue(0.0);
        row->addWidget(mixbusMakeupSpin_);
        row->addStretch();
        vbox->addLayout(row);

        mixbusRmsLbl_ = new QLabel(QString::fromUtf8("—"), mixbusBox_);
        mixbusRmsLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(mixbusRmsLbl_);

        connect(mixbusThreshSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(mixbusThreshSpin_, cleanValues_.mixbusThresh);
                    emitOverride();
                });
        connect(mixbusMakeupSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(mixbusMakeupSpin_, cleanValues_.mixbusMakeup);
                    emitOverride();
                });
        connect(mixbusBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(mixbusBox_, 2, 0, 1, 4);
    }

    // ── Tier 3 col 4-5: Limiter ──────────────────────────────────────────────
    {
        limBox_ = new QGroupBox("Limiter", this);
        limBox_->setCheckable(true);
        limBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(limBox_);

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Ceil:", limBox_));
        limCeilingSpin_ = new QDoubleSpinBox(limBox_);
        limCeilingSpin_->setRange(-6.0, 0.0);
        limCeilingSpin_->setSingleStep(0.5);
        limCeilingSpin_->setDecimals(1);
        limCeilingSpin_->setSuffix(" dBTP");
        limCeilingSpin_->setValue(-1.0);
        row->addWidget(limCeilingSpin_);
        row->addStretch();
        vbox->addLayout(row);

        limPeakLbl_ = new QLabel(QString::fromUtf8("—"), limBox_);
        limPeakLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(limPeakLbl_);

        connect(limCeilingSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(limCeilingSpin_, cleanValues_.limCeiling);
                    emitOverride();
                });
        connect(limBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(limBox_, 2, 4, 1, 2);
    }

    outer->addLayout(grid);
}

// ─────────────────────────────────────────────────────────────────────────────

void ChainPanel::applyToSpins(const mt::AdviceSet& adv) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqGainSpins_[i]->blockSignals(true);
        eqGainSpins_[i]->setValue(static_cast<double>(adv.eq[si].gainDb));
        eqGainSpins_[i]->blockSignals(false);
    }
    limCeilingSpin_->blockSignals(true);
    limCeilingSpin_->setValue(static_cast<double>(adv.limiter.ceilingDb));
    limCeilingSpin_->blockSignals(false);

    satDriveSpin_->blockSignals(true);
    satDriveSpin_->setValue(static_cast<double>(adv.saturator.driveDb));
    satDriveSpin_->blockSignals(false);

    mixbusThreshSpin_->blockSignals(true);
    mixbusThreshSpin_->setValue(static_cast<double>(adv.mixbusComp.thresholdDb));
    mixbusThreshSpin_->blockSignals(false);

    mixbusMakeupSpin_->blockSignals(true);
    mixbusMakeupSpin_->setValue(static_cast<double>(adv.mixbusComp.makeupDb));
    mixbusMakeupSpin_->blockSignals(false);
}

void ChainPanel::updateTint(QDoubleSpinBox* sp, double cleanVal) {
    constexpr double kEps = 1e-9;
    sp->setStyleSheet(std::abs(sp->value() - cleanVal) > kEps
                      ? "background-color: #d0e8ff;" : "");
}

void ChainPanel::clearAllTints() {
    for (int i = 0; i < kNumBands; ++i)
        eqGainSpins_[i]->setStyleSheet("");
    limCeilingSpin_->setStyleSheet("");
    satDriveSpin_->setStyleSheet("");
    mixbusThreshSpin_->setStyleSheet("");
    mixbusMakeupSpin_->setStyleSheet("");
}

void ChainPanel::emitOverride() {
    emit overrideChanged(currentAdvice());
}

// ─────────────────────────────────────────────────────────────────────────────

void ChainPanel::setAdvice(const mt::AdviceSet& advice,
                           const mt::AnalysisSnapshot& snap,
                           const mt::PresetData& preset) {
    autoAdvice_ = advice;
    hasAdvice_  = true;

    for (int i = 0; i < kNumBands; ++i)
        cleanValues_.eqGain[i] = static_cast<double>(advice.eq[static_cast<size_t>(i)].gainDb);
    cleanValues_.limCeiling   = static_cast<double>(advice.limiter.ceilingDb);
    cleanValues_.satDrive     = static_cast<double>(advice.saturator.driveDb);
    cleanValues_.mixbusThresh = static_cast<double>(advice.mixbusComp.thresholdDb);
    cleanValues_.mixbusMakeup = static_cast<double>(advice.mixbusComp.makeupDb);

    applyToSpins(advice);
    clearAllTints();

    // EQ per-band readouts: "preset target → measured RMS"
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqReadouts_[i]->setText(
            QString("%1→%2 dB")
                .arg(static_cast<int>(std::round(preset.bandRmsDb[si])))
                .arg(static_cast<int>(std::round(snap.bands[si].avgRmsDb))));
    }

    // Average crest factor (shared by MB Comp and Saturator sections)
    float crestSum = 0.f;
    for (int i = 0; i < kNumBands; ++i)
        crestSum += snap.bands[static_cast<size_t>(i)].crestDb;
    const float avgCrest = crestSum / static_cast<float>(kNumBands);

    mbCrestLbl_->setText(QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));
    satCrestLbl_->setText(QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));

    widthCorrLbl_->setText(
        QString("Corr: %1").arg(static_cast<double>(snap.overallCorr), 0, 'f', 2));

    mixbusRmsLbl_->setText(
        QString("rms: %1 dBFS").arg(static_cast<double>(snap.overallAvgDb), 0, 'f', 1));

    limPeakLbl_->setText(
        QString("peak: %1 dBFS").arg(static_cast<double>(snap.overallPeakDb), 0, 'f', 1));
}

mt::AdviceSet ChainPanel::currentAdvice() const {
    mt::AdviceSet adv = autoAdvice_;
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        adv.eq[si].gainDb = static_cast<float>(eqGainSpins_[i]->value());
    }
    adv.limiter.ceilingDb      = static_cast<float>(limCeilingSpin_->value());
    adv.saturator.driveDb      = static_cast<float>(satDriveSpin_->value());
    adv.mixbusComp.thresholdDb = static_cast<float>(mixbusThreshSpin_->value());
    adv.mixbusComp.makeupDb    = static_cast<float>(mixbusMakeupSpin_->value());
    return adv;
}

void ChainPanel::populateBypassFlags(mt::RenderOptions& opts) const {
    opts.bypassEq         = !eqBox_->isChecked();
    opts.bypassMbComp     = !mbBox_->isChecked();
    opts.bypassSaturator  = !satBox_->isChecked();
    opts.bypassWidth      = !widthBox_->isChecked();
    opts.bypassMixbusComp = !mixbusBox_->isChecked();
    opts.bypassLimiter    = !limBox_->isChecked();
    // bypassDither not exposed in UI; stays false (default)
}

void ChainPanel::resetToAdvice() {
    if (!hasAdvice_) return;
    applyToSpins(autoAdvice_);
    clearAllTints();
}

void ChainPanel::clear() {
    hasAdvice_ = false;
    autoAdvice_ = mt::AdviceSet{};

    for (int i = 0; i < kNumBands; ++i) cleanValues_.eqGain[i] = 0.0;
    cleanValues_.limCeiling   = -1.0;
    cleanValues_.satDrive     =  0.0;
    cleanValues_.mixbusThresh = -20.0;
    cleanValues_.mixbusMakeup =  0.0;

    for (int i = 0; i < kNumBands; ++i) {
        eqGainSpins_[i]->blockSignals(true);
        eqGainSpins_[i]->setValue(0.0);
        eqGainSpins_[i]->blockSignals(false);
        eqGainSpins_[i]->setStyleSheet("");
        eqReadouts_[i]->setText(QString::fromUtf8("—"));
    }

    auto resetSpin = [](QDoubleSpinBox* sp, double val) {
        sp->blockSignals(true);
        sp->setValue(val);
        sp->blockSignals(false);
        sp->setStyleSheet("");
    };
    resetSpin(limCeilingSpin_,    -1.0);
    resetSpin(satDriveSpin_,       0.0);
    resetSpin(mixbusThreshSpin_, -20.0);
    resetSpin(mixbusMakeupSpin_,   0.0);

    const QString dash = QString::fromUtf8("—");
    mbCrestLbl_->setText(dash);
    widthCorrLbl_->setText(dash);
    satCrestLbl_->setText(dash);
    mixbusRmsLbl_->setText(dash);
    limPeakLbl_->setText(dash);
}

} // namespace gui
```

- [ ] **Step 3: Add `ChainPanel.h` + `ChainPanel.cpp` to `gui/CMakeLists.txt`**

Replace the `qt_add_executable` source list with the following (adds the two new files; keeps `ParameterPanel` and `AnalysisPanel` for now):

```cmake
qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ChainPanel.h
    ChainPanel.cpp
    PresetSelector.h
    PresetSelector.cpp
    AnalysisPanel.h
    AnalysisPanel.cpp
    ParameterPanel.h
    ParameterPanel.cpp
    TransportWidget.h
    TransportWidget.cpp
    TargetLevelCombo.h
    TargetLevelCombo.cpp
)
```

- [ ] **Step 4: Build to verify ChainPanel compiles**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings. `ParameterPanel` and `AnalysisPanel` still compile — they are not yet removed.

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/ChainPanel.h gui/ChainPanel.cpp gui/CMakeLists.txt
git commit -m "feat: add ChainPanel widget (6-section DSP grid layout)"
```

---

### Task 2: Wire `MainWindow` to use `ChainPanel`

**Files:**
- Modify: `gui/MainWindow.h`
- Modify: `gui/MainWindow.cpp`

- [ ] **Step 1: Update `gui/MainWindow.h`**

Replace the existing content with the following. Changes: remove `AnalysisPanel` forward declaration and member, replace `ParameterPanel*` with `ChainPanel*`.

```cpp
#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"
#include "TargetLevelCombo.h"

#include <QMainWindow>
#include <QThread>
#include <optional>
#include <string>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressDialog;
class QPushButton;
QT_END_NAMESPACE

namespace gui {
class ChainPanel;
class PresetSelector;
class TransportWidget;
}

namespace gui {

class AnalysisWorker : public QThread {
    Q_OBJECT
public:
    explicit AnalysisWorker(QObject* parent = nullptr);
    void setup(const std::string& inputPath, const mt::PresetData& preset, int seq);

signals:
    void finished(bool ok, const QString& errorMsg, mt::MasterResult result, int seq);

protected:
    void run() override;

private:
    std::string    inputPath_;
    mt::PresetData preset_;
    int            seq_ = 0;
};

class RenderWorker : public QThread {
    Q_OBJECT
public:
    explicit RenderWorker(QObject* parent = nullptr);

    void setup(const std::string& inputPath,
               const std::string& outputPath,
               const mt::PresetData& preset,
               const mt::RenderOptions& opts,
               const mt::AdviceSet* adviceOverride);

signals:
    void progress(float fraction, const QString& stage);
    void finished(bool ok, const QString& errorMsg, mt::MasterResult result);

protected:
    void run() override;

private:
    std::string   inputPath_;
    std::string   outputPath_;
    mt::PresetData preset_;
    mt::RenderOptions opts_;
    std::optional<mt::AdviceSet> adviceOverride_;
};

// ─────────────────────────────────────────────────────────────────────────────

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onOpenFile();
    void onPresetChanged(const mt::PresetData& preset);
    void onRenderClicked();
    void onSaveAs();
    void onAnalysisFinished(bool ok, const QString& errorMsg, mt::MasterResult result, int seq);
    void onRenderProgress(float fraction, const QString& stage);
    void onRenderFinished(bool ok, const QString& errorMsg, mt::MasterResult result);

private:
    void buildUi();
    void startAnalysis();
    void setInputFile(const QString& path);
    QString makeDefaultOutputPath(bool flac) const;
    static std::string sanitizePresetName(const std::string& name);

    // ── State ─────────────────────────────────────────────────────────────────
    QString                       inputPath_;
    QString                       renderedPath_;
    std::optional<mt::PresetData> currentPreset_;

    // ── Widgets ───────────────────────────────────────────────────────────────
    QLabel*           inputLabel_     = nullptr;
    PresetSelector*   presetSelector_ = nullptr;
    ChainPanel*       chainPanel_     = nullptr;
    TransportWidget*  transport_      = nullptr;
    QPushButton*      renderButton_   = nullptr;
    QPushButton*      saveButton_     = nullptr;
    QPushButton*      resetBtn_       = nullptr;
    QComboBox*        bitDepthCombo_  = nullptr;
    QCheckBox*        flacCheck_      = nullptr;
    TargetLevelCombo* targetCombo_    = nullptr;
    QLabel*           statusLabel_    = nullptr;

    // ── Workers ───────────────────────────────────────────────────────────────
    AnalysisWorker*  analysisWorker_ = nullptr;
    int              analysisSeq_    = 0;
    RenderWorker*    renderWorker_   = nullptr;
    QProgressDialog* progressDialog_ = nullptr;
};

} // namespace gui
```

- [ ] **Step 2: Update `gui/MainWindow.cpp`**

Replace the full contents of `MainWindow.cpp` with the following. Key changes:
- Include `ChainPanel.h` instead of `AnalysisPanel.h` + `ParameterPanel.h`
- `buildUi()`: replace scroll area containing `analysisPanel_` + `parameterPanel_` with `chainPanel_`
- `setInputFile()`: call `chainPanel_->clear()` instead of two separate clears
- `onAnalysisFinished()`: call `chainPanel_->setAdvice(...)` with all three arguments
- `onRenderClicked()`: use `chainPanel_->populateBypassFlags(opts)` and `chainPanel_->currentAdvice()`
- `onRenderFinished()`: call `chainPanel_->setAdvice(...)` with all three arguments
- Reset button: call `chainPanel_->resetToAdvice()`

```cpp
#include "MainWindow.h"
#include "ChainPanel.h"
#include "PresetSelector.h"
#include "TargetLevelCombo.h"
#include "TransportWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

// ─────────────────────────────────────────────────────────────────────────────
// AnalysisWorker
// ─────────────────────────────────────────────────────────────────────────────

AnalysisWorker::AnalysisWorker(QObject* parent) : QThread(parent) {}

void AnalysisWorker::setup(const std::string& inputPath,
                            const mt::PresetData& preset, int seq) {
    inputPath_ = inputPath;
    preset_    = preset;
    seq_       = seq;
}

void AnalysisWorker::run() {
    std::string err;
    auto result = mt::analyseOnly(inputPath_, preset_, &err);
    emit finished(result.ok, QString::fromStdString(err), std::move(result), seq_);
}

// ─────────────────────────────────────────────────────────────────────────────
// RenderWorker
// ─────────────────────────────────────────────────────────────────────────────

RenderWorker::RenderWorker(QObject* parent) : QThread(parent) {}

void RenderWorker::setup(const std::string& inputPath,
                         const std::string& outputPath,
                         const mt::PresetData& preset,
                         const mt::RenderOptions& opts,
                         const mt::AdviceSet* adviceOverride) {
    inputPath_  = inputPath;
    outputPath_ = outputPath;
    preset_     = preset;
    opts_       = opts;
    if (adviceOverride) adviceOverride_ = *adviceOverride;
    else                adviceOverride_.reset();
}

void RenderWorker::run() {
    std::string err;
    auto result = mt::renderFile(
        inputPath_, outputPath_, preset_, opts_,
        adviceOverride_ ? &*adviceOverride_ : nullptr,
        [this](float frac, const std::string& stage) {
            emit progress(frac, QString::fromStdString(stage));
        },
        &err);
    emit finished(result.ok, QString::fromStdString(err), std::move(result));
}

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();

    const std::string execDir = fs::path{
        QApplication::applicationFilePath().toStdString()
    }.parent_path().string();
    presetSelector_->populate(execDir);
}

void MainWindow::buildUi() {
    setWindowTitle("MasterTweak");
    resize(900, 740);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* vbox = new QVBoxLayout(central);

    // ── Input file row ────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        auto* openBtn = new QPushButton("Open audio…", central);
        inputLabel_ = new QLabel("(no file)", central);
        inputLabel_->setWordWrap(false);
        connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenFile);
        row->addWidget(openBtn);
        row->addWidget(inputLabel_, 1);
        vbox->addLayout(row);
    }

    // ── Preset row ────────────────────────────────────────────────────────────
    {
        auto* row  = new QHBoxLayout;
        auto* lbl  = new QLabel("Preset:", central);
        presetSelector_ = new PresetSelector(central);
        resetBtn_ = new QPushButton(QString::fromUtf8("↺ Reset"), central);
        resetBtn_->setEnabled(false);
        connect(presetSelector_, &PresetSelector::presetChanged,
                this, &MainWindow::onPresetChanged);
        connect(resetBtn_, &QPushButton::clicked, this, [this] {
            chainPanel_->resetToAdvice();
        });
        row->addWidget(lbl);
        row->addWidget(presetSelector_, 1);
        row->addWidget(resetBtn_);
        vbox->addLayout(row);
    }

    // ── Chain panel (scrollable) ──────────────────────────────────────────────
    {
        auto* scroll = new QScrollArea(central);
        scroll->setWidgetResizable(true);
        auto* inner  = new QWidget;
        auto* ivbox  = new QVBoxLayout(inner);

        chainPanel_ = new ChainPanel(inner);
        ivbox->addWidget(chainPanel_);
        ivbox->addStretch();

        scroll->setWidget(inner);
        vbox->addWidget(scroll, 1);
    }

    // ── Transport ─────────────────────────────────────────────────────────────
    transport_ = new TransportWidget(central);
    vbox->addWidget(transport_);

    // ── Output format + Render row ────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;

        renderButton_ = new QPushButton("Render", central);
        saveButton_   = new QPushButton("Save As…", central);
        renderButton_->setEnabled(false);
        saveButton_->setEnabled(false);

        auto* depthLbl = new QLabel("Bit depth:", central);
        bitDepthCombo_ = new QComboBox(central);
        bitDepthCombo_->addItem("16-bit", 16);
        bitDepthCombo_->addItem("24-bit", 24);
        bitDepthCombo_->addItem("32-bit float", 32);
        bitDepthCombo_->setCurrentIndex(1);

        flacCheck_ = new QCheckBox("FLAC", central);

        auto* targetLbl = new QLabel("Target:", central);
        targetCombo_ = new TargetLevelCombo(central);

        statusLabel_ = new QLabel("Ready", central);

        connect(renderButton_, &QPushButton::clicked, this, &MainWindow::onRenderClicked);
        connect(saveButton_,   &QPushButton::clicked, this, &MainWindow::onSaveAs);

        row->addWidget(renderButton_);
        row->addWidget(saveButton_);
        row->addSpacing(16);
        row->addWidget(depthLbl);
        row->addWidget(bitDepthCombo_);
        row->addWidget(flacCheck_);
        row->addSpacing(8);
        row->addWidget(targetLbl);
        row->addWidget(targetCombo_);
        row->addWidget(statusLabel_, 1);
        vbox->addLayout(row);
    }

    analysisWorker_ = new AnalysisWorker(this);
    connect(analysisWorker_, &AnalysisWorker::finished,
            this, &MainWindow::onAnalysisFinished);

    renderWorker_ = new RenderWorker(this);
    connect(renderWorker_, &RenderWorker::progress, this, &MainWindow::onRenderProgress);
    connect(renderWorker_, &RenderWorker::finished, this, &MainWindow::onRenderFinished);
}

void MainWindow::onOpenFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Audio File", QString(),
        "Audio Files (*.wav *.flac *.aiff *.aif);;All Files (*)");
    if (path.isEmpty()) return;
    setInputFile(path);
}

void MainWindow::setInputFile(const QString& path) {
    inputPath_ = path;
    inputLabel_->setText(path);
    chainPanel_->clear();
    transport_->unload();
    transport_->setOriginalFile(path);
    renderedPath_.clear();
    saveButton_->setEnabled(false);
    renderButton_->setEnabled(false);
    resetBtn_->setEnabled(false);
    startAnalysis();
}

void MainWindow::onPresetChanged(const mt::PresetData& preset) {
    currentPreset_ = preset;
    startAnalysis();
}

void MainWindow::startAnalysis() {
    if (inputPath_.isEmpty() || !currentPreset_) return;

    const int seq = ++analysisSeq_;

    if (analysisWorker_->isRunning()) {
        analysisWorker_->requestInterruption();
        analysisWorker_->wait(500);
    }
    statusLabel_->setText("Analysing…");
    renderButton_->setEnabled(false);
    if (resetBtn_) resetBtn_->setEnabled(false);
    analysisWorker_->setup(inputPath_.toStdString(), *currentPreset_, seq);
    analysisWorker_->start();
}

void MainWindow::onAnalysisFinished(bool ok, const QString& errorMsg,
                                    mt::MasterResult result, int seq) {
    if (seq != analysisSeq_) return;

    if (!ok) {
        statusLabel_->setText(QString("Analysis failed: %1").arg(errorMsg));
        return;
    }
    chainPanel_->setAdvice(result.advice, result.analysis, *currentPreset_);
    resetBtn_->setEnabled(true);
    statusLabel_->setText("Analysis complete — ready to render");
    renderButton_->setEnabled(true);
}

void MainWindow::onRenderClicked() {
    if (inputPath_.isEmpty() || !currentPreset_) return;

    const bool useFlac = flacCheck_->isChecked();
    renderedPath_ = makeDefaultOutputPath(useFlac);
    renderButton_->setEnabled(false);

    auto* dlg = new QProgressDialog("Rendering…", "Cancel", 0, 100, this);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(false);
    progressDialog_ = dlg;
    dlg->show();

    connect(dlg, &QProgressDialog::canceled, this, [this] {
        if (renderWorker_->isRunning()) renderWorker_->terminate();
    });

    mt::RenderOptions opts;
    chainPanel_->populateBypassFlags(opts);
    opts.outputBitDepth = bitDepthCombo_->currentData().toInt();
    opts.outputFlac     = useFlac;
    opts.targetLevel    = targetCombo_->currentTarget();

    mt::AdviceSet params = chainPanel_->currentAdvice();
    renderWorker_->setup(inputPath_.toStdString(), renderedPath_.toStdString(),
                         *currentPreset_, opts, &params);
    renderWorker_->start();
}

void MainWindow::onRenderProgress(float fraction, const QString& stage) {
    if (progressDialog_) {
        progressDialog_->setValue(static_cast<int>(fraction * 100.f));
        progressDialog_->setLabelText(stage);
    }
}

void MainWindow::onRenderFinished(bool ok, const QString& errorMsg, mt::MasterResult result) {
    if (progressDialog_) {
        progressDialog_->close();
        progressDialog_->deleteLater();
        progressDialog_ = nullptr;
    }
    renderButton_->setEnabled(true);

    if (!ok) {
        QMessageBox::critical(this, "Render failed", errorMsg);
        statusLabel_->setText("Render failed");
        return;
    }

    saveButton_->setEnabled(true);
    statusLabel_->setText(QString("Rendered → %1").arg(renderedPath_));
    transport_->loadFile(renderedPath_);

    chainPanel_->setAdvice(result.advice, result.analysis, *currentPreset_);
}

void MainWindow::onSaveAs() {
    if (renderedPath_.isEmpty()) return;
    const bool flac = renderedPath_.endsWith(".flac", Qt::CaseInsensitive);
    const QString dest = QFileDialog::getSaveFileName(
        this, "Save Mastered File", renderedPath_,
        flac ? "FLAC Files (*.flac);;All Files (*)"
             : "WAV Files (*.wav);;All Files (*)");
    if (dest.isEmpty()) return;

    if (!QFile::copy(renderedPath_, dest)) {
        QMessageBox::warning(this, "Save failed",
            QString("Could not copy %1 to %2").arg(renderedPath_, dest));
    }
}

// static
std::string MainWindow::sanitizePresetName(const std::string& name) {
    std::string s;
    s.reserve(name.size());
    for (char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        s += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : '_';
    }
    std::string out;
    bool prevUnder = false;
    for (char c : s) {
        if (c == '_') { if (!prevUnder) out += c; prevUnder = true; }
        else          { out += c; prevUnder = false; }
    }
    auto start = out.find_first_not_of('_');
    if (start == std::string::npos) return "preset";
    out = out.substr(start);
    auto end = out.find_last_not_of('_');
    if (end != std::string::npos) out = out.substr(0, end + 1);
    if (out.size() > 24) out.resize(24);
    auto end2 = out.find_last_not_of('_');
    if (end2 != std::string::npos) out = out.substr(0, end2 + 1);
    return out.empty() ? "preset" : out;
}

QString MainWindow::makeDefaultOutputPath(bool flac) const {
    if (inputPath_.isEmpty()) return {};
    fs::path p{inputPath_.toStdString()};
    const std::string suffix = currentPreset_ ? sanitizePresetName(currentPreset_->name) : "master";
    const std::string stem = p.stem().string() + "_" + suffix;
    return QString::fromStdString(
        (p.parent_path() / (stem + (flac ? ".flac" : ".wav"))).string());
}

} // namespace gui
```

- [ ] **Step 3: Build — verify MainWindow compiles with ChainPanel**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings. `ParameterPanel.cpp` and `AnalysisPanel.cpp` still exist and compile but are no longer used.

- [ ] **Step 4: Run all existing tests**

```bash
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (core lib tests are unaffected by GUI changes).

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/MainWindow.h gui/MainWindow.cpp
git commit -m "feat: wire MainWindow to ChainPanel, remove AnalysisPanel/ParameterPanel usage"
```

---

### Task 3: Delete old files, update CMakeLists, update TODO

**Files:**
- Delete: `gui/ParameterPanel.h`, `gui/ParameterPanel.cpp`
- Delete: `gui/AnalysisPanel.h`, `gui/AnalysisPanel.cpp`
- Modify: `gui/CMakeLists.txt` (remove deleted sources)
- Modify: `TODO.md`

- [ ] **Step 1: Remove old source files**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
rm gui/ParameterPanel.h gui/ParameterPanel.cpp
rm gui/AnalysisPanel.h gui/AnalysisPanel.cpp
```

- [ ] **Step 2: Update `gui/CMakeLists.txt`**

Replace the `qt_add_executable` source list with the following (removes the four deleted files):

```cmake
qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ChainPanel.h
    ChainPanel.cpp
    PresetSelector.h
    PresetSelector.cpp
    TransportWidget.h
    TransportWidget.cpp
    TargetLevelCombo.h
    TargetLevelCombo.cpp
)
```

- [ ] **Step 3: Build — verify everything still compiles**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 4: Run all tests**

```bash
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass.

- [ ] **Step 5: Update `TODO.md`**

Find this line in `TODO.md`:
```
- Make the GUI sound-engineer friendly
```

Replace it with:
```
- ~~Make the GUI sound-engineer friendly~~ **IN PROGRESS** — Section layout done (ChainPanel with 6 bypassable sections, inline analysis readouts). Remaining: custom knob/fader widgets, VU meter.
```

- [ ] **Step 6: Final commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/CMakeLists.txt TODO.md
git rm gui/ParameterPanel.h gui/ParameterPanel.cpp
git rm gui/AnalysisPanel.h gui/AnalysisPanel.cpp
git commit -m "refactor: replace ParameterPanel+AnalysisPanel with ChainPanel"
```
