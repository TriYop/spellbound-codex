# VU Meter Widget — Design Spec

**Date:** 2026-05-28  
**Feature:** Classic stereo VU meter integrated into the TransportWidget playback preview

---

## Overview

Add a stereo VU meter to the `TransportWidget` that displays real-time L/R channel levels during playback preview. Classic VU ballistics (300 ms integration), displayed as two vertical bars inside the existing transport row.

---

## New Files

- `gui/VUMeterWidget.h`
- `gui/VUMeterWidget.cpp`

---

## Data Flow

`TransportWidget` owns two `std::atomic<float>` members (`atomicRmsL_`, `atomicRmsR_`), zero-initialized. The miniaudio `dataCallback` (audio thread) computes per-channel RMS over each callback buffer and writes into these atomics with `store(..., std::memory_order_relaxed)`.

`VUMeterWidget` holds non-owning `const std::atomic<float>*` pointers to those atomics. It owns a `QTimer` at 30 Hz that reads both atomics, applies ballistic smoothing, and calls `update()`. The widget is a child of `TransportWidget` (destroyed first), so the pointer lifetime is always valid.

On `cleanup()` and `unload()`, `TransportWidget` writes `0.0f` to both atomics so the meter decays naturally to the bottom.

---

## RMS Calculation (dataCallback)

The decoder output format is already float. For each channel `c` in `[0, channels)`:

```
sum_c += sample[frame * channels + c]²
rms_c = sqrt(sum_c / framesRead)
```

- If `channels == 1`: store the same value to both atomics.
- If `channels > 2`: use channels 0 and 1 only.
- Clamp `rms` to a minimum of `1e-7f` before any log conversion (prevents log(0)).

---

## VU Ballistics

Timer period: `dt = 1/30 s`.  
Time constant: `τ = 300 ms`.  
EMA coefficient: `α = 1 − exp(−dt / τ) ≈ 0.105`.

Each tick:
```
smoothedL_ = α * newRmsL + (1 − α) * smoothedL_
smoothedR_ = α * newRmsR + (1 − α) * smoothedR_
```

When stopped, atomics are zeroed and the EMA decays the display to silence over ~1 s — no special reset logic needed.

---

## Scale

- **Reference:** `0 VU = −18 dBFS` (EBU standard, consistent with pipeline LUFS targets)
- **Display range:** `−20 VU` (bottom) to `+3 VU` (top) → `−38 dBFS` to `−15 dBFS`
- **Conversion:** `vu = 20 * log10(rms) + 18`
- **Bar fill:** linear in VU units across the displayed range

**Colour zones:**
| Range | Colour |
|---|---|
| −20 to −3 VU | Green |
| −3 to 0 VU | Yellow |
| 0 to +3 VU | Red |

A slightly thicker/brighter tick mark at `0 VU`.

---

## Widget Layout and Painting

**`sizeHint()`:** `QSize(40, 80)` — two 14 px wide bars, 4 px gap, 4 px padding each side.  
Fixed width; may expand vertically with the layout.

**`paintEvent` order:**
1. Dark background fill (`#1a1a1a`)
2. Per channel (L then R): fill bar from bottom up to current smoothed level in zone colours
3. Channel labels "L" / "R" at the bottom of each bar (10 px font, white)
4. Scale ticks on the right edge at −20, −10, −3, 0, +3 VU; 0 VU tick is thicker

**Timer:** starts in constructor, runs continuously. No start/stop plumbing required.

**Placement:** `VUMeterWidget` is added to `TransportWidget`'s existing `QHBoxLayout` after the file label.

---

## Edge Cases

| Case | Behaviour |
|---|---|
| Mono file | Both bars show same level (channel 0 → both atomics) |
| Silence / near-silence | RMS clamped to 1e-7f; meter sits at bottom |
| Not playing | Atomics are 0; meter at bottom |
| A/B switch mid-play | `cleanup()` zeroes atomics; meter briefly dips then recovers on resumed play |

---

## Out of Scope

- Peak hold dot (easy to add later as a one-liner extension)
- LUFS display during playback (separate feature; `LufsAnalyser` is batch-only)
- Clip indicator

---

## Testing

No new unit tests. The ballistics math is trivial and the widget is paint-only. Manual verification:
- Play a rendered file → L/R bars respond to audio
- Stop → bars decay to zero over ~1 s
- A/B switch → brief dip, then recovery
- 0 VU reference mark is visible at the correct position
