# Transport Clock & Scrub Bar — Design Spec

**Date:** 2026-05-29  
**Feature:** Playback position clock and seekable progress bar in the TransportWidget

---

## Overview

Add a large monospace position clock (`MM:SS / MM:SS`) and a horizontal scrub bar to `TransportWidget`. The VU meter spans the full height of both rows. All changes are inline in `TransportWidget` — no new files.

---

## Layout

```
QHBoxLayout (outer)
  ├─ QVBoxLayout (left, stretch=1)
  │    ├─ Row 1 — QHBoxLayout: [▶ Play]  [A/B]  filename.wav
  │    └─ Row 2 — QHBoxLayout: [00:14 / 03:42]  [══════●══════════════]
  └─ VUMeterWidget (right, fixed width, full height of both rows)
```

`VUMeterWidget` already has `QSizePolicy::Fixed` (horizontal) and `QSizePolicy::Expanding` (vertical) — no changes needed. It naturally fills both rows' combined height.

---

## New Members on TransportWidget

| Member | Type | Purpose |
|---|---|---|
| `totalFrames_` | `uint64_t` | Total frames of loaded file; set in `loadFile()`, read main-thread only |
| `sampleRate_` | `uint32_t` | Sample rate of loaded file; set in `loadFile()` |
| `posTimer_` | `QTimer*` | 10 Hz timer that updates clock label and slider |
| `clockLabel_` | `QLabel*` | Monospace time display |
| `scrubSlider_` | `QSlider*` | Horizontal seek bar |

`playbackFrame_` is changed from `uint64_t` to `std::atomic<uint64_t>`. The audio callback writes with `memory_order_relaxed`; the 10 Hz timer reads with `memory_order_relaxed`. This fixes a pre-existing data race.

`PlaybackCtx::framePos` changes from `uint64_t*` to `std::atomic<uint64_t>*` to match.

---

## Data Flow

**At `loadFile()`:**
1. Open a temporary `ma_decoder` (f32, native channels/rate), call `ma_decoder_get_length_in_pcm_frames()` to get `totalFrames_` and read `outputSampleRate` into `sampleRate_`, then `ma_decoder_uninit` immediately. This gives total duration before first play.
2. Set `scrubSlider_->setRange(0, static_cast<int>(totalFrames_))` (capped at `INT_MAX` for files > ~12 hours at 48 kHz — not a practical concern).
3. Set `clockLabel_` to `"00:00 / " + formatTime(totalFrames_, sampleRate_)`.
4. If `totalFrames_ == 0` (unknown length): disable slider, show `"--:-- / --:--"`.

**At `unload()`:** reset slider range to `[0, 1]`, label to `"--:-- / --:--"`, disable slider.

**10 Hz `posTimer_` tick:**
1. Read `playbackFrame_.load(memory_order_relaxed)`.
2. If `!scrubSlider_->isSliderDown()`: call `scrubSlider_->setValue(frame)`.
3. Compute display position: `pos = isSliderDown ? scrubSlider_->value() : frame`.
4. Update `clockLabel_->setText(formatTime(pos, sampleRate_) + " / " + formatTime(totalFrames_, sampleRate_))`.

The `isSliderDown()` guard on step 2 prevents the timer from fighting the user's drag. During drag, the clock still updates (step 3 reads the slider value), giving a live preview of the target position.

`posTimer_` starts in `play()` and stops in `cleanup()`.

---

## Scrubbing

`QSlider::sliderReleased()` is connected to a new private slot `onScrubReleased()`:

```
onScrubReleased():
  uint64_t frame = static_cast<uint64_t>(scrubSlider_->value())
  bool wasPlaying = playing_
  if wasPlaying: cleanup()      // stops device, zeros atomics, stops timers
  playbackFrame_ = frame
  if wasPlaying: play()         // restarts from new position
```

Seeking while stopped: sets `playbackFrame_` only; position is remembered for next `play()`.

---

## Clock Display

**Helper function** (free function in `TransportWidget.cpp`):

```cpp
static QString formatTime(uint64_t frames, uint32_t sampleRate) {
    if (sampleRate == 0) return "--:--";
    uint64_t totalSecs = frames / sampleRate;
    uint64_t h  = totalSecs / 3600;
    uint64_t m  = (totalSecs % 3600) / 60;
    uint64_t s  = totalSecs % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}
```

**Styling:**
- Font: `QFont("monospace", 14, QFont::Bold)`
- Fixed width: set via `clockLabel_->setFixedWidth(fontMetrics().horizontalAdvance("0:00:00 / 0:00:00"))` after font is applied
- Text colour: `#ffffff` via `setStyleSheet("color: #ffffff;")`
- Initial text: `"--:-- / --:--"`

---

## Slider Styling (QSS)

Applied to `scrubSlider_` via `setStyleSheet()`:

```css
QSlider::groove:horizontal {
    height: 4px;
    background: #3a3a3a;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #2a6099;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 12px;
    height: 12px;
    margin: -4px 0;
    background: #ffffff;
    border-radius: 6px;
}
```

---

## Edge Cases

| Case | Behaviour |
|---|---|
| `totalFrames_ == 0` (unknown) | Slider disabled, clock shows `"--:-- / --:--"` |
| Seek while stopped | Sets `playbackFrame_`, no playback started |
| Seek past end | Slider clamped to `[0, totalFrames_]` by QSlider range |
| File < 1 second | Shows `"00:00 / 00:00"` — acceptable |
| A/B toggle + scrub | Both use `cleanup()` → `play()` path, no conflict |

---

## Modified Files

| File | Change |
|---|---|
| `gui/TransportWidget.h` | `playbackFrame_` → `std::atomic<uint64_t>`; new members: `totalFrames_`, `sampleRate_`, `posTimer_`, `clockLabel_`, `scrubSlider_` |
| `gui/TransportWidget.cpp` | Layout restructure; `formatTime()` helper; `loadFile()`/`unload()` updates; `posTimer_` lifecycle; `onScrubReleased()` slot; `PlaybackCtx::framePos` type change |
| `gui/CMakeLists.txt` | No change (no new files) |

---

## Out of Scope

- Waveform thumbnail in scrub bar
- Loop region markers
- SMPTE timecode display
- Millisecond precision

---

## Testing

No new unit tests. Manual verification:
- Load a file → total duration displayed, slider enabled
- Play → clock advances, slider tracks position
- Drag slider while playing → clock shows preview position; on release, playback resumes from new position
- Drag slider while stopped → position remembered, next play() starts from dragged position
- Unload → clock resets to `"--:-- / --:--"`, slider disabled
