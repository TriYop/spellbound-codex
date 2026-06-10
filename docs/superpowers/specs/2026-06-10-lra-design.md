# LRA (Loudness Range, EBU R128) — Design Spec

**Date:** 2026-06-10  
**Status:** Approved

---

## Overview

Add EBU R128 Loudness Range (LRA) to MasterTweak's analysis pipeline. LRA quantifies
macro-dynamics (P95 − P10 of gated short-term loudness blocks) and feeds into the
limiter advice: low-LRA material (hyper-limited pop) gets tighter limiting, high-LRA
material (orchestral, film) gets more transparent limiting.

---

## 1. Core DSP — `LufsAnalyser` extension

### New return type

```cpp
// lufs_analyser.hpp
struct LoudnessMetrics {
    float integratedLufs = -70.f;
    float lra            =   0.f;  // LU; 0 = not enough signal / silence
};
```

### New method

```cpp
LoudnessMetrics measureWithLra(const std::vector<std::vector<float>>& samples,
                                int numFrames);
```

**Algorithm:**

1. K-weight the full buffer (single pass — same two-stage biquad as `measure()`).
2. In one loop over the K-weighted buffer, accumulate **two** sets of block powers:
   - **400 ms / 100 ms hop** → integrated LUFS (existing logic).
   - **3 s / 100 ms hop** → LRA short-term blocks.
3. Apply absolute gate (−70 LUFS) to both sets.
4. **Integrated LUFS:** relative gate −10 LU → gated mean → LUFS formula.
5. **LRA:**
   - Relative gate −20 LU (not −10 LU).
   - Sort surviving block loudness values.
   - LRA = P95 − P10 of that distribution (in LU).
   - Return 0.f if fewer than 2 blocks survive gating.

The existing `measure()` method is unchanged — the post-render target-level path in
`pipeline.cpp` keeps calling it.

---

## 2. Analysis snapshot

```cpp
// analysis.hpp — AnalysisSnapshot
float lraLu = 0.f;  // EBU R128 Loudness Range (LU); 0 = unknown/silence
```

`analyseFile()` in `analysis.cpp` constructs a `LufsAnalyser`, calls
`measureWithLra()` on the full audio buffer, and stores `metrics.lra` in
`snap.lraLu`. The integrated LUFS from `metrics.integratedLufs` is not stored in
`AnalysisSnapshot` (not needed downstream — `deriveAdvice()` works from RMS).

---

## 3. Advice wiring

In `advice.cpp`, `deriveAdvice()`, after the existing limiter target calculation:

```cpp
// LRA-based limiter adjustment: neutral at 12 LU, ±3 LU clamp.
// Low LRA (hyperlimited) → push target down (tighter); high LRA (dynamic) → pull up.
const float lraOffset = std::clamp((snap.lraLu - 12.f) * 0.30f, -3.f, 3.f);
out.limiter.targetLufsApprox += lraOffset;
```

**Curve reference:**

| LRA (LU) | Offset | Effect |
|----------|--------|--------|
| ≤ 2      | −3 LU  | Hyper-limited stream rip — push hard |
| 12       |  0 LU  | Neutral (typical pop/rock) |
| ≥ 22     | +3 LU  | Orchestral/film — transparent limiting |

No new fields on `LimiterParams`. The offset is baked into `targetLufsApprox` before
it leaves `deriveAdvice()`, so GUI overrides and CLI flags still work naturally.

---

## 4. Preset builder data layer

### `TrackAnalysis` (domain model)

```cpp
// preset_builder/include/preset_builder/domain/track.hpp
struct TrackAnalysis {
    // ... existing fields ...
    float lra = 0.f;  // LU
};
```

### SQLite schema migration

In `database.cpp`, `createSchema()`, after the existing `file_size` migration:

```cpp
sqlite3_exec(db_,
    "ALTER TABLE track_analysis ADD COLUMN lra REAL DEFAULT 0;",
    nullptr, nullptr, nullptr);
```

Uses the same ignore-on-duplicate-column pattern as the `file_size` migration.

### Repository

- `kSelectJoin` — add `a.lra` to the SELECT column list.
- `stmtToTrack()` — read column by index into `t.analysis.lra`.
- `save()` INSERT OR REPLACE — include `lra` column and bind.

### Ingest

`toTrackAnalysis()` in `ingest_service.cpp`:
```cpp
a.lra = snap.lraLu;
```

---

## 5. GUI — ChainPanel

`ChainPanel.h` declares a second read-only label in the Limiter section:
```cpp
QLabel* limLraLbl_ = nullptr;
```

During construction (in the Limiter `QVBoxLayout`), add `limLraLbl_` after
`limPeakLbl_`, same `color: #555` style.

In `setAdvice()`:
```cpp
limPeakLbl_->setText(
    QString("peak: %1 dBFS").arg(static_cast<double>(snap.overallPeakDb), 0, 'f', 1));
limLraLbl_->setText(
    QString("LRA: %1 LU").arg(static_cast<double>(snap.lraLu), 0, 'f', 1));
```

---

## 6. Markdown report

In `report.cpp`, `formatAdviceMarkdown()`, in the Analysis section after the overall
RMS row:

```
**LRA:** 12.4 LU
```

The Limiter parameters section already shows the derived `targetLufsApprox` (which
now includes the LRA offset), so no extra annotation is needed there.

---

## 7. Tests (`tests/test_lra.cpp`)

| Test | Signal | Expected |
|------|--------|----------|
| Silence | All zeros | `lra == 0.f` (below gate) |
| Single level | 10 s sine, constant amplitude | `lra < 1.f` (P95−P10 ≈ 0) |
| Two segments | 10 s quiet (−30 dBFS) + 10 s loud (−10 dBFS) | `lra` in [10, 18] LU |
| Advice low LRA | `snap.lraLu = 2.f` | `targetLufsApprox == baseline − 3.f` |
| Advice high LRA | `snap.lraLu = 22.f` | `targetLufsApprox == baseline + 3.f` |
| Advice neutral | `snap.lraLu = 12.f` | `targetLufsApprox == baseline` |

---

## 8. Files touched

| File | Change |
|------|--------|
| `core/include/mastertweak/dsp/lufs_analyser.hpp` | Add `LoudnessMetrics`, `measureWithLra()` |
| `core/src/dsp/lufs_analyser.cpp` | Implement `measureWithLra()` |
| `core/include/mastertweak/analysis.hpp` | Add `lraLu` to `AnalysisSnapshot` |
| `core/src/analysis.cpp` | Call `measureWithLra()` in `analyseFile()` |
| `core/include/mastertweak/advice.hpp` | No change |
| `core/src/advice.cpp` | Add LRA offset to `targetLufsApprox` |
| `core/src/report.cpp` | Add LRA line to markdown export |
| `preset_builder/include/preset_builder/domain/track.hpp` | Add `lra` to `TrackAnalysis` |
| `preset_builder/src/adapters/database.cpp` | `ALTER TABLE` migration |
| `preset_builder/src/adapters/sqlite_track_repository.cpp` | SELECT / read / write `lra` |
| `preset_builder/src/services/ingest_service.cpp` | Copy `snap.lraLu → a.lra` |
| `gui/ChainPanel.h` | Declare `limLraLbl_` |
| `gui/ChainPanel.cpp` | Construct + update `limLraLbl_` |
| `tests/test_lra.cpp` | New test file (6 cases) |
