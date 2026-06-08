# Custom Knob & Fader Widgets — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all `QDoubleSpinBox` controls in `ChainPanel` with custom `RotaryKnob` (knobs) and `VerticalFader` (faders) Qt6 widgets that support drag, scroll-wheel, double-click-to-type, and a dirty-dot indicator.

**Architecture:** A shared `AudioControl` base class owns all interaction logic (drag, wheel, line-edit popup, dirty dot, value label). `RotaryKnob` and `VerticalFader` inherit it and add only their `paintControl()` implementation. `ChainPanel` swaps its `QDoubleSpinBox` members for the new controls and removes `cleanValues_` / `updateTint` / `clearAllTints` (replaced by `setClean()` on each widget).

**Tech Stack:** C++20, Qt6 (`QWidget`, `QPainter`, `QMouseEvent`, `QWheelEvent`, `QLineEdit`), CMake/Ninja.

---

## File Map

| Action | File | Purpose |
|--------|------|---------|
| Create | `gui/AudioControl.h` | Base class: value/range/step, interaction, label, dirty dot |
| Create | `gui/AudioControl.cpp` | Base class implementation |
| Create | `gui/RotaryKnob.h` | Rotary knob subclass declaration |
| Create | `gui/RotaryKnob.cpp` | 270° arc painting |
| Create | `gui/VerticalFader.h` | Vertical fader subclass declaration |
| Create | `gui/VerticalFader.cpp` | Track + thumb painting |
| Replace | `gui/ChainPanel.h` | Swap spinbox members for knob/fader pointers |
| Replace | `gui/ChainPanel.cpp` | Swap buildUi, applyToControls, currentAdvice, clear |
| Modify | `gui/CMakeLists.txt` | Add 6 new source files |

---

### Task 1: `AudioControl` base class

**Files:**
- Create: `gui/AudioControl.h`
- Create: `gui/AudioControl.cpp`

- [ ] **Step 1: Write `gui/AudioControl.h`**

```cpp
#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

namespace gui {

// Base class for all custom audio parameter controls (knobs, faders).
// Subclasses implement paintControl() and sizeHint().
// Provides: drag-to-change (vertical), scroll-wheel, double-click-to-type,
// value label with sign, dirty-dot indicator.
class AudioControl : public QWidget {
    Q_OBJECT
public:
    explicit AudioControl(QWidget* parent = nullptr);

    void   setValue(double v);
    double value() const;
    void   setRange(double min, double max);
    void   setSingleStep(double step);
    void   setClean(double baseline);   // dirty dot shown when value ≠ baseline
    void   setSuffix(const QString& s); // e.g. " dB", " dBTP"

signals:
    void valueChanged(double value);

protected:
    // Subclass paints its visual in controlRect (widget rect minus label strip).
    virtual void paintControl(QPainter& p, const QRect& controlRect) = 0;

    // Exposed so subclasses can compute correct sizeHint().
    static constexpr int kLabelH = 20;

    double  value_    = 0.0;
    double  minVal_   = 0.0;
    double  maxVal_   = 1.0;
    double  step_     = 0.1;
    double  cleanVal_ = 0.0;
    QString suffix_;

private:
    void paintEvent(QPaintEvent*) override final;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

    void clampAndEmit(double v);

    QLineEdit* lineEdit_     = nullptr;
    double     startDragVal_ = 0.0;
    int        startDragY_   = 0;
    bool       dragging_     = false;

    static constexpr double kDirtyEps     = 1e-9;
    static constexpr int    kPixelsPerStep = 2;  // drag pixels per single step
};

} // namespace gui
```

- [ ] **Step 2: Write `gui/AudioControl.cpp`**

```cpp
#include "AudioControl.h"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace gui {

AudioControl::AudioControl(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::SizeVerCursor);

    lineEdit_ = new QLineEdit(this);
    lineEdit_->hide();
    lineEdit_->setAlignment(Qt::AlignCenter);

    // Hide on Enter or focus-lost; validate and apply value.
    connect(lineEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (!lineEdit_->isVisible()) return;  // guard against double-fire
        bool ok = false;
        const double parsed = lineEdit_->text().toDouble(&ok);
        lineEdit_->hide();
        if (ok) clampAndEmit(parsed);
    });
}

void AudioControl::setValue(double v) {
    value_ = std::clamp(v, minVal_, maxVal_);
    update();
}

double AudioControl::value() const { return value_; }

void AudioControl::setRange(double min, double max) {
    minVal_ = min;
    maxVal_ = max;
    value_  = std::clamp(value_, minVal_, maxVal_);
    update();
}

void AudioControl::setSingleStep(double step) { step_ = step; }

void AudioControl::setClean(double baseline) {
    cleanVal_ = baseline;
    update();
}

void AudioControl::setSuffix(const QString& s) {
    suffix_ = s;
    update();
}

void AudioControl::clampAndEmit(double v) {
    value_ = std::clamp(v, minVal_, maxVal_);
    update();
    emit valueChanged(value_);
}

// ─────────────────────────────────────────────────────────────────────────────

void AudioControl::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Delegate visual drawing to subclass
    const QRect controlRect(0, 0, width(), height() - kLabelH);
    paintControl(p, controlRect);

    // Value label strip at the bottom
    const QRect labelRect(0, height() - kLabelH, width(), kLabelH);

    // "+3.0 dB" / "-1.0 dBTP" / "0.0 dB"
    const QString valStr = (value_ > 0.0 ? "+" : "") +
                           QString::number(value_, 'f', 1) + suffix_;

    p.setPen(QColor("#222222"));
    QFont lf = font();
    lf.setPointSize(8);
    p.setFont(lf);
    const QFontMetrics fm(lf);
    const int textW = fm.horizontalAdvance(valStr);
    const int textX = (width() - textW) / 2;
    const int textY = labelRect.top() + (kLabelH + fm.ascent() - fm.descent()) / 2;
    p.drawText(textX, textY, valStr);

    // Blue dot to the right of the text when value ≠ clean baseline
    if (std::abs(value_ - cleanVal_) > kDirtyEps) {
        p.setBrush(QColor("#4a90e2"));
        p.setPen(Qt::NoPen);
        const int dotX = textX + textW + 4;
        const int dotY = textY - fm.ascent() / 2;
        p.drawEllipse(dotX, dotY, 6, 6);
    }
}

void AudioControl::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        startDragY_   = e->pos().y();
        startDragVal_ = value_;
        dragging_     = true;
        setFocus();
    }
}

void AudioControl::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_) return;
    const double delta = static_cast<double>(startDragY_ - e->pos().y());
    clampAndEmit(startDragVal_ + delta * step_ / static_cast<double>(kPixelsPerStep));
}

void AudioControl::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        dragging_ = false;
}

void AudioControl::wheelEvent(QWheelEvent* e) {
    clampAndEmit(value_ + e->angleDelta().y() / 120.0 * step_);
    e->accept();
}

void AudioControl::mouseDoubleClickEvent(QMouseEvent*) {
    lineEdit_->setGeometry(0, height() - kLabelH, width(), kLabelH);
    lineEdit_->setText(QString::number(value_, 'f', 1));
    lineEdit_->show();
    lineEdit_->setFocus();
    lineEdit_->selectAll();
}

} // namespace gui
```

- [ ] **Step 3: Build to verify compilation (no tests yet — widget base class needs subclass)**

We can't build a useful binary yet since `AudioControl` is abstract. Skip build and proceed to Task 2.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/AudioControl.h gui/AudioControl.cpp
git commit -m "feat: add AudioControl base class (drag/wheel/label/dirty-dot)"
```

---

### Task 2: `RotaryKnob`, `VerticalFader`, and CMakeLists update

**Files:**
- Create: `gui/RotaryKnob.h`
- Create: `gui/RotaryKnob.cpp`
- Create: `gui/VerticalFader.h`
- Create: `gui/VerticalFader.cpp`
- Modify: `gui/CMakeLists.txt`

- [ ] **Step 1: Write `gui/RotaryKnob.h`**

```cpp
#pragma once

#include "AudioControl.h"

namespace gui {

// Rotary knob: 270° arc from 7 o'clock (min) to 5 o'clock (max).
class RotaryKnob : public AudioControl {
    Q_OBJECT
public:
    explicit RotaryKnob(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintControl(QPainter& p, const QRect& r) override;
};

} // namespace gui
```

- [ ] **Step 2: Write `gui/RotaryKnob.cpp`**

```cpp
#include "RotaryKnob.h"

#include <cmath>

#include <QPainter>

namespace gui {

// Arc geometry: start at 225° (between 7 and 8 o'clock, Qt CCW convention)
// sweeping -270° CW through 12 o'clock to ~5 o'clock.
// Qt angle convention: 0° = 3 o'clock, positive = CCW. Stored in 1/16 degrees.
static constexpr int kArcStartQt  =  225 * 16;   // 225° CCW from 3 o'clock = 7:30 position
static constexpr int kArcSpanQt   = -270 * 16;   // 270° CW sweep

RotaryKnob::RotaryKnob(QWidget* parent)
    : AudioControl(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize RotaryKnob::sizeHint() const {
    return QSize(56, 56 + kLabelH);
}

void RotaryKnob::paintControl(QPainter& p, const QRect& r) {
    const double norm = (maxVal_ > minVal_)
                        ? (value_ - minVal_) / (maxVal_ - minVal_)
                        : 0.0;

    const int cx     = r.center().x();
    const int cy     = r.center().y();
    const int radius = std::min(r.width(), r.height()) / 2 - 4;
    const QRect arcRect(cx - radius, cy - radius, radius * 2, radius * 2);

    // Background groove arc
    QPen arcPen(QColor("#cccccc"), 4, Qt::SolidLine, Qt::FlatCap);
    p.setPen(arcPen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(arcRect, kArcStartQt, kArcSpanQt);

    // Value arc (filled proportion)
    arcPen.setColor(QColor("#2a6099"));
    p.setPen(arcPen);
    const int valSpan = static_cast<int>(norm * kArcSpanQt);  // kArcSpanQt is negative → CW
    p.drawArc(arcRect, kArcStartQt, valSpan);

    // Centre circle
    p.setBrush(QColor("#3a3a3a"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(cx, cy), 12, 12);

    // Pointer line from centre to arc edge
    // kArcStartQt is 225 * 16; at norm=0 angle=225°, at norm=1 angle=225-270=-45°=315°
    const double angleDeg = 225.0 - norm * 270.0;
    const double angleRad = angleDeg * M_PI / 180.0;
    const int px = cx + static_cast<int>((radius - 2) * std::cos(angleRad));
    const int py = cy - static_cast<int>((radius - 2) * std::sin(angleRad));
    p.setPen(QPen(QColor("#ffffff"), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(cx, cy, px, py);
}

} // namespace gui
```

- [ ] **Step 3: Write `gui/VerticalFader.h`**

```cpp
#pragma once

#include "AudioControl.h"

namespace gui {

// Vertical fader: track with a thumb. Bottom = min, top = max.
class VerticalFader : public AudioControl {
    Q_OBJECT
public:
    explicit VerticalFader(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintControl(QPainter& p, const QRect& r) override;
};

} // namespace gui
```

- [ ] **Step 4: Write `gui/VerticalFader.cpp`**

```cpp
#include "VerticalFader.h"

#include <QPainter>

namespace gui {

static constexpr int kThumbW = 20;
static constexpr int kThumbH = 10;
static constexpr int kTrackW =  3;

VerticalFader::VerticalFader(QWidget* parent)
    : AudioControl(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
}

QSize VerticalFader::sizeHint() const {
    return QSize(30, 80 + kLabelH);
}

void VerticalFader::paintControl(QPainter& p, const QRect& r) {
    const double norm = (maxVal_ > minVal_)
                        ? (value_ - minVal_) / (maxVal_ - minVal_)
                        : 0.0;

    const int cx = r.center().x();

    // Vertical groove
    const int trackX = cx - kTrackW / 2;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#cccccc"));
    p.drawRect(trackX, r.top() + kThumbH / 2, kTrackW, r.height() - kThumbH);

    // Zero-crossing tick (if range spans 0)
    if (minVal_ < 0.0 && maxVal_ > 0.0) {
        const double normZero = -minVal_ / (maxVal_ - minVal_);
        const int yZero = r.bottom() - static_cast<int>(normZero * r.height());
        p.setPen(QPen(QColor("#888888"), 1));
        p.drawLine(cx - 6, yZero, cx + 6, yZero);
    }

    // Thumb: rounded rectangle centered on current value position
    const int thumbY = r.bottom() - static_cast<int>(norm * r.height()) - kThumbH / 2;
    const QRect thumbRect(cx - kThumbW / 2, thumbY, kThumbW, kThumbH);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#2a6099"));
    p.drawRoundedRect(thumbRect, 3, 3);
}

} // namespace gui
```

- [ ] **Step 5: Update `gui/CMakeLists.txt`**

Replace the `qt_add_executable` source list with:

```cmake
qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    AudioControl.h
    AudioControl.cpp
    RotaryKnob.h
    RotaryKnob.cpp
    VerticalFader.h
    VerticalFader.cpp
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

- [ ] **Step 6: Build to verify all three new widgets compile**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build. The existing `ChainPanel` still uses `QDoubleSpinBox` — that is fine and will be fixed in Task 3.

- [ ] **Step 7: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/RotaryKnob.h gui/RotaryKnob.cpp \
        gui/VerticalFader.h gui/VerticalFader.cpp \
        gui/CMakeLists.txt
git commit -m "feat: add RotaryKnob and VerticalFader widgets"
```

---

### Task 3: Swap `ChainPanel` spinboxes for knobs/faders

**Files:**
- Replace: `gui/ChainPanel.h`
- Replace: `gui/ChainPanel.cpp`

- [ ] **Step 1: Replace `gui/ChainPanel.h`**

```cpp
#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QGroupBox;
class QLabel;
QT_END_NAMESPACE

namespace gui {
class AudioControl;
class RotaryKnob;
class VerticalFader;
}

namespace gui {

class ChainPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChainPanel(QWidget* parent = nullptr);

    // Populate all controls and inline readouts from advice + analysis + preset.
    // Stores advice as the clean baseline; widgets clear dirty dots.
    void setAdvice(const mt::AdviceSet& advice,
                   const mt::AnalysisSnapshot& snap,
                   const mt::PresetData& preset);

    // Returns current AdviceSet: non-editable fields from stored baseline,
    // editable fields from current control values.
    mt::AdviceSet currentAdvice() const;

    // Writes bypass states into opts (one flag per checkable QGroupBox).
    void populateBypassFlags(mt::RenderOptions& opts) const;

    // Restore controls to last-advised baseline and clear dirty dots.
    void resetToAdvice();

    // Reset to pre-analysis state: default values, "—" readouts.
    void clear();

signals:
    // Emitted when any control value or bypass checkbox changes.
    void overrideChanged(const mt::AdviceSet& advice);

private:
    void buildUi();
    void applyToControls(const mt::AdviceSet& adv);
    void emitOverride();

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    mt::AdviceSet autoAdvice_;
    bool          hasAdvice_ = false;

    // EQ section (Tier 1)
    QGroupBox*    eqBox_                = nullptr;
    QLabel*       eqReadouts_[kNumBands]{};
    VerticalFader* eqGainFaders_[kNumBands]{};

    // Multiband Comp section (Tier 2, col 0-1)
    QGroupBox* mbBox_      = nullptr;
    QLabel*    mbCrestLbl_ = nullptr;

    // Stereo Width section (Tier 2, col 2-3)
    QGroupBox* widthBox_     = nullptr;
    QLabel*    widthCorrLbl_ = nullptr;

    // Saturator section (Tier 2, col 4-5)
    QGroupBox*  satBox_       = nullptr;
    QLabel*     satCrestLbl_  = nullptr;
    RotaryKnob* satDriveKnob_ = nullptr;

    // Mixbus Comp section (Tier 3, col 0-3)
    QGroupBox*  mixbusBox_        = nullptr;
    RotaryKnob* mixbusThreshKnob_ = nullptr;
    RotaryKnob* mixbusMakeupKnob_ = nullptr;
    QLabel*     mixbusRmsLbl_     = nullptr;

    // Limiter section (Tier 3, col 4-5)
    QGroupBox*    limBox_          = nullptr;
    VerticalFader* limCeilingFader_ = nullptr;
    QLabel*       limPeakLbl_      = nullptr;
};

} // namespace gui
```

- [ ] **Step 2: Replace `gui/ChainPanel.cpp`**

```cpp
#include "ChainPanel.h"
#include "AudioControl.h"
#include "RotaryKnob.h"
#include "VerticalFader.h"

#include "mastertweak/analysis.hpp"

#include <cmath>

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
    for (int c = 0; c < 6; ++c)
        grid->setColumnStretch(c, 1);

    // ── Tier 1: EQ ────────────────────────────────────────────────────────────
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

            auto* readout = new QLabel(QString::fromUtf8("\xe2\x80\x94"), eqBox_);
            readout->setAlignment(Qt::AlignHCenter);
            readout->setStyleSheet("font-size: 9pt; color: #555;");
            eqReadouts_[i] = readout;
            col->addWidget(readout);

            auto* fdr = new VerticalFader(eqBox_);
            fdr->setRange(-12.0, 12.0);
            fdr->setSingleStep(0.5);
            fdr->setSuffix(" dB");
            fdr->setValue(0.0);
            eqGainFaders_[i] = fdr;
            col->addWidget(fdr, 0, Qt::AlignHCenter);

            hbox->addLayout(col);

            connect(fdr, &AudioControl::valueChanged,
                    this, [this](double) { emitOverride(); });
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
        mbCrestLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), mbBox_);
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
        widthCorrLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), widthBox_);
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
        satCrestLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), satBox_);
        vbox->addWidget(satCrestLbl_);

        auto* drvLbl = new QLabel("Drive", satBox_);
        drvLbl->setAlignment(Qt::AlignHCenter);
        vbox->addWidget(drvLbl);

        satDriveKnob_ = new RotaryKnob(satBox_);
        satDriveKnob_->setRange(0.0, 6.0);
        satDriveKnob_->setSingleStep(0.5);
        satDriveKnob_->setSuffix(" dB");
        satDriveKnob_->setValue(0.0);
        vbox->addWidget(satDriveKnob_, 0, Qt::AlignHCenter);
        vbox->addStretch();

        connect(satDriveKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(satBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(satBox_, 1, 4, 1, 2);
    }

    // ── Tier 3 col 0-3: Mixbus Comp ──────────────────────────────────────────
    {
        mixbusBox_ = new QGroupBox("Mixbus Comp", this);
        mixbusBox_->setCheckable(true);
        mixbusBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(mixbusBox_);
        auto* row  = new QHBoxLayout;

        auto* thrCol = new QVBoxLayout;
        auto* thrLbl = new QLabel("Thr", mixbusBox_);
        thrLbl->setAlignment(Qt::AlignHCenter);
        thrCol->addWidget(thrLbl);
        mixbusThreshKnob_ = new RotaryKnob(mixbusBox_);
        mixbusThreshKnob_->setRange(-40.0, 0.0);
        mixbusThreshKnob_->setSingleStep(1.0);
        mixbusThreshKnob_->setSuffix(" dB");
        mixbusThreshKnob_->setValue(-20.0);
        thrCol->addWidget(mixbusThreshKnob_, 0, Qt::AlignHCenter);
        row->addLayout(thrCol);

        auto* mkupCol = new QVBoxLayout;
        auto* mkupLbl = new QLabel("Mkup", mixbusBox_);
        mkupLbl->setAlignment(Qt::AlignHCenter);
        mkupCol->addWidget(mkupLbl);
        mixbusMakeupKnob_ = new RotaryKnob(mixbusBox_);
        mixbusMakeupKnob_->setRange(-12.0, 12.0);
        mixbusMakeupKnob_->setSingleStep(0.5);
        mixbusMakeupKnob_->setSuffix(" dB");
        mixbusMakeupKnob_->setValue(0.0);
        mkupCol->addWidget(mixbusMakeupKnob_, 0, Qt::AlignHCenter);
        row->addLayout(mkupCol);

        row->addStretch();
        vbox->addLayout(row);

        mixbusRmsLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), mixbusBox_);
        mixbusRmsLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(mixbusRmsLbl_);

        connect(mixbusThreshKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(mixbusMakeupKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(mixbusBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(mixbusBox_, 2, 0, 1, 4);
    }

    // ── Tier 3 col 4-5: Limiter ──────────────────────────────────────────────
    {
        limBox_ = new QGroupBox("Limiter", this);
        limBox_->setCheckable(true);
        limBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(limBox_);

        auto* ceilLbl = new QLabel("Ceiling", limBox_);
        ceilLbl->setAlignment(Qt::AlignHCenter);
        vbox->addWidget(ceilLbl);

        limCeilingFader_ = new VerticalFader(limBox_);
        limCeilingFader_->setRange(-6.0, 0.0);
        limCeilingFader_->setSingleStep(0.5);
        limCeilingFader_->setSuffix(" dBTP");
        limCeilingFader_->setValue(-1.0);
        vbox->addWidget(limCeilingFader_);

        limPeakLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), limBox_);
        limPeakLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(limPeakLbl_);

        connect(limCeilingFader_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(limBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(limBox_, 2, 4, 1, 2);
    }

    outer->addLayout(grid);
}

// ─────────────────────────────────────────────────────────────────────────────

void ChainPanel::applyToControls(const mt::AdviceSet& adv) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const double v = static_cast<double>(adv.eq[si].gainDb);
        eqGainFaders_[i]->blockSignals(true);
        eqGainFaders_[i]->setValue(v);
        eqGainFaders_[i]->setClean(v);
        eqGainFaders_[i]->blockSignals(false);
    }
    auto setCtrl = [](AudioControl* c, double v) {
        c->blockSignals(true);
        c->setValue(v);
        c->setClean(v);
        c->blockSignals(false);
    };
    setCtrl(satDriveKnob_,     static_cast<double>(adv.saturator.driveDb));
    setCtrl(mixbusThreshKnob_, static_cast<double>(adv.mixbusComp.thresholdDb));
    setCtrl(mixbusMakeupKnob_, static_cast<double>(adv.mixbusComp.makeupDb));
    setCtrl(limCeilingFader_,  static_cast<double>(adv.limiter.ceilingDb));
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

    applyToControls(advice);

    // EQ per-band readouts: "preset target → measured RMS"
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqReadouts_[i]->setText(
            QString("%1\xe2\x86\x92%2 dB")
                .arg(static_cast<int>(std::round(preset.bandRmsDb[si])))
                .arg(static_cast<int>(std::round(snap.bands[si].avgRmsDb))));
    }

    float crestSum = 0.f;
    for (int i = 0; i < kNumBands; ++i)
        crestSum += snap.bands[static_cast<size_t>(i)].crestDb;
    const float avgCrest = crestSum / static_cast<float>(kNumBands);

    mbCrestLbl_->setText(
        QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));
    satCrestLbl_->setText(
        QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));
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
        adv.eq[si].gainDb = static_cast<float>(eqGainFaders_[i]->value());
    }
    adv.limiter.ceilingDb      = static_cast<float>(limCeilingFader_->value());
    adv.saturator.driveDb      = static_cast<float>(satDriveKnob_->value());
    adv.mixbusComp.thresholdDb = static_cast<float>(mixbusThreshKnob_->value());
    adv.mixbusComp.makeupDb    = static_cast<float>(mixbusMakeupKnob_->value());
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
    applyToControls(autoAdvice_);

    for (QGroupBox* box : {eqBox_, mbBox_, widthBox_, satBox_, mixbusBox_, limBox_}) {
        box->blockSignals(true);
        box->setChecked(true);
        box->blockSignals(false);
    }
}

void ChainPanel::clear() {
    hasAdvice_  = false;
    autoAdvice_ = mt::AdviceSet{};

    applyToControls(autoAdvice_);  // resets to defaults, clears dirty dots

    const QString dash = QString::fromUtf8("\xe2\x80\x94");
    for (int i = 0; i < kNumBands; ++i)
        eqReadouts_[i]->setText(dash);
    mbCrestLbl_->setText(dash);
    widthCorrLbl_->setText(dash);
    satCrestLbl_->setText(dash);
    mixbusRmsLbl_->setText(dash);
    limPeakLbl_->setText(dash);
}

} // namespace gui
```

- [ ] **Step 3: Build and run tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: clean build (zero warnings), all tests pass.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/ChainPanel.h gui/ChainPanel.cpp
git commit -m "feat: replace spinboxes with RotaryKnob/VerticalFader in ChainPanel"
```

---

### Task 4: Update TODO + final verification

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Update `TODO.md`**

Find the IN PROGRESS GUI line:
```
- ~~Make the GUI sound-engineer friendly~~ **IN PROGRESS** — Section layout done (ChainPanel: 6 bypassable QGroupBox sections in a 3-tier grid, inline analysis readouts per section). Remaining: custom knob/fader widgets, VU meter.
```

Replace with:
```
- ~~Make the GUI sound-engineer friendly~~ **IN PROGRESS** — Section layout done (ChainPanel: 6 bypassable sections). Knob/fader widgets done (RotaryKnob + VerticalFader via AudioControl base). Remaining: VU meter for playback.
```

- [ ] **Step 2: Run full test suite**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass.

- [ ] **Step 3: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add TODO.md
git commit -m "docs: update TODO — knob/fader widgets done, VU meter remains"
```
