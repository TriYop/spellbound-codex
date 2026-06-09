# BandStats Percentile Descriptors — Design Spec

**Date:** 2026-06-09  
**Status:** Approved

## Goal

Add P10 / P50 / P95 of per-block RMS to `BandStats` so that `deriveAdvice()` can use a distribution-aware characteristic level instead of the current mean/peak-hold blend. This makes the EQ and multiband compression advice more robust on highly dynamic tracks (e.g. alternating quiet verses and dense choruses), where the long-term mean is pulled down by quiet sections and the peak-hold is dominated by the loudest moment.

## Scope

- `core/include/mastertweak/analysis.hpp` — add three fields to `BandStats`
- `core/src/analysis.cpp` — collect per-block RMS samples, sort, compute percentiles
- `core/src/advice.cpp` — replace characteristic-level formula with percentile blend
- `core/src/report.cpp` — extend analysis table in markdown export
- `tests/test_analysis.cpp` — add dynamic-signal percentile test

Out of scope: `AnalysisSnapshot` overall-level percentiles, preset-builder `TrackAnalysis`, GUI display of percentiles.

## Data Model

`BandStats` gains three fields (dBFS, same floor as existing fields):

```cpp
struct BandStats {
    float avgRmsDb    = -100.f;  // existing — long-term RMS mean
    float peakRmsDb   = -100.f;  // existing — 100ms-smoothed peak-hold
    float p10RmsDb    = -100.f;  // NEW — 10th-percentile block RMS
    float p50RmsDb    = -100.f;  // NEW — median block RMS
    float p95RmsDb    = -100.f;  // NEW — 95th-percentile block RMS
    float correlation =    1.f;  // existing
    float crestDb     =    0.f;  // existing
};
```

`avgRmsDb` and `peakRmsDb` are kept — they remain useful for display and future use.

## Analysis Implementation

**Collection:** Inside `analyseFile()`, add one `std::vector<float>` per band:

```cpp
std::array<std::vector<float>, kNumBands> bandRmsSamples;
```

In `storeBand`, after computing `rmsL` and `rmsR`, push the L+R-averaged dBFS value:

```cpp
const float blockDb = (toDb(rmsL) + toDb(rmsR)) * 0.5f;
bandRmsSamples[bandIdx].push_back(blockDb);
```

**Percentile extraction:** After the main loop, for each band: sort the vector and read indices at 10 %, 50 %, 95 %:

```cpp
std::sort(v.begin(), v.end());
auto pct = [&](float p) -> float {
    if (v.empty()) return -100.f;
    return v[static_cast<size_t>(std::clamp(
        static_cast<int>(std::floor(p * static_cast<float>(v.size()))),
        0, static_cast<int>(v.size()) - 1))];
};
snap.bands[i].p10RmsDb = pct(0.10f);
snap.bands[i].p50RmsDb = pct(0.50f);
snap.bands[i].p95RmsDb = pct(0.95f);
```

**Memory:** ~290 KB for a 4-min / 44.1 kHz track, ~2 MB for 30 min. Acceptable for offline processing.

**Note:** `toDb` is currently defined after the main loop in `analyseFile()`. Since `storeBand` uses it via `[&]` capture, `toDb` must be moved to before the `storeBand` lambda definition. No other structural change needed.

## Advice Algorithm

Single-line change in `deriveAdvice()`:

```cpp
// Before
const float refDb = (snap.bands[bi].avgRmsDb + snap.bands[bi].peakRmsDb) * 0.5f;

// After
const float refDb = (snap.bands[bi].p50RmsDb + snap.bands[bi].p95RmsDb) * 0.5f;
```

Rationale: P50 gives the "typical loud section" level (not pulled down by quiet passages), while P95 gives a near-peak reference that is more stable than a single peak-hold. Together they represent the same avg/peak intent as before, but distribution-aware.

All downstream consumers of `refDb` (EQ gain clamp ±12 dB, Q selection, multiband comp threshold, ratio) are unchanged.

## Report

Extend the Analysis table in `formatAdviceMarkdown()`:

```
| Band    | Avg (dBFS) | P10 (dBFS) | P50 (dBFS) | P95 (dBFS) | Crest (dB) | L/R Corr |
```

## Tests

Add one test case to `test_analysis.cpp`: a signal that alternates between a loud sine burst and silence (e.g. 50 % duty cycle). Verify:

- `p10RmsDb < p50RmsDb < p95RmsDb` (ordering invariant)
- `p95RmsDb` is within ±3 dB of the sine RMS (loud sections dominate)
- `p10RmsDb` is at or near the silence floor (−100 dBFS or below −50 dBFS)
- `p50RmsDb` is between the floor and the sine RMS
