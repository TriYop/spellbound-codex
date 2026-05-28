# Gain Staging Between DSP Stages

**Date:** 2026-05-28  
**Status:** Approved

## Problem

The mastering chain has no inter-stage headroom management. Two concrete failure modes:

1. **Post-multiband comp level drop** — The multiband compressor reduces level with no makeup gain. The saturator and mixbus compressor downstream receive a signal that may be 3–8 dB lower than they were calibrated for (advice thresholds and drive values are derived from the pre-chain reference level). This degrades saturation character and mixbus compression behaviour.

2. **Pre-limiter overshoot** — The mixbus compressor applies advice-driven makeup gain unconditionally. If the input to the mixbus comp is already hot, the makeup gain can push signal to +3 dBFS or above, forcing the limiter into heavy gain reduction that audibly pumps.

The pre-analysis gain staging (implemented previously) sets a reference level before analysis, but nothing preserves that calibration through the chain.

## Solution

Add two gain staging points to `renderFile()`:

| Position | Operation | Purpose |
|---|---|---|
| After multiband comp | RMS restore | Bring broadband RMS back to pre-chain reference so saturator/mixbus see calibrated level |
| After mixbus comp | Peak trim (−3 dBFS ceiling) | Prevent makeup gain overshoot from forcing heavy limiting |

**Post-EQ staging is intentionally omitted.** EQ feeding a compressor is standard mastering practice — the compressor should respond to any EQ boosts. The pre-analysis normalization ensures EQ corrections are modest.

## New Component: `dsp::GainStager`

### Header: `core/include/mastertweak/dsp/gain_stager.hpp`

```cpp
namespace mt::dsp {

struct StageLevelReport {
    float inputPeakDb  = -100.f;
    float inputRmsDb   = -100.f;
    float trimDb       = 0.f;        // gain applied in dB (negative = trim, positive = restore)
    float outputPeakDb = -100.f;
    float outputRmsDb  = -100.f;
};

class GainStager {
public:
    // Apply gain so broadband RMS = targetRmsDb. Clamped to ±12 dB.
    StageLevelReport restoreRms(std::vector<std::vector<float>>& buf,
                                int numFrames, float targetRmsDb);

    // Trim only if peak > targetPeakDb. Never boosts.
    StageLevelReport trimPeak(std::vector<std::vector<float>>& buf,
                              int numFrames, float targetPeakDb = -3.f);

private:
    static constexpr float kMaxRestoreDb = 12.f;
    static float measurePeakDb(const std::vector<std::vector<float>>&, int numFrames);
    static float measureRmsDb(const std::vector<std::vector<float>>&, int numFrames);
    static void  applyGain(std::vector<std::vector<float>>&, int numFrames, float gainLin);
};

} // namespace mt::dsp
```

### Source: `core/src/dsp/gain_stager.cpp`

Implementations are straightforward:
- `measurePeakDb`: iterate all channels/frames, track `std::abs` max, convert to dB. Return −100 dBFS floor for silence.
- `measureRmsDb`: iterate all channels/frames, compute mean-square, sqrt, convert to dB. Return −100 dBFS for silence.
- `restoreRms`: measure input RMS, compute `trimDb = targetRmsDb − measuredRmsDb`, clamp to ±`kMaxRestoreDb`, apply, measure output. Return `StageLevelReport`.
- `trimPeak`: measure input peak. If `peakDb ≤ targetPeakDb` return zero-trim report. Otherwise compute `trimDb = targetPeakDb − peakDb` (always negative), apply, measure output. Return `StageLevelReport`.
- `applyGain`: multiply every sample by `gainLin`.

Silence floor guard: if `rmsLin < 1e-7f`, return report with `trimDb = 0` (no operation).

## Pipeline Integration

### `core/include/mastertweak/pipeline.hpp`

Add to `MasterResult`:

```cpp
struct GainStageReport {
    dsp::StageLevelReport postMbComp;   // RMS restore after multiband compressor
    dsp::StageLevelReport postMixbus;   // peak trim after mixbus compressor
};

struct MasterResult {
    AnalysisSnapshot  analysis;
    AdviceSet         advice;
    float             preGainDb  = 0.f;
    GainStageReport   gainStages;
    bool              ok         = false;
    std::string       errMsg;
};
```

### `core/src/pipeline.cpp`

Two new call sites in `renderFile()`:

```
// After MultibandComp block:
result.gainStages.postMbComp =
    gs.restoreRms(buf, nf, /*targetRmsDb=*/ rmsDbBeforeChain);

// After MixbusComp block:
result.gainStages.postMixbus =
    gs.trimPeak(buf, nf, /*targetPeakDb=*/ -3.f);
```

`rmsDbBeforeChain` is measured by calling `gs.restoreRms` with a dummy self-measurement — no, more directly: instantiate `GainStager gs` at the top of `renderFile()`, then after `applyPreGain()` call `gs.trimPeak(buf, nf, 0.f)` just to capture the level — no, simplest: add a `measureRmsDb()` public static method to `GainStager` so pipeline.cpp can call it directly after `applyPreGain()`.

Concretely:

```cpp
GainStager gs;

// After applyPreGain() and before analyseFile():
const float rmsDbBeforeChain = GainStager::measureRmsDb(buf, nf);

// ... EQ, multiband comp ...

// After MultibandComp:
result.gainStages.postMbComp = gs.restoreRms(buf, nf, rmsDbBeforeChain);

// ... saturator, stereo width, mixbus comp ...

// After MixbusComp:
result.gainStages.postMixbus = gs.trimPeak(buf, nf, -3.f);
```

`measureRmsDb()` is promoted to `public static` in the class interface (no state needed).

`GainStager gs;` is instantiated once at the top of `renderFile()`.

## Build

Add `src/dsp/gain_stager.cpp` to the source list in `core/CMakeLists.txt`.

## Testing

Existing tests must continue to pass. No new unit tests are strictly required (the helpers are trivial math), but a test with a known synthetic buffer would be a good addition to `tests/test_pipeline.cpp`:

- Feed a −30 dBFS white-noise buffer through `restoreRms(-18)` → verify output RMS ≈ −18 dBFS ± 0.5 dB.
- Feed a +3 dBFS sine through `trimPeak(-3)` → verify output peak ≈ −3 dBFS ± 0.1 dB.
- Feed a −6 dBFS sine through `trimPeak(-3)` → verify `trimDb == 0` (no change).

### End-to-end verification

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/cli/mastertweak --analyze-only <quiet_track.wav> --preset <preset.xml>
./build/cli/mastertweak <input.wav> <output.wav> --preset <preset.xml>
```
