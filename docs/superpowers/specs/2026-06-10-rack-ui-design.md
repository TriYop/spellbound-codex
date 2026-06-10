# Processor Rack UI — Design Spec

**Date:** 2026-06-10  
**Status:** Approved

---

## Overview

Redesign `ChainPanel` as a Carla-rack-inspired vertical processor rack. Each processor is a
full-width `RackUnit` strip that can be collapsed (header only) or bypassed (LED toggle),
stacked in signal-chain order. All advisor-derived parameters become overridable via
appropriate knobs and faders.

---

## 1. `RackUnit` widget (`gui/RackUnit.h/cpp`)

A reusable `QWidget` consisting of:

### 1.1 Header bar (fixed 36 px height)

`[LED] [▶/▼] [Title] ──── [inline stats] ──── [badge]`

| Element | Widget | Behaviour |
|---|---|---|
| **LED** | `QPushButton` 10×10 px, round, CSS-styled | Single-click → toggle bypass (green = active, red = bypassed) |
| **Collapse button** | `QToolButton` with `▼`/`▶` text | Single-click → toggle collapse |
| **Title** | `QLabel` | Read-only, uppercase, 11 pt |
| **Inline stats** | `QLabel` | Updated by `ChainPanel`; shows key values when collapsed |
| **Badge** | `QLabel` | `"advised"` (green) or `"auto"` (blue) |

**Collapse triggers:**
- Single-click on the collapse `▶/▼` button
- Double-click anywhere on the header strip

Single-click elsewhere on the header does **nothing** (no accidental collapse).

### 1.2 Body widget

Any `QWidget` passed at construction. `setVisible(false)` on collapse; unchanged on bypass.

### 1.3 API

```cpp
class RackUnit : public QWidget {
    Q_OBJECT
public:
    RackUnit(const QString& title, QWidget* body, QWidget* parent = nullptr);

    void setBypassed(bool);
    bool isBypassed() const;
    void setCollapsed(bool);
    bool isCollapsed() const;

    void setStats(const QString&);   // updates inline stats label
    void setBadge(const QString&, const QString& colour = "advised");

signals:
    void bypassChanged(bool bypassed);
    void collapseChanged(bool collapsed);
};
```

### 1.4 Collapse persistence

Each unit saves/restores its collapsed state in `QSettings`:
- Key: `ChainPanel/<unit-id>/collapsed`
- Applied in `RackUnit` constructor after `QSettings` read.

---

## 2. `ChainPanel` rebuild

`ChainPanel::buildUi()` replaces the `QGridLayout` of `QGroupBox` tiers with a
`QVBoxLayout` of 7 `RackUnit` instances in signal-chain order:

1. Resonance EQ
2. Parametric EQ
3. Multiband Comp
4. Saturator
5. Stereo Width
6. Mixbus Comp
7. Limiter

Each `RackUnit` receives a body `QWidget` built the same way as the current tier content,
extended with the new controls listed in Section 3.

`populateBypassFlags()` calls `unit->isBypassed()` instead of `QGroupBox::isChecked()`.

---

## 3. Controls per processor

### 3.1 Resonance EQ *(no change)*
Body: per-peak rows (freq / Q / gain label + enable checkbox), "No resonances detected" label.

### 3.2 Parametric EQ *(no change)*
Body: 7 `VerticalFader` (–12…+12 dB) with band-name labels and preset→measured readout.

### 3.3 Multiband Comp *(new — replaces "Advice-driven" label)*

7 per-band cells laid out in a horizontal row. Each cell (`QFrame`, dark background,
rounded border) contains:

```
┌─ Sub ──────┐
│  Thr       │
│ –18 dBFS   │
│ [fader]    │  ← VerticalFader  –40…0 dBFS
│ ─────────  │
│  Ratio     │
│  [knob]    │  ← RotaryKnob  1.0…8.0
│   2.1:1    │
└────────────┘
```

New members: `VerticalFader* mbThreshFaders_[7]`, `RotaryKnob* mbRatioKnobs_[7]`

`currentAdvice()` reads `mbComp[i].thresholdDb` from fader, `mbComp[i].ratio` from knob.

### 3.4 Saturator *(no change)*
Body: drive `RotaryKnob` (0…6 dB) + crest readout label.

### 3.5 Stereo Width *(new — replaces "Advice-driven" label)*

7 `VerticalFader` (0.0…2.0, unity mark at 1.0) with band-name labels. Fader track
colour: blue tint to distinguish from EQ faders.

New members: `VerticalFader* widthFaders_[7]`

`currentAdvice()` reads `width[i].width` from each fader.

### 3.6 Mixbus Comp *(adds ratio knob)*
Body: threshold `RotaryKnob` (–40…0 dB) + **ratio `RotaryKnob` (1.0…8.0)** (new) +
makeup `RotaryKnob` (–12…+12 dB) + RMS readout label.

New member: `RotaryKnob* mixbusRatioKnob_`

`currentAdvice()` additionally reads `mixbusComp.ratio` from the new knob.

### 3.7 Limiter *(adds target knob)*
Body: **target `RotaryKnob` (–23…–6 LUFS)** (new) + ceiling `VerticalFader` (–6…0 dBTP)
+ peak and LRA readout labels.

New member: `RotaryKnob* limTargetKnob_`

`currentAdvice()` additionally reads `limiter.targetLufsApprox` from the new knob.

---

## 4. Inline stats (collapsed header)

Each `RackUnit`'s stats label is updated in `ChainPanel::setAdvice()`:

| Processor | Stats string |
|---|---|
| Resonance EQ | `"2 peaks · deepest –8.4 dB @ 412 Hz"` |
| Parametric EQ | `"Sub +2.0  Lows –1.5  Mids +0.5  Highs +3.0  Air +1.5 dB"` |
| Multiband Comp | `"Sub –18/2.1  Mids –14/1.8  Air –10/2.8 · crest 9.2 dB"` |
| Saturator | `"drive 2.4 dB · crest 9.2 dB"` |
| Stereo Width | `"corr 0.72 · Lows 0.80×  Mids 1.10×  Highs 1.20×"` |
| Mixbus Comp | `"–18 dB / 2.4:1 / +2.0 mkup · rms –16.2 dBFS"` |
| Limiter | `"–10.0 LUFS · ceil –1.0 dBTP · peak –1.4 · LRA 12.3 LU"` |

Stats are also shown when the unit is expanded (the header is always visible).

---

## 5. MIDI accessors

New public accessors added to `ChainPanel` for MIDI wiring:

```cpp
RotaryKnob*    mbRatioKnob(int band)   const;  // band 0–6
VerticalFader* mbThreshFader(int band) const;  // band 0–6
VerticalFader* widthFader(int band)    const;  // band 0–6
RotaryKnob*    limTargetKnob()         const;
RotaryKnob*    mixbusRatioKnob()       const;
```

---

## 6. Files touched

| File | Change |
|---|---|
| `gui/RackUnit.h` | New widget |
| `gui/RackUnit.cpp` | New widget |
| `gui/ChainPanel.h` | Replace `QGroupBox*` fields with `RackUnit*`; add new control members and MIDI accessors |
| `gui/ChainPanel.cpp` | Rebuild `buildUi()` as rack; update `currentAdvice()`, `applyToControls()`, `resetToAdvice()`, `clear()`, `populateBypassFlags()`, `setAdvice()` |
| `gui/CMakeLists.txt` | Add `RackUnit.h/cpp` |
