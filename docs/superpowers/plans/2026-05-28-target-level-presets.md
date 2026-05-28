# Target Level Presets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `TargetLevelProfile` (LUFS + true-peak ceiling) to `RenderOptions` so the pipeline can normalise output loudness to an industry-standard platform target via EBU R128 measurement between the mixbus compressor and the limiter.

**Architecture:** `target_level.hpp` defines the struct and 8-entry built-in table. `LufsAnalyser` (new DSP class) implements EBU R128 gated integrated loudness. `renderFile()` inserts a measurement + gain-trim step after the mixbus comp and before the limiter when `opts.targetLevel` is set; the ceiling is also overridden to the target's true-peak value. GUI gets a `TargetLevelCombo` widget added to the render row; CLI gets `--target-level` and `--list-targets`.

**Tech Stack:** C++20, doctest, Qt6, CMake/Ninja. EBU R128 / ITU-R BS.1770-4 for the loudness algorithm. Existing `BiquadCoeffs`/`BiquadState` from `biquad.hpp` for K-weighting filters.

---

## File Map

| Action  | Path |
|---------|------|
| Create  | `core/include/mastertweak/target_level.hpp` |
| Create  | `core/src/target_level.cpp` |
| Create  | `core/include/mastertweak/dsp/lufs_analyser.hpp` |
| Create  | `core/src/dsp/lufs_analyser.cpp` |
| Create  | `tests/test_target_level.cpp` |
| Create  | `gui/TargetLevelCombo.h` |
| Create  | `gui/TargetLevelCombo.cpp` |
| Modify  | `core/include/mastertweak/pipeline.hpp` |
| Modify  | `core/src/pipeline.cpp` |
| Modify  | `core/CMakeLists.txt` |
| Modify  | `tests/CMakeLists.txt` |
| Modify  | `gui/CMakeLists.txt` |
| Modify  | `gui/MainWindow.h` |
| Modify  | `gui/MainWindow.cpp` |
| Modify  | `cli/main.cpp` |

---

## Task 1: TargetLevelProfile data model

**Files:**
- Create: `core/include/mastertweak/target_level.hpp`
- Create: `core/src/target_level.cpp`
- Create: `tests/test_target_level.cpp`
- Modify: `core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write test_target_level.cpp with table sanity and lookup tests**

```cpp
// tests/test_target_level.cpp
#include "mastertweak/target_level.hpp"
#include <doctest.h>
#include <cmath>

TEST_CASE("target_level: table has exactly 8 entries") {
    CHECK(mt::kTargetLevelProfiles.size() == 8);
}

TEST_CASE("target_level: all entries have negative LUFS and non-positive ceiling") {
    for (const auto& p : mt::kTargetLevelProfiles) {
        CHECK(p.lufs < 0.f);
        CHECK(p.peakCeiling <= 0.f);
        CHECK(!p.name.empty());
    }
}

TEST_CASE("target_level: findTargetLevel case-insensitive hit") {
    const auto* p = mt::findTargetLevel("spotify");
    REQUIRE(p != nullptr);
    CHECK(p->lufs == doctest::Approx(-14.f));
    CHECK(p->peakCeiling == doctest::Approx(-1.f));
}

TEST_CASE("target_level: findTargetLevel uppercase") {
    CHECK(mt::findTargetLevel("APPLE MUSIC") != nullptr);
}

TEST_CASE("target_level: findTargetLevel unknown returns nullptr") {
    CHECK(mt::findTargetLevel("does not exist") == nullptr);
}

TEST_CASE("target_level: CD / Download entry exists") {
    const auto* p = mt::findTargetLevel("cd / download");
    REQUIRE(p != nullptr);
    CHECK(p->lufs == doctest::Approx(-9.f));
    CHECK(p->peakCeiling == doctest::Approx(-0.1f));
}
```

- [ ] **Step 2: Add test file to tests/CMakeLists.txt**

Open `tests/CMakeLists.txt`. Add `test_target_level.cpp` to the `add_executable` source list:

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
)
```

- [ ] **Step 3: Run tests — expect linker errors (header not yet created)**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
cmake --build build --parallel 2>&1 | tail -20
```

Expected: build fails because `target_level.hpp` does not exist.

- [ ] **Step 4: Create core/include/mastertweak/target_level.hpp**

```cpp
#pragma once

#include <array>
#include <string>
#include <string_view>

namespace mt {

struct TargetLevelProfile {
    std::string name;
    float lufs;         // integrated loudness target (LUFS)
    float peakCeiling;  // true-peak ceiling (dBTP)
};

extern const std::array<TargetLevelProfile, 8> kTargetLevelProfiles;

// Case-insensitive lookup. Returns nullptr if not found.
const TargetLevelProfile* findTargetLevel(std::string_view name);

} // namespace mt
```

- [ ] **Step 5: Create core/src/target_level.cpp**

```cpp
#include "mastertweak/target_level.hpp"

#include <algorithm>
#include <cctype>

namespace mt {

const std::array<TargetLevelProfile, 8> kTargetLevelProfiles = {{
    {"Spotify",              -14.f, -1.f  },
    {"YouTube",              -14.f, -1.f  },
    {"Amazon Music",         -14.f, -1.f  },
    {"Tidal",                -14.f, -1.f  },
    {"Apple Music",          -16.f, -1.f  },
    {"CD / Download",         -9.f, -0.1f },
    {"Broadcast EBU R128",   -23.f, -1.f  },
    {"Broadcast ATSC A/85",  -24.f, -2.f  },
}};

const TargetLevelProfile* findTargetLevel(std::string_view name) {
    // Convert query to lowercase for comparison
    std::string query{name};
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    for (const auto& p : kTargetLevelProfiles) {
        std::string lower{p.name};
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (lower == query) return &p;
    }
    return nullptr;
}

} // namespace mt
```

- [ ] **Step 6: Add target_level.cpp to core/CMakeLists.txt**

Open `core/CMakeLists.txt`. Add `src/target_level.cpp` to the source list:

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
)
```

- [ ] **Step 7: Build and run table tests**

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure -R mastertweak_tests
```

Expected: all existing tests pass; the 6 new `target_level:` tests pass.

- [ ] **Step 8: Commit**

```bash
git add core/include/mastertweak/target_level.hpp \
        core/src/target_level.cpp \
        core/CMakeLists.txt \
        tests/test_target_level.cpp \
        tests/CMakeLists.txt
git commit -m "feat: add TargetLevelProfile data model and built-in platform table"
```

---

## Task 2: LufsAnalyser — header and failing tests

**Files:**
- Create: `core/include/mastertweak/dsp/lufs_analyser.hpp`
- Modify: `tests/test_target_level.cpp`

**Background:** EBU R128 integrated loudness uses two K-weighting biquad stages (BS.1770 Stage 1: high-shelf at 1682 Hz; Stage 2: high-pass at 38 Hz), mean-square power over 400 ms blocks with 75% overlap, absolute gate at −70 LUFS, relative gate at −10 LU below the ungated mean, then: `L = −0.691 + 10·log₁₀(gated_mean_power)`. For a stereo sine at peak amplitude `A` with K-weighting ≈ 0 dB (e.g., 100 Hz): `expected_LUFS ≈ −0.691 + 20·log₁₀(A)`.

- [ ] **Step 1: Append LufsAnalyser tests to test_target_level.cpp**

```cpp
// Append to tests/test_target_level.cpp:

#include "mastertweak/dsp/lufs_analyser.hpp"
#include <numbers>
#include <vector>

static constexpr float kSr = 44100.f;
static constexpr int   kSrI = 44100;

// Make a stereo 100 Hz sine of given amplitude and length in seconds.
// 100 Hz is well below the K-weighting shelf (1682 Hz) and well above
// the high-pass (38 Hz), so K-weighting gain is approximately 0 dB there.
static std::vector<std::vector<float>> makeTestSine(float ampPeak, float durationSec) {
    const int frames = static_cast<int>(durationSec * kSr);
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames)));
    for (int i = 0; i < frames; ++i) {
        const float s = ampPeak * std::sin(2.f * std::numbers::pi_v<float>
                                           * 100.f * static_cast<float>(i) / kSr);
        buf[0][static_cast<size_t>(i)] = s;
        buf[1][static_cast<size_t>(i)] = s;
    }
    return buf;
}

TEST_CASE("LufsAnalyser: silence returns floor value") {
    const int frames = kSrI * 3;  // 3 seconds
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames), 0.f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, frames);
    CHECK(lufs <= -69.f);  // at or below the absolute gate floor
}

TEST_CASE("LufsAnalyser: short buffer below one block returns floor") {
    // 100ms < 400ms block — no complete blocks, should return floor
    const int frames = kSrI / 10;
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames), 0.1f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, frames);
    CHECK(lufs <= -69.f);
}

TEST_CASE("LufsAnalyser: stereo 100 Hz sine at -20 dBFS -> ~-20.7 LUFS") {
    // Peak amplitude -20 dBFS: A = 10^(-20/20) = 0.1
    // With K-weighting ~0 dB at 100 Hz:
    //   z = A^2 (sum of ms over L and R)
    //   LUFS = -0.691 + 10*log10(0.01) = -0.691 - 20 = -20.691
    const float A = std::pow(10.f, -20.f / 20.f);
    auto buf = makeTestSine(A, 3.f);
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, static_cast<int>(3.f * kSr));
    CHECK(lufs > -21.5f);
    CHECK(lufs < -19.5f);
}

TEST_CASE("LufsAnalyser: mono input does not crash") {
    const int frames = kSrI * 3;
    std::vector<std::vector<float>> buf(1, std::vector<float>(static_cast<size_t>(frames), 0.05f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 1);
    const float lufs = la.measure(buf, frames);
    CHECK(std::isfinite(lufs));
}
```

- [ ] **Step 2: Create core/include/mastertweak/dsp/lufs_analyser.hpp**

```cpp
#pragma once

#include "mastertweak/dsp/biquad.hpp"

#include <vector>

namespace mt::dsp {

// EBU R128 integrated loudness measurement (ITU-R BS.1770-4).
// K-weighting: Stage 1 high-shelf (1682 Hz, +4 dB) + Stage 2 high-pass (38 Hz).
// 400 ms blocks, 75% overlap, absolute gate -70 LUFS, relative gate -10 LU.
// Returns: L = -0.691 + 10*log10(gated_mean_power).
// Returns -70.f when no blocks survive gating (silence or sub-block buffers).
class LufsAnalyser {
public:
    void prepare(float sampleRate, int numChannels);

    // Measure integrated LUFS of the full buffer [numChannels × numFrames].
    float measure(const std::vector<std::vector<float>>& samples, int numFrames);

private:
    static BiquadCoeffs kWeightingStage1(double sr);
    static BiquadCoeffs kWeightingStage2(double sr);

    float sampleRate_  = 48000.f;
    int   numChannels_ = 2;
    int   blockSize_   = 0;   // 400 ms in samples
    int   hopSize_     = 0;   // 100 ms in samples (75% overlap)
    BiquadCoeffs stage1_{};
    BiquadCoeffs stage2_{};
};

} // namespace mt::dsp
```

- [ ] **Step 3: Attempt build — expect linker error (implementation missing)**

```bash
cmake --build build --parallel 2>&1 | tail -10
```

Expected: fails because `LufsAnalyser` methods are not yet defined.

---

## Task 3: LufsAnalyser implementation

**Files:**
- Create: `core/src/dsp/lufs_analyser.cpp`
- Modify: `core/CMakeLists.txt`

- [ ] **Step 1: Create core/src/dsp/lufs_analyser.cpp**

```cpp
#include "mastertweak/dsp/lufs_analyser.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace mt::dsp {

// ── K-weighting coefficient factories ────────────────────────────────────────
//
// BS.1770 Stage 1: 2nd-order high-shelf, f0=1681.97 Hz, +4 dB gain, Q=0.7072.
// Bilinear transform with K = tan(pi * f0 / sr).
BiquadCoeffs LufsAnalyser::kWeightingStage1(double sr) {
    const double f0 = 1681.974450955533;
    const double G  = 3.999843853;
    const double Q  = 0.7071752369554196;
    const double K  = std::tan(std::numbers::pi_v<double> * f0 / sr);
    const double K2 = K * K;
    const double Vh = std::pow(10.0, G / 20.0);  // linear amplitude gain
    const double Vb = std::pow(10.0, G / 40.0);  // ≈ sqrt(Vh)
    const double n  = 1.0 / (1.0 + K / Q + K2);
    return BiquadCoeffs{
        float((Vh + Vb * K / Q + K2) * n),
        float(2.0  * (K2 - Vh)       * n),
        float((Vh - Vb * K / Q + K2) * n),
        float(2.0  * (K2 - 1.0)      * n),
        float((1.0 - K / Q + K2)     * n)
    };
}

// BS.1770 Stage 2: 2nd-order high-pass, f0=38.135 Hz, Q=0.5003 (Butterworth).
BiquadCoeffs LufsAnalyser::kWeightingStage2(double sr) {
    const double f0 = 38.13547087602444;
    const double Q  = 0.5003270373238773;
    const double K  = std::tan(std::numbers::pi_v<double> * f0 / sr);
    const double K2 = K * K;
    const double n  = 1.0 / (1.0 + K / Q + K2);
    return BiquadCoeffs{
        float(1.0            * n),
        float(-2.0           * n),
        float(1.0            * n),
        float(2.0 * (K2 - 1.0) * n),
        float((1.0 - K / Q + K2) * n)
    };
}

// ── Public API ────────────────────────────────────────────────────────────────

void LufsAnalyser::prepare(float sampleRate, int numChannels) {
    sampleRate_  = sampleRate;
    numChannels_ = numChannels;
    blockSize_   = static_cast<int>(0.4  * sampleRate);  // 400 ms
    hopSize_     = static_cast<int>(0.1  * sampleRate);  // 100 ms (75% overlap)
    stage1_ = kWeightingStage1(static_cast<double>(sampleRate));
    stage2_ = kWeightingStage2(static_cast<double>(sampleRate));
}

float LufsAnalyser::measure(const std::vector<std::vector<float>>& samples,
                             int numFrames) {
    const auto nch = static_cast<size_t>(numChannels_);

    // ── Step 1: K-weight entire buffer per channel (single pass, O(n)) ───────
    // Pre-filtering the full buffer avoids restarting filter state per block
    // (which would be O(n²) in the number of blocks).
    std::vector<std::vector<float>> kw(nch, std::vector<float>(static_cast<size_t>(numFrames)));
    for (size_t ch = 0; ch < nch; ++ch) {
        BiquadState s1{}, s2{};
        for (int f = 0; f < numFrames; ++f) {
            const float x  = samples[ch][static_cast<size_t>(f)];
            const float y1 = biquadProcess(stage1_, s1, x);
            kw[ch][static_cast<size_t>(f)] = biquadProcess(stage2_, s2, y1);
        }
    }

    // ── Step 2: Compute mean-square power per 400ms block (75% overlap) ──────
    std::vector<double> blockPowers;
    for (int offset = 0; offset + blockSize_ <= numFrames; offset += hopSize_) {
        double z = 0.0;
        for (size_t ch = 0; ch < nch; ++ch) {
            double sumSq = 0.0;
            for (int f = offset; f < offset + blockSize_; ++f)
                sumSq += static_cast<double>(kw[ch][static_cast<size_t>(f)])
                       * kw[ch][static_cast<size_t>(f)];
            z += sumSq / blockSize_;  // mean square, this channel
        }
        blockPowers.push_back(z);
    }

    if (blockPowers.empty()) return -70.f;

    // ── Step 2: Absolute gate — discard blocks below -70 LUFS ────────────────
    // -70 LUFS ↔ -0.691 + 10*log10(z) = -70 → z = 10^((-70+0.691)/10)
    constexpr double kAbsGateZ = 1.0954e-7;  // 10^(-69.309/10)
    std::vector<double> gated1;
    for (double z : blockPowers)
        if (z >= kAbsGateZ) gated1.push_back(z);

    if (gated1.empty()) return -70.f;

    // ── Step 3: Relative gate — discard blocks > 10 LU below ungated mean ──
    double Jg = 0.0;
    for (double z : gated1) Jg += z;
    Jg /= static_cast<double>(gated1.size());

    const double relGateZ = Jg * 0.1;  // 10 LU = factor 10 in power
    std::vector<double> gated2;
    for (double z : gated1)
        if (z >= relGateZ) gated2.push_back(z);

    if (gated2.empty()) return -70.f;

    // ── Step 4: Gated mean and LUFS ──────────────────────────────────────────
    double mean = 0.0;
    for (double z : gated2) mean += z;
    mean /= static_cast<double>(gated2.size());

    return static_cast<float>(-0.691 + 10.0 * std::log10(mean));
}

} // namespace mt::dsp
```

- [ ] **Step 2: Add lufs_analyser.cpp to core/CMakeLists.txt**

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
)
```

- [ ] **Step 3: Build and run LufsAnalyser tests**

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure -R mastertweak_tests
```

Expected: all tests pass including the 4 new `LufsAnalyser:` tests.

- [ ] **Step 4: Commit**

```bash
git add core/include/mastertweak/dsp/lufs_analyser.hpp \
        core/src/dsp/lufs_analyser.cpp \
        core/CMakeLists.txt \
        tests/test_target_level.cpp
git commit -m "feat: add LufsAnalyser (EBU R128 integrated loudness measurement)"
```

---

## Task 4: Pipeline integration

**Files:**
- Modify: `core/include/mastertweak/pipeline.hpp`
- Modify: `core/src/pipeline.cpp`
- Modify: `tests/test_target_level.cpp`

- [ ] **Step 1: Add pipeline integration test to test_target_level.cpp**

Append to `tests/test_target_level.cpp`:

```cpp
#include "mastertweak/pipeline.hpp"
#include "mastertweak/io.hpp"
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("pipeline: Spotify target normalises output to ~-14 LUFS") {
    // Build a flat preset whose overallRmsDb = -14 to avoid extreme gain trims.
    mt::PresetData preset;
    preset.name           = "test-flat";
    preset.overallRmsDb   = -14.f;
    preset.overallMinCorr = 0.5f;
    for (int i = 0; i < 7; ++i) {
        preset.bandRmsDb[static_cast<size_t>(i)]       = -18.f;
        preset.bandMinCorr[static_cast<size_t>(i)]     =  0.5f;
        preset.bandTransientDb[static_cast<size_t>(i)] =  8.f;
    }

    // Write 3-second 100 Hz stereo sine at -20 dBFS.
    const std::string inPath  = (fs::temp_directory_path() / "mt_lufs_in.wav").string();
    const std::string outPath = (fs::temp_directory_path() / "mt_lufs_out.wav").string();
    {
        mt::AudioFile af;
        af.sampleRate  = kSrI;
        af.numChannels = 2;
        af.numFrames   = kSrI * 3;
        af.bitDepth    = 24;
        af.samples.assign(2, std::vector<float>(static_cast<size_t>(kSrI * 3)));
        const float amp = std::pow(10.f, -20.f / 20.f);
        for (int i = 0; i < kSrI * 3; ++i) {
            const float s = amp * std::sin(2.f * std::numbers::pi_v<float>
                                           * 100.f * static_cast<float>(i) / kSr);
            af.samples[0][static_cast<size_t>(i)] = s;
            af.samples[1][static_cast<size_t>(i)] = s;
        }
        REQUIRE(mt::writeAudioFile(inPath, af));
    }

    // Render with Spotify target.
    mt::RenderOptions opts;
    opts.targetLevel = *mt::findTargetLevel("spotify");

    std::string err;
    auto result = mt::renderFile(inPath, outPath, preset, opts, nullptr, {}, &err);
    REQUIRE_MESSAGE(result.ok, "render failed: " << err);

    // Re-measure output LUFS.
    auto out = mt::readAudioFile(outPath);
    REQUIRE(out.has_value());

    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float measuredLufs = la.measure(out->samples, out->numFrames);

    // Should land within ±2 LU of the Spotify target (-14 LUFS).
    CHECK(measuredLufs > -16.f);
    CHECK(measuredLufs < -12.f);

    // True-peak ceiling must not exceed -1 dBTP (≈ 0.891 linear).
    float maxAbs = 0.f;
    for (const auto& ch : out->samples)
        for (auto s : ch)
            maxAbs = std::max(maxAbs, std::abs(s));
    CHECK(maxAbs <= 0.892f);
}
```

- [ ] **Step 2: Verify test fails (targetLevel field does not exist yet)**

```bash
cmake --build build --parallel 2>&1 | grep "targetLevel" | head -5
```

Expected: compile error — `targetLevel` is not a member of `RenderOptions`.

- [ ] **Step 3: Add targetLevel to RenderOptions in pipeline.hpp**

Open `core/include/mastertweak/pipeline.hpp`. Add the include and the new field:

```cpp
#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/preset.hpp"
#include "mastertweak/target_level.hpp"

#include <functional>
#include <optional>
#include <string>

namespace mt {

struct RenderOptions {
    int  outputBitDepth = 24;
    bool outputFlac     = false;
    bool bypassEq       = false;
    bool bypassMbComp   = false;
    bool bypassSaturator= false;
    bool bypassWidth    = false;
    bool bypassMixbusComp = false;
    bool bypassLimiter  = false;
    bool bypassDither   = false;
    std::optional<TargetLevelProfile> targetLevel;  // nullopt = derive from advice
};
```

- [ ] **Step 4: Add LUFS measurement + gain trim to pipeline.cpp**

Open `core/src/pipeline.cpp`. Add the include at the top:

```cpp
#include "mastertweak/dsp/lufs_analyser.hpp"
#include "mastertweak/target_level.hpp"
```

Then in `renderFile()`, replace the block between the mixbus comp step and the limiter step.

Current code (lines ~98–114):
```cpp
    // ── Mixbus compressor ─────────────────────────────────────────────────────
    if (!opts.bypassMixbusComp) {
        report(0.70f, "Mixbus compression");
        dsp::MixbusComp mbusComp;
        mbusComp.prepare(sr, nch);
        mbusComp.setAdvice(adv.mixbusComp);
        mbusComp.process(buf, nf);
    }

    // ── Limiter ───────────────────────────────────────────────────────────────
    if (!opts.bypassLimiter) {
        report(0.80f, "Limiting");
        dsp::Limiter lim;
        lim.prepare(sr, nch);
        lim.setAdvice(adv.limiter);
        lim.process(buf, nf);
    }
```

Replace with:

```cpp
    // ── Mixbus compressor ─────────────────────────────────────────────────────
    if (!opts.bypassMixbusComp) {
        report(0.70f, "Mixbus compression");
        dsp::MixbusComp mbusComp;
        mbusComp.prepare(sr, nch);
        mbusComp.setAdvice(adv.mixbusComp);
        mbusComp.process(buf, nf);
    }

    // ── Target-level normalisation (EBU R128) ────────────────────────────────
    if (opts.targetLevel.has_value()) {
        report(0.75f, "Normalising to target");
        dsp::LufsAnalyser la;
        la.prepare(sr, nch);
        const float measuredLufs = la.measure(buf, nf);
        float trimDb = opts.targetLevel->lufs - measuredLufs;
        // Clamp to ±24 dB to guard against silence or very short inputs.
        trimDb = std::clamp(trimDb, -24.f, 24.f);
        const float gain = std::pow(10.f, trimDb / 20.f);
        for (auto& ch : buf)
            for (int f = 0; f < nf; ++f)
                ch[static_cast<size_t>(f)] *= gain;
        // Override the advice ceiling with the target's true-peak ceiling.
        result.advice.limiter.ceilingDb = opts.targetLevel->peakCeiling;
    }

    // ── Limiter ───────────────────────────────────────────────────────────────
    if (!opts.bypassLimiter) {
        report(0.80f, "Limiting");
        dsp::Limiter lim;
        lim.prepare(sr, nch);
        lim.setAdvice(result.advice.limiter);
        lim.process(buf, nf);
    }
```

Note: `lim.setAdvice` now uses `result.advice.limiter` (which may have the overridden ceiling) instead of `adv.limiter`.

- [ ] **Step 5: Build and run all tests**

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

Expected: all tests pass including `pipeline: Spotify target normalises output to ~-14 LUFS`.

- [ ] **Step 6: Commit**

```bash
git add core/include/mastertweak/pipeline.hpp \
        core/src/pipeline.cpp \
        tests/test_target_level.cpp
git commit -m "feat: integrate LUFS normalisation into renderFile pipeline"
```

---

## Task 5: TargetLevelCombo GUI widget

**Files:**
- Create: `gui/TargetLevelCombo.h`
- Create: `gui/TargetLevelCombo.cpp`
- Modify: `gui/CMakeLists.txt`

- [ ] **Step 1: Create gui/TargetLevelCombo.h**

```cpp
#pragma once

#include "mastertweak/target_level.hpp"

#include <QComboBox>
#include <optional>

namespace gui {

class TargetLevelCombo : public QComboBox {
    Q_OBJECT
public:
    explicit TargetLevelCombo(QWidget* parent = nullptr);
    std::optional<mt::TargetLevelProfile> currentTarget() const;

signals:
    void targetChanged(std::optional<mt::TargetLevelProfile> profile);

private slots:
    void onIndexChanged(int index);
};

} // namespace gui
```

- [ ] **Step 2: Create gui/TargetLevelCombo.cpp**

```cpp
#include "TargetLevelCombo.h"

namespace gui {

TargetLevelCombo::TargetLevelCombo(QWidget* parent) : QComboBox(parent) {
    addItem("Auto (from preset)");
    for (const auto& p : mt::kTargetLevelProfiles) {
        const QString label = QString("%1 — %2 LUFS / %3 dBTP")
            .arg(QString::fromStdString(p.name))
            .arg(static_cast<double>(p.lufs),         0, 'f', 1)
            .arg(static_cast<double>(p.peakCeiling),  0, 'f', 1);
        addItem(label);
    }
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TargetLevelCombo::onIndexChanged);
}

std::optional<mt::TargetLevelProfile> TargetLevelCombo::currentTarget() const {
    const int idx = currentIndex();
    if (idx <= 0) return std::nullopt;
    return mt::kTargetLevelProfiles[static_cast<size_t>(idx - 1)];
}

void TargetLevelCombo::onIndexChanged(int index) {
    if (index <= 0)
        emit targetChanged(std::nullopt);
    else
        emit targetChanged(mt::kTargetLevelProfiles[static_cast<size_t>(index - 1)]);
}

} // namespace gui
```

- [ ] **Step 3: Add files to gui/CMakeLists.txt**

```cmake
qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    PresetSelector.h
    PresetSelector.cpp
    AnalysisPanel.h
    AnalysisPanel.cpp
    ParameterPanel.h
    ParameterPanel.cpp
    TransportWidget.h
    TransportWidget.cpp
    TargetLevelCombo.h
    TargetLevelCombo.cpp
)
```

- [ ] **Step 4: Build GUI (no wiring yet — just verify it compiles)**

```bash
cmake --build build --parallel 2>&1 | grep -E "error:|warning:" | head -20
```

Expected: builds cleanly (no errors).

- [ ] **Step 5: Commit**

```bash
git add gui/TargetLevelCombo.h gui/TargetLevelCombo.cpp gui/CMakeLists.txt
git commit -m "feat: add TargetLevelCombo Qt widget"
```

---

## Task 6: MainWindow wiring

**Files:**
- Modify: `gui/MainWindow.h`
- Modify: `gui/MainWindow.cpp`

- [ ] **Step 1: Add targetCombo_ member to MainWindow.h**

Open `gui/MainWindow.h`. Add the include and member. Find the private members section and add:

```cpp
#include "TargetLevelCombo.h"
```

In the `private:` member list (alongside `bitDepthCombo_`, `flacCheck_`), add:

```cpp
    TargetLevelCombo*   targetCombo_   = nullptr;
```

- [ ] **Step 2: Add combo to the render row in buildUi()**

Open `gui/MainWindow.cpp`. In `buildUi()`, find the render row block (the block that adds `depthLbl`, `bitDepthCombo_`, `flacCheck_`, `statusLabel_`). After the `flacCheck_` lines and before `statusLabel_`, insert:

```cpp
        auto* targetLbl = new QLabel("Target:", central);
        targetCombo_ = new TargetLevelCombo(central);

        row->addWidget(targetLbl);
        row->addWidget(targetCombo_);
```

The full render row after the change:

```cpp
    {
        auto* row = new QHBoxLayout;

        renderButton_ = new QPushButton("Render", central);
        saveButton_   = new QPushButton("Save As…", central);
        renderButton_->setEnabled(false);
        saveButton_->setEnabled(false);

        auto* depthLbl = new QLabel("Bit depth:", central);
        bitDepthCombo_ = new QComboBox(central);
        bitDepthCombo_->addItem("16-bit", 16);
        bitDepthCombo_->addItem("24-bit", 24);
        bitDepthCombo_->addItem("32-bit float", 32);
        bitDepthCombo_->setCurrentIndex(1);

        flacCheck_ = new QCheckBox("FLAC", central);

        auto* targetLbl = new QLabel("Target:", central);
        targetCombo_ = new TargetLevelCombo(central);

        statusLabel_ = new QLabel("Ready", central);

        connect(renderButton_, &QPushButton::clicked, this, &MainWindow::onRenderClicked);
        connect(saveButton_,   &QPushButton::clicked, this, &MainWindow::onSaveAs);

        row->addWidget(renderButton_);
        row->addWidget(saveButton_);
        row->addSpacing(16);
        row->addWidget(depthLbl);
        row->addWidget(bitDepthCombo_);
        row->addWidget(flacCheck_);
        row->addSpacing(8);
        row->addWidget(targetLbl);
        row->addWidget(targetCombo_);
        row->addWidget(statusLabel_, 1);
        vbox->addLayout(row);
    }
```

- [ ] **Step 3: Read targetLevel in onRenderClicked()**

In `onRenderClicked()`, find the lines that build `opts`:

```cpp
    mt::RenderOptions opts = parameterPanel_->getBypassOptions();
    opts.outputBitDepth = bitDepthCombo_->currentData().toInt();
    opts.outputFlac     = useFlac;
```

Add one line after `opts.outputFlac`:

```cpp
    opts.targetLevel = targetCombo_->currentTarget();
```

- [ ] **Step 4: Build and launch GUI**

```bash
cmake --build build --parallel && ./build/gui/MasterTweak
```

Expected: GUI launches. The render row shows `Target: [Auto (from preset) ▾]` next to the FLAC checkbox. Opening the combo shows 9 items (Auto + 8 platforms).

- [ ] **Step 5: Commit**

```bash
git add gui/MainWindow.h gui/MainWindow.cpp
git commit -m "feat: wire TargetLevelCombo into MainWindow render row"
```

---

## Task 7: CLI flags

**Files:**
- Modify: `cli/main.cpp`

- [ ] **Step 1: Add `--target-level` and `--list-targets` flags**

Open `cli/main.cpp`. Add the include near the top with other core includes:

```cpp
#include "mastertweak/target_level.hpp"
```

In `main()`, after the existing `--bypass` option block and before the advice override block, add:

```cpp
    // ── Target level ─────────────────────────────────────────────────────────
    std::string targetLevelName;
    app.add_option("--target-level", targetLevelName,
                   "Set output loudness target by platform name (case-insensitive).\n"
                   "  e.g. --target-level spotify\n"
                   "  Use --list-targets to see all valid names.");

    bool listTargets = false;
    app.add_flag("--list-targets", listTargets,
                 "Print built-in target levels and exit.");
```

After `CLI11_PARSE(app, argc, argv);` and before the preset resolution block, add:

```cpp
    // ── --list-targets ────────────────────────────────────────────────────────
    if (listTargets) {
        std::printf("Built-in target levels:\n");
        for (const auto& p : mt::kTargetLevelProfiles)
            std::printf("  %-24s  %5.1f LUFS / %4.1f dBTP\n",
                        p.name.c_str(), p.lufs, p.peakCeiling);
        return 0;
    }
```

In `buildRenderOpts()`, add a parameter for the target level, OR resolve it in `main()` after preset resolution. The cleanest approach is to resolve in `main()` and set directly on the built opts object. After the `const mt::RenderOptions renderOpts = buildRenderOpts(...)` line, add:

```cpp
    // ── Resolve --target-level ────────────────────────────────────────────────
    mt::RenderOptions renderOpts = buildRenderOpts(bitDepth, outputFormat == "flac", bypasses);
    if (!targetLevelName.empty()) {
        const auto* tp = mt::findTargetLevel(targetLevelName);
        if (!tp) {
            std::fprintf(stderr, "error: unknown target level '%s'.\n",
                         targetLevelName.c_str());
            std::fprintf(stderr, "Built-in target levels:\n");
            for (const auto& p : mt::kTargetLevelProfiles)
                std::fprintf(stderr, "  %-24s  %5.1f LUFS / %4.1f dBTP\n",
                             p.name.c_str(), p.lufs, p.peakCeiling);
            return 1;
        }
        renderOpts.targetLevel = *tp;
    }
```

Note: change the existing `const mt::RenderOptions renderOpts = buildRenderOpts(...)` declaration to `mt::RenderOptions renderOpts = buildRenderOpts(...)` (drop the `const`) so `targetLevel` can be assigned to it.

- [ ] **Step 2: Build CLI**

```bash
cmake --build build --parallel 2>&1 | grep -E "error:" | head -10
```

Expected: builds cleanly.

- [ ] **Step 3: Smoke-test --list-targets**

```bash
./build/cli/mastertweak --list-targets
```

Expected output:
```
Built-in target levels:
  Spotify                    -14.0 LUFS /  -1.0 dBTP
  YouTube                    -14.0 LUFS /  -1.0 dBTP
  Amazon Music               -14.0 LUFS /  -1.0 dBTP
  Tidal                      -14.0 LUFS /  -1.0 dBTP
  Apple Music                -16.0 LUFS /  -1.0 dBTP
  CD / Download               -9.0 LUFS /  -0.1 dBTP
  Broadcast EBU R128         -23.0 LUFS /  -1.0 dBTP
  Broadcast ATSC A/85        -24.0 LUFS /  -2.0 dBTP
```

- [ ] **Step 4: Smoke-test unknown name**

```bash
./build/cli/mastertweak --target-level "bad name" --preset nonexistent /dev/null 2>&1
```

Expected: prints error message with the table and exits with code 1.

- [ ] **Step 5: Run full test suite one final time**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add cli/main.cpp
git commit -m "feat: add --target-level and --list-targets CLI flags"
```
