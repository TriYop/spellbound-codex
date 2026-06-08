# Gain Staging Between DSP Stages — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Insert two inter-stage gain-management points in the mastering pipeline — an RMS restore after the multiband compressor and a peak trim after the mixbus compressor — so each downstream processor sees a calibrated signal level.

**Architecture:** New `dsp::GainStager` class (header + source) provides `restoreRms()` and `trimPeak()`, both returning a `StageLevelReport` with pre/post levels and the gain applied. `pipeline.cpp` calls these at two points in `renderFile()`, storing results in a new `GainStageReport` field on `MasterResult`. `measureRmsDb()` is public static so `pipeline.cpp` can snapshot the reference level after pre-gain staging.

**Tech Stack:** C++20, doctest (tests), CMake/Ninja (build). No new dependencies.

---

## File Map

| Action | Path | Purpose |
|--------|------|---------|
| Create | `core/include/mastertweak/dsp/gain_stager.hpp` | `StageLevelReport` struct + `GainStager` class declaration |
| Create | `core/src/dsp/gain_stager.cpp` | `GainStager` implementation |
| Create | `tests/test_gain_stager.cpp` | Unit tests for `GainStager` |
| Modify | `core/CMakeLists.txt` | Add `gain_stager.cpp` to `mastertweak_core` sources |
| Modify | `tests/CMakeLists.txt` | Add `test_gain_stager.cpp` to test executable |
| Modify | `core/include/mastertweak/pipeline.hpp` | Add `GainStageReport` + `gainStages` field to `MasterResult` |
| Modify | `core/src/pipeline.cpp` | Include header, snapshot reference RMS, call both staging points |

---

### Task 1: Header — `StageLevelReport` + `GainStager` declaration

**Files:**
- Create: `core/include/mastertweak/dsp/gain_stager.hpp`

- [ ] **Step 1: Write the header**

```cpp
// core/include/mastertweak/dsp/gain_stager.hpp
#pragma once

#include <vector>

namespace mt::dsp {

struct StageLevelReport {
    float inputPeakDb  = -100.f;
    float inputRmsDb   = -100.f;
    float trimDb       = 0.f;        // gain applied (negative = trim, positive = restore)
    float outputPeakDb = -100.f;
    float outputRmsDb  = -100.f;
};

class GainStager {
public:
    // Bring broadband RMS to targetRmsDb. Clamped to ±kMaxRestoreDb.
    StageLevelReport restoreRms(std::vector<std::vector<float>>& buf,
                                int numFrames, float targetRmsDb);

    // Trim only if peak > targetPeakDb. Never boosts.
    StageLevelReport trimPeak(std::vector<std::vector<float>>& buf,
                              int numFrames, float targetPeakDb = -3.f);

    // Public so pipeline.cpp can snapshot the reference RMS without an instance.
    static float measureRmsDb(const std::vector<std::vector<float>>& buf, int numFrames);
    static float measurePeakDb(const std::vector<std::vector<float>>& buf, int numFrames);

private:
    static constexpr float kMaxRestoreDb = 12.f;
    static void applyGain(std::vector<std::vector<float>>& buf, int numFrames, float gainLin);
};

} // namespace mt::dsp
```

- [ ] **Step 2: Create a stub `gain_stager.cpp` so it compiles**

```cpp
// core/src/dsp/gain_stager.cpp
#include "mastertweak/dsp/gain_stager.hpp"

#include <algorithm>
#include <cmath>

namespace mt::dsp {

float GainStager::measurePeakDb(const std::vector<std::vector<float>>& buf, int numFrames) {
    float peak = 0.f;
    for (const auto& ch : buf)
        for (int f = 0; f < numFrames; ++f)
            peak = std::max(peak, std::abs(ch[static_cast<size_t>(f)]));
    return peak > 1e-7f ? 20.f * std::log10(peak) : -100.f;
}

float GainStager::measureRmsDb(const std::vector<std::vector<float>>& buf, int numFrames) {
    double sumSq = 0.0;
    long   count = 0;
    for (const auto& ch : buf)
        for (int f = 0; f < numFrames; ++f) {
            const double s = ch[static_cast<size_t>(f)];
            sumSq += s * s;
            ++count;
        }
    if (count == 0) return -100.f;
    const float rmsLin = static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
    return rmsLin > 1e-7f ? 20.f * std::log10(rmsLin) : -100.f;
}

void GainStager::applyGain(std::vector<std::vector<float>>& buf, int numFrames, float gainLin) {
    for (auto& ch : buf)
        for (int f = 0; f < numFrames; ++f)
            ch[static_cast<size_t>(f)] *= gainLin;
}

StageLevelReport GainStager::restoreRms(std::vector<std::vector<float>>& buf,
                                        int numFrames, float targetRmsDb) {
    StageLevelReport r;
    r.inputPeakDb = measurePeakDb(buf, numFrames);
    r.inputRmsDb  = measureRmsDb(buf, numFrames);

    if (r.inputRmsDb <= -99.f) return r;  // silence guard

    r.trimDb = std::clamp(targetRmsDb - r.inputRmsDb, -kMaxRestoreDb, kMaxRestoreDb);
    applyGain(buf, numFrames, std::pow(10.f, r.trimDb / 20.f));

    r.outputPeakDb = measurePeakDb(buf, numFrames);
    r.outputRmsDb  = measureRmsDb(buf, numFrames);
    return r;
}

StageLevelReport GainStager::trimPeak(std::vector<std::vector<float>>& buf,
                                      int numFrames, float targetPeakDb) {
    StageLevelReport r;
    r.inputPeakDb = measurePeakDb(buf, numFrames);
    r.inputRmsDb  = measureRmsDb(buf, numFrames);

    if (r.inputPeakDb <= targetPeakDb) {
        // No trim needed — copy input values to output
        r.outputPeakDb = r.inputPeakDb;
        r.outputRmsDb  = r.inputRmsDb;
        return r;  // trimDb stays 0
    }

    r.trimDb = targetPeakDb - r.inputPeakDb;  // always negative
    applyGain(buf, numFrames, std::pow(10.f, r.trimDb / 20.f));

    r.outputPeakDb = measurePeakDb(buf, numFrames);
    r.outputRmsDb  = measureRmsDb(buf, numFrames);
    return r;
}

} // namespace mt::dsp
```

- [ ] **Step 3: Register in `core/CMakeLists.txt`**

Add `src/dsp/gain_stager.cpp` after `src/dsp/lufs_analyser.cpp`:

```cmake
add_library(mastertweak_core STATIC
    src/version.cpp
    src/io.cpp
    src/analysis.cpp
    src/preset.cpp
    src/advice.cpp
    src/pipeline.cpp
    src/target_level.cpp
    src/dsp/linkwitz_riley.cpp
    src/dsp/parametric_eq.cpp
    src/dsp/compressor.cpp
    src/dsp/multiband_comp.cpp
    src/dsp/mixbus_comp.cpp
    src/dsp/saturator.cpp
    src/dsp/stereo_width.cpp
    src/dsp/limiter.cpp
    src/dsp/dither.cpp
    src/dsp/lufs_analyser.cpp
    src/dsp/gain_stager.cpp
)
```

- [ ] **Step 4: Verify it compiles (no tests yet)**

```bash
cmake --build build --parallel 2>&1
```

Expected: build succeeds, zero warnings.

- [ ] **Step 5: Commit**

```bash
git add core/include/mastertweak/dsp/gain_stager.hpp \
        core/src/dsp/gain_stager.cpp \
        core/CMakeLists.txt
git commit -m "feat: add GainStager DSP class (restoreRms, trimPeak)"
```

---

### Task 2: Tests for `GainStager`

**Files:**
- Create: `tests/test_gain_stager.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// tests/test_gain_stager.cpp
#include "mastertweak/dsp/gain_stager.hpp"

#include <doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

using mt::dsp::GainStager;
using mt::dsp::StageLevelReport;

static constexpr int kSr = 44100;
static constexpr int kFrames = kSr;  // 1 second

// Build a mono-in-stereo sine buffer at a given peak amplitude.
static std::vector<std::vector<float>> makeSine(float amplitude) {
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(kFrames)));
    for (int f = 0; f < kFrames; ++f) {
        const float s = amplitude * std::sin(
            2.f * std::numbers::pi_v<float> * 1000.f * static_cast<float>(f) / kSr);
        buf[0][static_cast<size_t>(f)] = s;
        buf[1][static_cast<size_t>(f)] = s;
    }
    return buf;
}

// ── measureRmsDb ─────────────────────────────────────────────────────────────

TEST_CASE("GainStager::measureRmsDb returns correct value for known sine") {
    // Sine RMS = amplitude / sqrt(2)
    const float amp = std::pow(10.f, -18.f / 20.f);
    auto buf = makeSine(amp);
    const float rms = GainStager::measureRmsDb(buf, kFrames);
    // Expected: -18 dBFS - 3.01 dB = -21.01 dBFS (sine RMS is 3 dB below peak)
    const float expected = 20.f * std::log10(amp / std::sqrt(2.f));
    CHECK(rms == doctest::Approx(expected).epsilon(0.5f));
}

TEST_CASE("GainStager::measureRmsDb returns -100 for silence") {
    auto buf = makeSine(0.f);
    CHECK(GainStager::measureRmsDb(buf, kFrames) <= -99.f);
}

// ── measurePeakDb ─────────────────────────────────────────────────────────────

TEST_CASE("GainStager::measurePeakDb returns correct value for known sine") {
    const float amp = std::pow(10.f, -6.f / 20.f);  // -6 dBFS peak
    auto buf = makeSine(amp);
    const float peak = GainStager::measurePeakDb(buf, kFrames);
    CHECK(peak == doctest::Approx(-6.f).epsilon(0.1f));
}

// ── restoreRms ───────────────────────────────────────────────────────────────

TEST_CASE("GainStager::restoreRms brings RMS to target") {
    // Sine at -30 dBFS peak → RMS ≈ -33 dBFS. Target: -18 dBFS RMS.
    const float amp = std::pow(10.f, -30.f / 20.f);
    auto buf = makeSine(amp);

    GainStager gs;
    const float targetRms = -18.f;
    auto report = gs.restoreRms(buf, kFrames, targetRms);

    CHECK(report.outputRmsDb == doctest::Approx(targetRms).epsilon(0.5f));
    CHECK(report.trimDb > 0.f);  // was quiet → boosted
    CHECK(report.inputRmsDb < report.outputRmsDb);
}

TEST_CASE("GainStager::restoreRms clamps trim to +12 dB max") {
    // Sine at -50 dBFS → needs +29 dB to reach -21 dBFS RMS; clamped to +12.
    const float amp = std::pow(10.f, -50.f / 20.f);
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.restoreRms(buf, kFrames, -18.f);

    CHECK(report.trimDb == doctest::Approx(12.f).epsilon(0.01f));
}

TEST_CASE("GainStager::restoreRms clamps trim to -12 dB min") {
    // Sine at +0 dBFS peak → RMS ≈ -3 dBFS. Target: -18 dBFS → needs -15 dB; clamped to -12.
    const float amp = 1.f;
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.restoreRms(buf, kFrames, -18.f);

    CHECK(report.trimDb == doctest::Approx(-12.f).epsilon(0.01f));
}

TEST_CASE("GainStager::restoreRms does nothing on silence") {
    auto buf = makeSine(0.f);
    GainStager gs;
    auto report = gs.restoreRms(buf, kFrames, -18.f);
    CHECK(report.trimDb == 0.f);
}

// ── trimPeak ─────────────────────────────────────────────────────────────────

TEST_CASE("GainStager::trimPeak reduces signal exceeding threshold") {
    // Sine at +3 dBFS peak (above 0 dBFS). Target: -3 dBFS.
    const float amp = std::pow(10.f, 3.f / 20.f);
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.trimPeak(buf, kFrames, -3.f);

    CHECK(report.outputPeakDb == doctest::Approx(-3.f).epsilon(0.1f));
    CHECK(report.trimDb < 0.f);  // trimmed down
}

TEST_CASE("GainStager::trimPeak does not boost signal below threshold") {
    // Sine at -6 dBFS peak. Target: -3 dBFS. Should be unchanged.
    const float amp = std::pow(10.f, -6.f / 20.f);
    auto buf = makeSine(amp);
    const auto bufCopy = buf;

    GainStager gs;
    auto report = gs.trimPeak(buf, kFrames, -3.f);

    CHECK(report.trimDb == 0.f);
    CHECK(report.outputPeakDb == doctest::Approx(report.inputPeakDb).epsilon(0.01f));
    // Buffer must be unmodified
    for (int ch = 0; ch < 2; ++ch)
        for (int f = 0; f < kFrames; ++f)
            CHECK(buf[static_cast<size_t>(ch)][static_cast<size_t>(f)]
                  == bufCopy[static_cast<size_t>(ch)][static_cast<size_t>(f)]);
}

TEST_CASE("GainStager::trimPeak report fields are populated correctly") {
    const float amp = std::pow(10.f, 3.f / 20.f);  // +3 dBFS peak
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.trimPeak(buf, kFrames, -3.f);

    // All five report fields must be non-default
    CHECK(report.inputPeakDb  > -99.f);
    CHECK(report.inputRmsDb   > -99.f);
    CHECK(report.outputPeakDb > -99.f);
    CHECK(report.outputRmsDb  > -99.f);
    CHECK(report.trimDb != 0.f);
}
```

- [ ] **Step 2: Register in `tests/CMakeLists.txt`**

Add `test_gain_stager.cpp` to the executable source list:

```cmake
add_executable(mastertweak_tests
    test_main.cpp
    test_sanity.cpp
    test_io.cpp
    test_analysis.cpp
    test_preset.cpp
    test_advice.cpp
    test_dsp.cpp
    test_pipeline.cpp
    test_target_level.cpp
    test_gain_stager.cpp
)
```

- [ ] **Step 3: Run tests — expect them to PASS** (implementation is already in the stub)

```bash
cmake --build build --parallel 2>&1 && ctest --test-dir build --output-on-failure 2>&1
```

Expected: all tests pass, including the 9 new `GainStager` tests.

- [ ] **Step 4: Commit**

```bash
git add tests/test_gain_stager.cpp tests/CMakeLists.txt
git commit -m "test: add GainStager unit tests"
```

---

### Task 3: Pipeline integration — `MasterResult` and `renderFile()`

**Files:**
- Modify: `core/include/mastertweak/pipeline.hpp`
- Modify: `core/src/pipeline.cpp`

- [ ] **Step 1: Update `pipeline.hpp`**

Add `#include "mastertweak/dsp/gain_stager.hpp"` and the new struct. The full updated `pipeline.hpp`:

```cpp
#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/dsp/gain_stager.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/preset.hpp"
#include "mastertweak/target_level.hpp"

#include <functional>
#include <optional>
#include <string>

namespace mt {

struct RenderOptions {
    int  outputBitDepth   = 24;
    bool outputFlac       = false;
    bool bypassEq         = false;
    bool bypassMbComp     = false;
    bool bypassSaturator  = false;
    bool bypassWidth      = false;
    bool bypassMixbusComp = false;
    bool bypassLimiter    = false;
    bool bypassDither     = false;
    std::optional<TargetLevelProfile> targetLevel;
};

struct GainStageReport {
    dsp::StageLevelReport postMbComp;   // RMS restore after multiband compressor
    dsp::StageLevelReport postMixbus;   // peak trim after mixbus compressor
};

// Full mastering result for one file.
struct MasterResult {
    AnalysisSnapshot  analysis;
    AdviceSet         advice;
    float             preGainDb  = 0.f;   // gain applied before analysis (dB)
    GainStageReport   gainStages;
    bool              ok         = false;
    std::string       errMsg;
};

// Progress callback: fraction in [0, 1], description string.
using ProgressCallback = std::function<void(float fraction, const std::string& stage)>;

// Analyse + derive advice without rendering.
MasterResult analyseOnly(const std::string& inputPath,
                         const PresetData&  preset,
                         std::string*       errOut = nullptr);

// Full mastering render: load → analyse → derive → apply DSP chain → write.
// adviceOverride: if provided, uses those values instead of deriving from analysis.
MasterResult renderFile(const std::string&      inputPath,
                        const std::string&      outputPath,
                        const PresetData&       preset,
                        const RenderOptions&    opts           = {},
                        const AdviceSet*        adviceOverride = nullptr,
                        const ProgressCallback& progress       = {},
                        std::string*            errOut         = nullptr);

} // namespace mt
```

- [ ] **Step 2: Add `#include` and two call sites in `pipeline.cpp`**

Add to the `#include` block at the top of `pipeline.cpp`:

```cpp
#include "mastertweak/dsp/gain_stager.hpp"
```

Then in `renderFile()`, add two blocks. The full diff is:

1. After `const int nf = audio->numFrames;`, add:

```cpp
    // Snapshot the reference RMS (post-pre-gain, pre-DSP) for inter-stage restore.
    dsp::GainStager gs;
    const float rmsDbBeforeChain = dsp::GainStager::measureRmsDb(buf, nf);
```

2. Replace the multiband comp block with:

```cpp
    // ── Multiband compressor ─────────────────────────────────────────────────
    if (!opts.bypassMbComp) {
        report(0.35f, "Multiband compression");
        dsp::MultibandComp mbComp;
        mbComp.prepare(sr, nch);
        mbComp.setAdvice(adv.mbComp);
        mbComp.process(buf, nf);
    }

    // ── Post-multiband gain staging ──────────────────────────────────────────
    // Restore RMS to pre-chain reference so saturator and mixbus comp see
    // the level their advice was calibrated for.
    report(0.40f, "Gain staging (post-multiband)");
    result.gainStages.postMbComp = gs.restoreRms(buf, nf, rmsDbBeforeChain);
```

3. Replace the mixbus comp block with:

```cpp
    // ── Mixbus compressor ─────────────────────────────────────────────────────
    if (!opts.bypassMixbusComp) {
        report(0.70f, "Mixbus compression");
        dsp::MixbusComp mbusComp;
        mbusComp.prepare(sr, nch);
        mbusComp.setAdvice(adv.mixbusComp);
        mbusComp.process(buf, nf);
    }

    // ── Post-mixbus gain staging ─────────────────────────────────────────────
    // Trim peaks > -3 dBFS so the limiter operates in its clean range rather
    // than fighting makeup-gain overshoot.
    report(0.73f, "Gain staging (post-mixbus)");
    result.gainStages.postMixbus = gs.trimPeak(buf, nf, -3.f);
```

- [ ] **Step 3: Build and verify all tests still pass**

```bash
cmake --build build --parallel 2>&1 && ctest --test-dir build --output-on-failure 2>&1
```

Expected: clean build, all tests pass (including existing pipeline tests).

- [ ] **Step 4: Commit**

```bash
git add core/include/mastertweak/pipeline.hpp core/src/pipeline.cpp
git commit -m "feat: integrate GainStager at post-multiband and post-mixbus points"
```

---

### Task 4: Update TODO + final verification

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Mark TODO item done**

In `TODO.md`, replace:

```
- Ensure optimal gain staging before/between each part of the mastering chain to ensure best audio treatment quality
```

with:

```
- ~~Ensure optimal gain staging before/between each part of the mastering chain to ensure best audio treatment quality~~ **DONE** — `dsp::GainStager` (restoreRms after multiband comp, trimPeak after mixbus comp); per-stage levels in `MasterResult::gainStages`.
```

- [ ] **Step 2: Run full test suite one final time**

```bash
cmake --build build --parallel 2>&1 && ctest --test-dir build --output-on-failure 2>&1
```

Expected: all tests pass.

- [ ] **Step 3: Final commit**

```bash
git add TODO.md
git commit -m "docs: mark gain staging TODO as done"
```
