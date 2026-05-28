# Target Level Presets — Design Spec

**Date:** 2026-05-28
**Feature:** Built-in platform loudness targets (LUFS + true-peak ceiling) selectable at render time.

---

## Overview

Add a `TargetLevelProfile` concept to MasterTweak: a named pair of `(targetLufs, peakCeiling)` drawn from a built-in table of industry standards. When a target is selected, the pipeline measures integrated LUFS (EBU R128) after the mixbus compressor, applies a gain trim to land at the target loudness, then runs the limiter with the target's true-peak ceiling. If no target is selected (the default), behaviour is identical to today.

---

## Data Model

**New file:** `core/include/mastertweak/target_level.hpp`

```cpp
struct TargetLevelProfile {
    std::string name;
    float lufs;         // integrated loudness target (LUFS)
    float peakCeiling;  // true-peak ceiling (dBTP)
};

extern const std::array<TargetLevelProfile, 8> kTargetLevelProfiles;

// Case-insensitive lookup by name. Returns nullptr if not found.
const TargetLevelProfile* findTargetLevel(std::string_view name);
```

**New file:** `core/src/target_level.cpp` — defines the table and `findTargetLevel`.

**Built-in table:**

| Name                 | LUFS  | True Peak |
|----------------------|-------|-----------|
| Spotify              | −14.0 | −1.0 dBTP |
| YouTube              | −14.0 | −1.0 dBTP |
| Amazon Music         | −14.0 | −1.0 dBTP |
| Tidal                | −14.0 | −1.0 dBTP |
| Apple Music          | −16.0 | −1.0 dBTP |
| CD / Download        |  −9.0 | −0.1 dBTP |
| Broadcast EBU R128   | −23.0 | −1.0 dBTP |
| Broadcast ATSC A/85  | −24.0 | −2.0 dBTP |

**`RenderOptions`** gains one new field:

```cpp
std::optional<TargetLevelProfile> targetLevel;  // nullopt = current behaviour
```

`nullopt` is the default, so all existing callers are unaffected.

---

## DSP: `LufsAnalyser` (EBU R128)

**New files:**
- `core/include/mastertweak/dsp/lufs_analyser.hpp`
- `core/src/dsp/lufs_analyser.cpp`

```cpp
class LufsAnalyser {
public:
    void prepare(float sampleRate, int numChannels);
    // Measures integrated loudness of the full buffer. Returns LUFS.
    float measure(const std::vector<std::vector<float>>& samples, int numFrames);
};
```

### Algorithm (ITU-R BS.1770 / EBU R128)

1. **K-weighting:** apply two fixed-coefficient biquad stages per channel (pre-filter + RLB weighting). Coefficients are ITU-R BS.1770 constants, computed once at `prepare()` time from sample rate using existing `biquad.hpp`.
2. **Mean-square integration:** compute mean square over 400 ms blocks with 75% overlap.
3. **Absolute gate:** discard blocks whose level is below −70 LUFS.
4. **Relative gate:** compute ungated mean power; discard blocks more than 10 LU below that mean.
5. **Return** gated mean power converted to LUFS.
6. **Edge cases:** silence or buffers shorter than one 400 ms block return −70.0f (the absolute gate floor).

The analyser is a pure audio utility — no Qt, no CLI11 dependency.

---

## Pipeline Integration

**Modified file:** `core/src/pipeline.cpp`

In `renderFile()`, insert between the mixbus compressor step and the limiter step:

```
// ~70%  mbusComp.process(buf, nf)     [unchanged]

// ~75%  [NEW — only when opts.targetLevel is set]
//       LufsAnalyser::measure(buf, nf)
//       gain_trim_db = targetLevel.lufs − measured_lufs
//       apply linear gain trim to all samples in buf
//       adv.limiter.ceilingDb = targetLevel.peakCeiling

// ~80%  lim.setAdvice(adv.limiter)    [uses potentially overridden ceiling]
//       lim.process(buf, nf)
```

Progress reports a `"Normalizing to target"` stage at 0.75.

`result.advice.limiter.ceilingDb` reflects the actual ceiling used (the target's value when a target is active), so the GUI parameter panel shows the correct value after render.

The gain trim is applied in-place to the in-memory float buffer — no second file write, no re-running of earlier DSP stages. Dither runs once after the limiter as today.

---

## GUI

**New files:** `gui/TargetLevelCombo.h`, `gui/TargetLevelCombo.cpp`

A thin `QComboBox` wrapper:
- First entry: `"Auto (from preset)"` → `std::nullopt`
- Remaining entries populated from `kTargetLevelProfiles`, formatted as `"Spotify — −14 LUFS / −1 dBTP"`
- Signal: `void targetChanged(std::optional<mt::TargetLevelProfile>)`
- Public accessor: `std::optional<mt::TargetLevelProfile> currentTarget() const`

**`MainWindow` render row** (bottom of window):

```
[Render]  [Save As…]  Bit depth: [24-bit ▾]  [FLAC]  Target: [Auto (from preset) ▾]  Ready
```

`onRenderClicked()` reads `targetCombo_->currentTarget()` and sets `opts.targetLevel` before handing off to `RenderWorker`.

No other panels change. `ParameterPanel` is unaffected.

---

## CLI

**Modified file:** `cli/main.cpp`

Two new flags:

```
--target-level <name>   Set loudness target by platform name (case-insensitive).
                        Accepted values: "spotify", "youtube", "amazon music",
                        "tidal", "apple music", "cd / download",
                        "broadcast ebu r128", "broadcast atsc a/85"

--list-targets          Print built-in target level table and exit (exit code 0).
```

`--list-targets` output:
```
Built-in target levels:
  Spotify              -14.0 LUFS / -1.0 dBTP
  YouTube              -14.0 LUFS / -1.0 dBTP
  Amazon Music         -14.0 LUFS / -1.0 dBTP
  Tidal                -14.0 LUFS / -1.0 dBTP
  Apple Music          -16.0 LUFS / -1.0 dBTP
  CD / Download         -9.0 LUFS / -0.1 dBTP
  Broadcast EBU R128   -23.0 LUFS / -1.0 dBTP
  Broadcast ATSC A/85  -24.0 LUFS / -2.0 dBTP
```

If `--target-level` receives an unknown name: print the table to stderr and exit with code 1. Works in both single-file and `--batch` modes.

---

## Tests

**`LufsAnalyser` unit tests:**
- Silence buffer → returns −70.0f (no crash, no NaN)
- 1 kHz sine at −23 dBFS stereo → integrated LUFS within ±0.5 LU of −23
- Mono (1 channel) input → correct result, no crash
- Buffer shorter than one 400 ms block → returns −70.0f

**`TargetLevelProfile` table tests:**
- `findTargetLevel("spotify")` → entry with `lufs == −14.0f`
- `findTargetLevel("APPLE MUSIC")` → case-insensitive match succeeds
- `findTargetLevel("unknown")` → `nullptr`
- All 8 entries: `lufs < 0`, `peakCeiling ≤ 0`

**Pipeline integration test:**
- Render a short sine WAV with `opts.targetLevel = *findTargetLevel("spotify")`
- Re-measure rendered output with `LufsAnalyser` → result within ±1.5 LU of −14

---

## Files Changed

| Action | Path |
|--------|------|
| Create | `core/include/mastertweak/target_level.hpp` |
| Create | `core/src/target_level.cpp` |
| Create | `core/include/mastertweak/dsp/lufs_analyser.hpp` |
| Create | `core/src/dsp/lufs_analyser.cpp` |
| Create | `gui/TargetLevelCombo.h` |
| Create | `gui/TargetLevelCombo.cpp` |
| Modify | `core/include/mastertweak/pipeline.hpp` — add `targetLevel` to `RenderOptions` |
| Modify | `core/src/pipeline.cpp` — insert LUFS measurement + gain trim step |
| Modify | `gui/MainWindow.h` — add `targetCombo_` member |
| Modify | `gui/MainWindow.cpp` — wire combo into render row and `onRenderClicked` |
| Modify | `cli/main.cpp` — add `--target-level` and `--list-targets` |
| Modify | `core/CMakeLists.txt` — add new source files |
| Modify | `gui/CMakeLists.txt` — add new source files |
