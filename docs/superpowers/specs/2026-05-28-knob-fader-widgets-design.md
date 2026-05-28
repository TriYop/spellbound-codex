# Custom Knob & Fader Widgets

**Date:** 2026-05-28  
**Status:** Approved  
**Scope:** Replace all `QDoubleSpinBox` controls in `ChainPanel` with custom `RotaryKnob` and `VerticalFader` widgets.

---

## Problem

`ChainPanel` uses `QDoubleSpinBox` for all editable DSP parameters. Spinboxes are precise but require clicking arrows or typing to change values — not ergonomic for live parameter adjustment during playback. The TODO calls for faders on level controls and rotary knobs on all other controls.

---

## New Widgets

### `AudioControl` (base class)

`gui/AudioControl.h/.cpp` — all shared logic. `RotaryKnob` and `VerticalFader` inherit and add only `paintControl()`.

**Public interface:**

```cpp
class AudioControl : public QWidget {
    Q_OBJECT
public:
    explicit AudioControl(QWidget* parent = nullptr);

    void   setValue(double v);
    double value() const;
    void   setRange(double min, double max);
    void   setSingleStep(double step);
    void   setClean(double baseline);   // sets dirty-dot baseline
    void   setSuffix(const QString& s); // e.g. " dB", " dBTP"

signals:
    void valueChanged(double value);

protected:
    // Subclass paints its control visual inside controlRect.
    // controlRect = widget rect minus the bottom kLabelH-px label strip.
    virtual void paintControl(QPainter& p, const QRect& controlRect) = 0;

    double value_    = 0.0;
    double minVal_   = 0.0;
    double maxVal_   = 1.0;
    double step_     = 0.1;
    double cleanVal_ = 0.0;
    QString suffix_;

private:
    void paintEvent(QPaintEvent*) override final;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

    void clampAndEmit(double v);

    double startDragVal_ = 0.0;
    int    startDragY_   = 0;
    bool   dragging_     = false;

    static constexpr double kDirtyEps     = 1e-9;
    static constexpr int    kLabelH       = 20;   // px reserved at bottom for value text
    static constexpr int    kPixelsPerStep = 2;   // drag pixels per single step
};
```

**`paintEvent` (base class, `final`):**
1. Calls `paintControl(p, QRect(0, 0, width(), height() - kLabelH))` — subclass draws knob/fader here.
2. Draws value label in the bottom `kLabelH`-px strip: formatted as `QString("%1%2").arg(value_, 0, 'f', 1).arg(suffix_)` with a leading `+` for non-negative values (e.g. `"+3.0 dB"`, `"-1.0 dBTP"`). Centered horizontally.
3. If `std::abs(value_ - cleanVal_) > kDirtyEps`, draws a small filled circle `●` (6 px diameter, `#4a90e2`) 4 px to the right of the value text.

**Drag interaction:**
- `mousePressEvent`: record `startDragY = event->pos().y()`, `startDragVal = value_`, `dragging = true`. Call `setFocus()`.
- `mouseMoveEvent`: if dragging, `delta = startDragY - event->pos().y()` (up = positive). `newVal = startDragVal + delta * step_ / kPixelsPerStep`. Call `clampAndEmit(newVal)`.
- `mouseReleaseEvent`: `dragging = false`.
- `wheelEvent`: `clampAndEmit(value_ + event->angleDelta().y() / 120.0 * step_)`.

**Double-click to type:**
- Constructor: create a `QLineEdit* lineEdit_` child, initially hidden, geometry = bottom `kLabelH`-px strip.
- `mouseDoubleClickEvent`: set `lineEdit_->setText(QString::number(value_, 'f', 1))`, show it, give it focus, select all.
- On `lineEdit_->returnPressed` or `lineEdit_->editingFinished`: parse text as double, clamp to `[minVal_, maxVal_]`, call `setValue(parsed)` + `emit valueChanged(value_)`, hide `lineEdit_`. Connect in constructor.

**`clampAndEmit`:** `value_ = std::clamp(v, minVal_, maxVal_)`, `update()`, `emit valueChanged(value_)`.

---

### `RotaryKnob`

`gui/RotaryKnob.h/.cpp` — `paintControl` only.

```cpp
class RotaryKnob : public AudioControl {
    Q_OBJECT
public:
    explicit RotaryKnob(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintControl(QPainter& p, const QRect& r) override;
};
```

**Visual (270° arc, 7 o'clock → 5 o'clock):**
- Background groove arc: grey `#cccccc`, 4 px pen, from 225° spanning 270° CCW.
- Value arc: dark `#2a6099`, same pen, from 225° spanning `normalised * 270°` CCW (normalised = (value - min) / (max - min)).
- Centre circle: filled `#3a3a3a`, radius ~12 px.
- Pointer line from centre to arc edge at the current angle: white `#ffffff`, 2 px.
- `sizeHint()`: `QSize(56, 56 + kLabelH)` → (56, 76).

---

### `VerticalFader`

`gui/VerticalFader.h/.cpp` — `paintControl` only.

```cpp
class VerticalFader : public AudioControl {
    Q_OBJECT
public:
    explicit VerticalFader(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintControl(QPainter& p, const QRect& r) override;
};
```

**Visual:**
- Vertical groove: grey `#cccccc`, 3 px wide, centered horizontally in `r`.
- If range spans 0: draw a small tick mark at the 0 position in `#888888`.
- Thumb: rounded rectangle, 20 × 10 px, `#2a6099` fill, centered horizontally, y position = `r.bottom() - normalised * r.height()` (bottom = min, top = max).
- `sizeHint()`: `QSize(30, 80 + kLabelH)` → (30, 100).

---

## ChainPanel Changes

Replace all `QDoubleSpinBox*` members with `RotaryKnob*` / `VerticalFader*`. Remove the `cleanValues_` struct and `updateTint()` / `clearAllTints()` helpers — widgets self-manage their dirty dot via `setClean()`.

**Member replacements:**

| Old | New |
|---|---|
| `QDoubleSpinBox* eqGainSpins_[7]` | `VerticalFader* eqGainFaders_[7]` |
| `QDoubleSpinBox* satDriveSpin_` | `RotaryKnob* satDriveKnob_` |
| `QDoubleSpinBox* mixbusThreshSpin_` | `RotaryKnob* mixbusThreshKnob_` |
| `QDoubleSpinBox* mixbusMakeupSpin_` | `RotaryKnob* mixbusMakeupKnob_` |
| `QDoubleSpinBox* limCeilingSpin_` | `VerticalFader* limCeilingFader_` |
| `CleanValues cleanValues_` | *(removed — widgets self-track)* |

**`applyToSpins(adv)` → `applyToControls(adv)`:**
```cpp
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
        c->setValue(v); c->setClean(v);
        c->blockSignals(false);
    };
    setCtrl(satDriveKnob_,      static_cast<double>(adv.saturator.driveDb));
    setCtrl(mixbusThreshKnob_,  static_cast<double>(adv.mixbusComp.thresholdDb));
    setCtrl(mixbusMakeupKnob_,  static_cast<double>(adv.mixbusComp.makeupDb));
    setCtrl(limCeilingFader_,   static_cast<double>(adv.limiter.ceilingDb));
}
```

**`currentAdvice()`:** reads `widget->value()` instead of `spin->value()`.

**`clear()`:** calls `applyToControls(mt::AdviceSet{})` — resets to defaults, clears dirty dots. Sets readout labels to "—" as before.

**`resetToAdvice()`:** `if (!hasAdvice_) return; applyToControls(autoAdvice_);` — restores bypass boxes to checked too (unchanged logic).

**Connections in `buildUi()`:**
```cpp
connect(fader, &AudioControl::valueChanged, this, [this](double) { emitOverride(); });
connect(knob,  &AudioControl::valueChanged, this, [this](double) { emitOverride(); });
```

---

## File Changes

| Action | File |
|--------|------|
| Create | `gui/AudioControl.h` |
| Create | `gui/AudioControl.cpp` |
| Create | `gui/RotaryKnob.h` |
| Create | `gui/RotaryKnob.cpp` |
| Create | `gui/VerticalFader.h` |
| Create | `gui/VerticalFader.cpp` |
| Modify | `gui/ChainPanel.h` — swap spinbox members |
| Modify | `gui/ChainPanel.cpp` — swap buildUi + applyToControls + currentAdvice + clear |
| Modify | `gui/CMakeLists.txt` — add 6 new sources |

---

## Testing

No automated tests for Qt widgets. Manual checklist:

1. Build clean: `cmake --build build --parallel`
2. Launch: `./build/gui/MasterTweak`
3. Load audio + preset → EQ faders and knobs populate with advised values, no dirty dots visible
4. Drag an EQ fader → value label updates in real time, dirty dot appears
5. Scroll wheel on a knob → value changes by one step
6. Double-click value label → QLineEdit appears; type a value + Enter → applied and validated
7. Click Reset → all controls snap back to advised values, all dots disappear
8. Uncheck a QGroupBox (bypass) → section dims, controls inside are not interactive
9. All core tests still pass: `ctest --test-dir build --output-on-failure`
