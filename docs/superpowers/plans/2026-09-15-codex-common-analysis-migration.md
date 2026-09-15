# Codex → Common (dsp/analysis) Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repoint Codex/MasterTweak's `core/` library at `AudioPlugins/Common`'s newly-extracted `common/dsp` and `common/analysis` modules (`v0.5.0`) instead of Codex's own private, duplicated implementations — the second step of Phase 4 Stage A of the AudioPlugins JUCE→DPF migration roadmap (Common's side, Plan A1, is done and merged: PR [#3](https://github.com/TriYop/spellbound-common/pull/3)).

**Architecture:** Codex's own public API (`mt::` namespace: `AnalysisSnapshot`, `PresetData`, `ResonancePeak`, `AdviceSet`, `LufsAnalyser`, `loadPreset`, `detectResonances`, `deriveAdvice`, etc.) does **not change** — every GUI/CLI/test call site keeps working unmodified. Only the *implementations* behind those names change: pure-math pieces with field-identical layouts (`Biquad`, `SevenBandSplitter`, `Fft`) become type aliases onto `audioplugins::common::dsp`; domain pieces whose Codex-local shape carries extra fields Common's doesn't have (`PresetData::kNumBands`, `ResonancePeak::gainDb`/`enabled`) become thin conversion wrappers around `audioplugins::common::analysis`. This is the same hexagonal adapter pattern already used elsewhere in the workspace (e.g. `ReliquaryVoiceAdapter`).

**Tech Stack:** C++20, CMake + Ninja, doctest (existing suite, unmodified), `FetchContent` for `AudioPluginsCommon` (this is Codex's first-ever dependency on `Common`).

**Spec:** `/home/yvan/Projects/AudioPlugins/Common/docs/superpowers/specs/2026-09-13-truesight-common-analysis-migration-design.md`

## Global Constraints

- Codex's public `mt::` API surface (types, function signatures, member names) must not change — every existing GUI/CLI/test call site compiles and behaves identically with no edits, except where a task explicitly says otherwise (only Task 6/resonance detection has an accepted approximate-behavior change, see that task).
- Regression gate: the full existing doctest suite (`ctest --test-dir build --output-on-failure`, or `./build/tests/mastertweak_tests`) must keep passing after every task. No test file is rewritten; at most a test's *expected values* change if a task's own notes say so (none do here — see Task 6's note on why its loose assertions already tolerate the algorithm change).
- `AudioPluginsCommon::dsp` and `AudioPluginsCommon::analysis` are C++20, framework-free, no GUI/DPF dependency — safe for a non-DPF, non-JUCE consumer like Codex (confirmed in `Common/CLAUDE.md`'s design notes).
- Codex's CMake targets stay as they are (`mastertweak_core`, `mastertweak`, `MasterTweak`, `preset_builder_core`, `mastertweak-backfill-analysis`) — this plan only touches `core/`'s internals and the top-level `CMakeLists.txt`/`third_party/CMakeLists.txt`/CI wiring needed to pull in `Common`.
- Work happens on a new branch off `main` (not `fix/static-link-portability`, which has unrelated, already-committed work — don't touch it). Suggested branch name: `feat/consume-common-analysis`.
- Commit after every task.

---

### Task 0: Wire `Common` into Codex's build, resolve the `pugixml` target collision

**Files:**
- Modify: `CMakeLists.txt` (top level)
- Modify: `third_party/CMakeLists.txt`
- Modify: `core/CMakeLists.txt`
- Modify: `.github/workflows/cmake-multi-platform.yml`

**Interfaces:**
- Produces: `AudioPluginsCommon::dsp`, `AudioPluginsCommon::analysis` CMake targets available to every subdirectory added after this point. `pugixml` becomes Common's fetched copy (target name unchanged, so every existing `#include <pugixml.hpp>` / `target_link_libraries(... pugixml)` call site keeps working).

Codex vendors its own `pugixml` as a local static-lib target named `pugixml` (`third_party/CMakeLists.txt:1-3`). `Common`'s own `CMakeLists.txt` also `FetchContent`s pugixml and creates a target literally named `pugixml` (`Common/CMakeLists.txt:33-39`). If both run in the same CMake configure, `add_library(pugixml ...)` fails with a duplicate-target error. Resolution: drop Codex's vendored copy and let Common's fetched pugixml (v1.16 vs Codex's currently-vendored v1.14 — a minor, low-risk bump for a stable XML library) satisfy every existing `pugixml` link.

- [ ] **Step 1: Remove Codex's vendored pugixml target**

In `third_party/CMakeLists.txt`, delete lines 1-3:

```cmake
add_library(pugixml STATIC pugixml/pugixml.cpp)
target_include_directories(pugixml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pugixml)
target_compile_options(pugixml PRIVATE -w)
```

Leave `miniaudio`/`cli11`/`doctest`/`picosha2`/`rtmidi` untouched. Do **not** delete the `third_party/pugixml/` vendored source directory itself in this step — leave it on disk for now (harmless, unreferenced); a follow-up cleanup can remove it later, out of scope here.

- [ ] **Step 2: Add the `AudioPluginsCommon` FetchContent block**

In the top-level `CMakeLists.txt`, the existing `add_subdirectory(third_party)` line already sits immediately before `add_subdirectory(core)` (in the block that also has `add_subdirectory(preset_builder)`/`add_subdirectory(cli)`/`add_subdirectory(tools)`) — no reordering needed. Insert the `FetchContent` block immediately above that existing `add_subdirectory(third_party)` line, so `pugixml`, `AudioPluginsCommon::dsp`, and `AudioPluginsCommon::analysis` all exist before `core/CMakeLists.txt` needs them:

```cmake
include(FetchContent)
FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.5.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

add_subdirectory(third_party)
add_subdirectory(core)
add_subdirectory(preset_builder)
add_subdirectory(cli)
add_subdirectory(tools)
```

(The last four lines above are the existing lines, shown only to make the insertion point unambiguous — only the `include(FetchContent)`...`FetchContent_MakeAvailable(AudioPluginsCommon)` block is new.) `AudioPluginsCommon`'s repo is private on GitHub; Step 4 below adds the CI auth step that makes this clone work in Actions the same way Hex/Pugilist/Outflank already do it. Local dev machines need `git` configured with access already (same as every other plugin's `Common` dependency).

- [ ] **Step 3: Point `core/CMakeLists.txt`'s pugixml link at the same target name (no change needed)**

Since Common's fetched pugixml target is also named `pugixml`, `core/CMakeLists.txt`'s existing line:

```cmake
target_link_libraries(mastertweak_core
    PUBLIC
        $<IF:$<BOOL:${MASTERTWEAK_STATIC}>,SndFile::sndfile,PkgConfig::SNDFILE>
    PRIVATE
        pugixml
)
```

needs no edit — it now resolves to Common's target automatically. Confirm this by grepping for any other `pugixml` reference that might assume Codex's old local path:

```bash
grep -rn "third_party/pugixml\|pugixml/pugixml" --include="*.txt" --include="*.cpp" --include="*.hpp" . | grep -v build
```

Expected: no hits outside `third_party/CMakeLists.txt` (already fixed in Step 1) and the still-present-but-now-unused `third_party/pugixml/` source directory itself.

- [ ] **Step 4: Add CI auth for Common's private repo**

`Common` is a private GitHub repo; CI needs the same `COMMON_REPO_TOKEN` secret and `git config` step Hex/Pugilist already use. In `.github/workflows/cmake-multi-platform.yml`, insert this step right after `- uses: actions/checkout@v4` and before `- name: Set reusable strings`:

```yaml
    - name: Configure Common repo access
      shell: bash
      run: git config --global url."https://x-access-token:${{ secrets.COMMON_REPO_TOKEN }}@github.com/TriYop/spellbound-common".insteadOf "https://github.com/TriYop/spellbound-common"
```

`COMMON_REPO_TOKEN` must exist as a repo secret in `TriYop/mastertweak` (or whatever Codex's GitHub repo is actually named) — the same fine-grained PAT already used by the other plugin repos consuming `Common`; if it isn't set yet, the workflow will fail with an auth error on the `FetchContent` clone and the secret needs adding via the GitHub repo settings (outside this plan's scope — flag to the user if CI fails here).

- [ ] **Step 5: Configure, build, and run the full existing suite to confirm nothing broke**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: configure succeeds (no duplicate-target error), full build succeeds, **every existing test still passes** — this task is purely additive plumbing, nothing consumes `Common` yet.

- [ ] **Step 6: Commit**

```bash
git checkout -b feat/consume-common-analysis main
git add CMakeLists.txt third_party/CMakeLists.txt .github/workflows/cmake-multi-platform.yml
git commit -m "build: add AudioPluginsCommon dependency, dedupe pugixml target"
```

---

### Task 1: `mt::dsp` Biquad → alias `AudioPluginsCommon::dsp`

**Files:**
- Modify: `core/include/mastertweak/dsp/biquad.hpp` (full rewrite, much shorter)
- Modify: `core/CMakeLists.txt` (link `AudioPluginsCommon::dsp` into `mastertweak_core`)

**Interfaces:**
- Consumes: `audioplugins::common::dsp::BiquadCoeffs`, `BiquadState`, `biquadProcess` (Common `v0.5.0`, header-only-compatible since `mt::dsp` is currently header-only for Biquad too).
- Produces: `mt::dsp::BiquadCoeffs`, `mt::dsp::BiquadState`, `mt::dsp::biquadProcess` — same names, same call sites everywhere in Codex (`analysis.cpp`, `dsp/parametric_eq.cpp`, `dsp/linkwitz_riley.cpp` pre-Task-2, tests) keep compiling unchanged.

Codex's current `biquad.hpp` (`core/include/mastertweak/dsp/biquad.hpp`) is a self-contained, header-only, `inline`-function implementation of the exact same RBJ cookbook formulas Common's `audioplugins::common::dsp::Biquad` already has (confirmed byte-for-byte identical during Common's Plan A1 port). Struct field names/order match exactly (`b0,b1,b2,a1,a2` / `s0,s1`), so a type alias is safe and correct — no conversion needed anywhere.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R test_dsp --output-on-failure` (existing binary from Task 0's build)
Expected: PASS — this is the pre-change baseline for the three `biquad ...` `TEST_CASE`s in `tests/test_dsp.cpp:46-86`.

- [ ] **Step 2: Replace `biquad.hpp` with an alias header**

```cpp
// core/include/mastertweak/dsp/biquad.hpp
#pragma once

#include "audioplugins/common/dsp/Biquad.h"

namespace mt::dsp {

using BiquadCoeffs = audioplugins::common::dsp::BiquadCoeffs;
using BiquadState  = audioplugins::common::dsp::BiquadState;
using audioplugins::common::dsp::biquadProcess;

} // namespace mt::dsp
```

- [ ] **Step 3: Link `AudioPluginsCommon::dsp` into `mastertweak_core`**

In `core/CMakeLists.txt`, change:

```cmake
target_link_libraries(mastertweak_core
    PUBLIC
        $<IF:$<BOOL:${MASTERTWEAK_STATIC}>,SndFile::sndfile,PkgConfig::SNDFILE>
    PRIVATE
        pugixml
)
```

to:

```cmake
target_link_libraries(mastertweak_core
    PUBLIC
        $<IF:$<BOOL:${MASTERTWEAK_STATIC}>,SndFile::sndfile,PkgConfig::SNDFILE>
        AudioPluginsCommon::dsp
        AudioPluginsCommon::analysis
    PRIVATE
        pugixml
)
```

(Both `dsp` and `analysis` are added now since Tasks 2-8 all need one or the other and this is the one `target_link_libraries` call site — avoids six near-identical CMake edits later.)

- [ ] **Step 4: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_dsp --output-on-failure`
Expected: PASS, same as Step 1 — behavior is bit-for-bit identical since the underlying formulas are the same code, just relocated.

- [ ] **Step 5: Commit**

```bash
git add core/include/mastertweak/dsp/biquad.hpp core/CMakeLists.txt
git commit -m "refactor(dsp): alias mt::dsp::Biquad onto AudioPluginsCommon::dsp"
```

---

### Task 2: `mt::dsp::SevenBandSplitter` + `analysis.cpp`'s local `LR4` → `AudioPluginsCommon::dsp`

**Files:**
- Modify: `core/include/mastertweak/dsp/linkwitz_riley.hpp` (full rewrite, much shorter)
- Delete: `core/src/dsp/linkwitz_riley.cpp`
- Modify: `core/CMakeLists.txt` (remove `src/dsp/linkwitz_riley.cpp` from the source list)
- Modify: `core/src/analysis.cpp` (replace its own separate, duplicate local `LR4` struct)

**Interfaces:**
- Consumes: `audioplugins::common::dsp::SevenBandSplitter`, `audioplugins::common::dsp::LinkwitzRileyCrossover` (Common `v0.5.0`).
- Produces: `mt::dsp::SevenBandSplitter` — same name/API, consumed unchanged by `core/include/mastertweak/dsp/stereo_width.hpp` and `core/include/mastertweak/dsp/multiband_comp.hpp`. `analysis.cpp`'s `analyseFile()` internals change (no public API change — `AnalysisSnapshot analyseFile(const AudioFile&)` keeps its signature).

Codex today has **two independent, duplicate implementations** of the same 4th-order Linkwitz-Riley math: the public `mt::dsp::SevenBandSplitter` (with a private nested `LR4` struct, `core/src/dsp/linkwitz_riley.cpp`) used by `stereo_width`/`multiband_comp`, and a *second*, separate anonymous-namespace `LR4` struct hand-rolled directly inside `core/src/analysis.cpp:23-40` for the band-stats pass. Common's port (`audioplugins::common::dsp::LinkwitzRileyCrossover` + `SevenBandSplitter`) replaces both — `SevenBandSplitter` via alias (buffer-oriented API matches exactly), `analysis.cpp`'s local `LR4` via direct use of the lower-level `LinkwitzRileyCrossover` primitive (per-sample `processLowpass`/`processHighpass`, which is what `analysis.cpp` needs since it interleaves per-band statistics gathering with the cascade, not a whole-block splitter call).

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R "test_dsp|test_analysis" --output-on-failure`
Expected: PASS — `test_dsp.cpp`'s `Compressor:`/`Saturator:`/`Limiter:` cases exercise the multiband chain indirectly, and `test_analysis.cpp`'s 6 cases (`tests/test_analysis.cpp:32-107`) directly exercise `analyseFile()`'s crossover + percentile pipeline.

- [ ] **Step 2: Replace `linkwitz_riley.hpp` with an alias header**

```cpp
// core/include/mastertweak/dsp/linkwitz_riley.hpp
#pragma once

#include "audioplugins/common/dsp/SevenBandSplitter.h"

namespace mt::dsp {

using SevenBandSplitter = audioplugins::common::dsp::SevenBandSplitter;

} // namespace mt::dsp
```

- [ ] **Step 3: Delete `core/src/dsp/linkwitz_riley.cpp` and remove it from `core/CMakeLists.txt`**

```bash
rm core/src/dsp/linkwitz_riley.cpp
```

In `core/CMakeLists.txt`, remove the line `src/dsp/linkwitz_riley.cpp` from the `add_library(mastertweak_core STATIC ...)` source list.

- [ ] **Step 4: Replace `analysis.cpp`'s local `LR4` with `LinkwitzRileyCrossover`**

In `core/src/analysis.cpp`, delete the anonymous-namespace `LR4` struct (lines 22-40) and its `#include "mastertweak/dsp/biquad.hpp"` become unneeded directly (still transitively pulled in via the new header, but the explicit `dsp::BiquadCoeffs`/`dsp::biquadProcess` calls go away with the struct). Replace:

```cpp
#include "mastertweak/dsp/biquad.hpp"
```

with:

```cpp
#include "audioplugins/common/dsp/LinkwitzRileyCrossover.h"
```

and delete the `struct LR4 { ... };` block entirely (lines 22-40 in the current file).

Then, in `analyseFile()`, change the filter setup:

```cpp
    // Set up the LR4 cascaded crossover bank
    std::array<LR4, kNumCrossovers> lpFilters, hpFilters;
    for (int i = 0; i < kNumCrossovers; ++i) {
        const float fc = AnalysisSnapshot::kCrossoverHz[static_cast<size_t>(i)];
        lpFilters[static_cast<size_t>(i)].setLowpass(fc, sr);
        hpFilters[static_cast<size_t>(i)].setHighpass(fc, sr);
    }
```

to:

```cpp
    // Set up the crossover bank (mono per filter: process() is called
    // per-channel below with an explicit channel index, so numChannels=1
    // here and each LR4-equivalent tracks 2 "channels" worth of state via
    // separate L/R filter instances — matches the old LR4's [stage][channel]
    // layout by using channel index 0 for L and a second array for R).
    std::array<audioplugins::common::dsp::LinkwitzRileyCrossover, kNumCrossovers> lpFiltersL, lpFiltersR;
    std::array<audioplugins::common::dsp::LinkwitzRileyCrossover, kNumCrossovers> hpFiltersL, hpFiltersR;
    for (int i = 0; i < kNumCrossovers; ++i) {
        const float fc = AnalysisSnapshot::kCrossoverHz[static_cast<size_t>(i)];
        lpFiltersL[static_cast<size_t>(i)].setLowpass(fc, sr, 1);
        lpFiltersR[static_cast<size_t>(i)].setLowpass(fc, sr, 1);
        hpFiltersL[static_cast<size_t>(i)].setHighpass(fc, sr, 1);
        hpFiltersR[static_cast<size_t>(i)].setHighpass(fc, sr, 1);
    }
```

and the cascade loop:

```cpp
        // Cascaded LR4 filterbank
        for (size_t ci = 0; ci < static_cast<size_t>(kNumCrossovers); ++ci) {
            for (int i = 0; i < n; ++i) {
                band0[static_cast<size_t>(i)] = lpFilters[ci].process(0, remainder0[static_cast<size_t>(i)]);
                band1[static_cast<size_t>(i)] = lpFilters[ci].process(1, remainder1[static_cast<size_t>(i)]);
                remainder0[static_cast<size_t>(i)] = hpFilters[ci].process(0, remainder0[static_cast<size_t>(i)]);
                remainder1[static_cast<size_t>(i)] = hpFilters[ci].process(1, remainder1[static_cast<size_t>(i)]);
            }
            storeBand(ci, band0.data(), band1.data());
        }
```

to:

```cpp
        // Cascaded crossover filterbank
        for (size_t ci = 0; ci < static_cast<size_t>(kNumCrossovers); ++ci) {
            for (int i = 0; i < n; ++i) {
                band0[static_cast<size_t>(i)] = lpFiltersL[ci].processLowpass(0, remainder0[static_cast<size_t>(i)]);
                band1[static_cast<size_t>(i)] = lpFiltersR[ci].processLowpass(0, remainder1[static_cast<size_t>(i)]);
                remainder0[static_cast<size_t>(i)] = hpFiltersL[ci].processHighpass(0, remainder0[static_cast<size_t>(i)]);
                remainder1[static_cast<size_t>(i)] = hpFiltersR[ci].processHighpass(0, remainder1[static_cast<size_t>(i)]);
            }
            storeBand(ci, band0.data(), band1.data());
        }
```

(Using channel index `0` on separate L/R filter instances rather than channel indices `0`/`1` on one shared instance — `LinkwitzRileyCrossover` tracks state per `numChannels` slot internally, and using two single-channel instances is equivalent to one two-channel instance addressed by index; either works, this keeps the diff smaller by not touching the `prepare(..., numChannels)` call shape elsewhere in the function.)

- [ ] **Step 5: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R "test_dsp|test_analysis" --output-on-failure`
Expected: PASS. `test_analysis.cpp`'s numeric assertions (`±2 dB` RMS tolerance, band-index checks, percentile ordering) have enough headroom that the switch from one LR4 struct to an algorithmically-identical one changes nothing observable.

- [ ] **Step 6: Commit**

```bash
git add core/include/mastertweak/dsp/linkwitz_riley.hpp core/src/analysis.cpp core/CMakeLists.txt
git rm core/src/dsp/linkwitz_riley.cpp
git commit -m "refactor(dsp): alias mt::dsp::SevenBandSplitter onto AudioPluginsCommon::dsp, dedupe analysis.cpp's own LR4 copy"
```

---

### Task 3: `mt::dsp::inplaceFft`/`averagedMagnitudeSpectrum` → alias `AudioPluginsCommon::dsp`

**Files:**
- Modify: `core/include/mastertweak/dsp/fft.hpp` (full rewrite, much shorter)
- Delete: `core/src/dsp/fft.cpp`
- Modify: `core/CMakeLists.txt` (remove `src/dsp/fft.cpp` from the source list)

**Interfaces:**
- Consumes: `audioplugins::common::dsp::inplaceFft`, `averagedMagnitudeSpectrum` (Common `v0.5.0`).
- Produces: `mt::dsp::inplaceFft`, `mt::dsp::averagedMagnitudeSpectrum` — same names, consumed unchanged by `core/src/resonance_detector.cpp` (until Task 6 replaces most of that file anyway) and any future caller.

Identical radix-2 Cooley-Tukey DIT algorithm, ported near-verbatim per the spec — signatures match exactly.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R test_resonance_detector --output-on-failure` (the only current consumer of the FFT path)
Expected: PASS — this is the pre-change baseline for `tests/test_resonance_detector.cpp`'s 5 cases.

- [ ] **Step 2: Replace `fft.hpp` with an alias header**

```cpp
// core/include/mastertweak/dsp/fft.hpp
#pragma once

#include "audioplugins/common/dsp/Fft.h"

namespace mt::dsp {

using audioplugins::common::dsp::inplaceFft;
using audioplugins::common::dsp::averagedMagnitudeSpectrum;

} // namespace mt::dsp
```

- [ ] **Step 3: Delete `core/src/dsp/fft.cpp` and remove it from `core/CMakeLists.txt`**

```bash
rm core/src/dsp/fft.cpp
```

Remove the line `src/dsp/fft.cpp` from `core/CMakeLists.txt`'s source list.

- [ ] **Step 4: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_resonance_detector --output-on-failure`
Expected: PASS, unchanged — bit-identical algorithm.

- [ ] **Step 5: Commit**

```bash
git add core/include/mastertweak/dsp/fft.hpp core/CMakeLists.txt
git rm core/src/dsp/fft.cpp
git commit -m "refactor(dsp): alias mt::dsp::Fft onto AudioPluginsCommon::dsp"
```

---

### Task 4: `mt::AnalysisSnapshot` band constants → source from `AudioPluginsCommon::analysis::BandConfig`

**Files:**
- Modify: `core/include/mastertweak/analysis.hpp`

**Interfaces:**
- Consumes: `audioplugins::common::analysis::BandConfig::{crossoverHz, bandNames, bandCenterHz, bandIsShelf}` (Common `v0.5.0`).
- Produces: `mt::AnalysisSnapshot::{kCrossoverHz, kBandNames, kBandIsShelf, kBandCenterHz}` — same names, same types, same values, on the same struct. Every call site (`advice.cpp`, `analysis.cpp`, `report.cpp`, tests) keeps compiling and behaving identically since the *values* were already numerically identical (confirmed during Common's Plan A1 port — see `Common/docs/superpowers/plans/2026-09-13-common-dsp-analysis-modules.md` Task 5's note).

`mt::AnalysisSnapshot`'s struct shape, `BandStats` fields, and `kNumBands`/`bands`/`overall*`/`lraLu` members all stay exactly as they are — only the four *band-constant* static members switch from hand-duplicated literals to referencing `BandConfig`'s single source of truth.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R "test_advice|test_analysis|test_lra" --output-on-failure`
Expected: PASS.

- [ ] **Step 2: Edit `analysis.hpp`'s band-constant declarations**

In `core/include/mastertweak/analysis.hpp`, add the include:

```cpp
#include "audioplugins/common/analysis/BandConfig.h"
```

Change:

```cpp
    static constexpr int kNumBands = 7;
    static constexpr std::array<float, kNumBands - 1> kCrossoverHz = {
        80.f, 250.f, 500.f, 2000.f, 6000.f, 16000.f
    };
    static constexpr std::array<const char*, kNumBands> kBandNames = {
        "Sub", "Lows", "Lo-Mid", "Mids", "Hi-Mid", "Highs", "Air"
    };
    // true = this band should be advised as a shelf filter, not a bell
    static constexpr std::array<bool, kNumBands> kBandIsShelf = {
        true, false, false, false, false, false, true
    };
    // Representative EQ frequencies per band (matches MixAdvice BandConfig)
    static constexpr std::array<float, kNumBands> kBandCenterHz = {
        50.f, 160.f, 375.f, 1000.f, 3500.f, 10000.f, 16000.f
    };
```

to:

```cpp
    static constexpr int kNumBands = audioplugins::common::analysis::BandConfig::numBands;
    static constexpr auto kCrossoverHz  = audioplugins::common::analysis::BandConfig::crossoverHz;
    static constexpr auto kBandNames    = audioplugins::common::analysis::BandConfig::bandNames;
    static constexpr auto kBandIsShelf  = audioplugins::common::analysis::BandConfig::bandIsShelf;
    static constexpr auto kBandCenterHz = audioplugins::common::analysis::BandConfig::bandCenterHz;
```

- [ ] **Step 3: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R "test_advice|test_analysis|test_lra" --output-on-failure`
Expected: PASS — values are numerically identical, only their origin changed.

- [ ] **Step 4: Commit**

```bash
git add core/include/mastertweak/analysis.hpp
git commit -m "refactor(analysis): source AnalysisSnapshot's band constants from AudioPluginsCommon::analysis::BandConfig"
```

---

### Task 5: `mt::loadPreset`/`loadPresetsFromDir` → delegate to `AudioPluginsCommon::analysis::PresetIO`

**Files:**
- Modify: `core/src/preset.cpp`

**Interfaces:**
- Consumes: `audioplugins::common::analysis::PresetIO::load`, `loadFromDirectory`, `audioplugins::common::analysis::PresetData` (Common `v0.5.0`).
- Produces: `mt::loadPreset`, `mt::loadPresetsFromDir` — same signatures, same behavior (same XML schema, same error semantics). `mt::resolvePreset`/`enumeratePresets` (Codex-specific search-path logic) are **unmodified** — they already call `loadPreset`/`loadPresetsFromDir` internally, so they get the new backing implementation for free with zero edits.

Common's `PresetIO::load`/`loadFromDirectory` is a direct port of Codex's own `loadPreset`/`loadPresetsFromDir` pugixml-parsing logic (per the spec: "NOT `resolvePreset`/`enumeratePresets`, which encode Codex-specific search-path conventions... belong in Codex's own app-glue code"). `mt::PresetData` and `audioplugins::common::analysis::PresetData` have identical field names/types/order except `mt::PresetData` also declares `static constexpr int kNumBands` (used elsewhere in Codex, e.g. `advice.hpp`'s `AdviceSet::kNumBands`) — so this task converts field-by-field rather than aliasing the struct itself.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R test_preset --output-on-failure`
Expected: PASS — 5 cases in `tests/test_preset.cpp` (the 5th, "loads a real MixAdvice preset if available," was already updated on `fix/static-link-portability`'s commit to point at `TrueSight/Presets/fest-noz.xml`; that fix is a separate branch and not part of this plan's diff, but its behavior is unaffected either way since it's a self-skipping test when the file isn't present).

- [ ] **Step 2: Replace `loadPreset`/`loadPresetsFromDir`'s bodies with delegation**

In `core/src/preset.cpp`, delete the `parsePresetNode` helper and the `kBandAttrNames` array (lines 15-64 — this exact XML-parsing logic now lives in `Common`), and change the includes:

```cpp
#include "mastertweak/preset.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>
```

to:

```cpp
#include "mastertweak/preset.hpp"

#include "audioplugins/common/analysis/PresetIO.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

namespace {

mt::PresetData toMtPresetData(const audioplugins::common::analysis::PresetData& src) {
    mt::PresetData out;
    out.name            = src.name;
    out.description      = src.description;
    out.bandRmsDb        = src.bandRmsDb;
    out.bandMinCorr      = src.bandMinCorr;
    out.bandTransientDb  = src.bandTransientDb;
    out.overallRmsDb     = src.overallRmsDb;
    out.overallMinCorr   = src.overallMinCorr;
    return out;
}

} // namespace
```

Then replace:

```cpp
std::optional<PresetData> loadPreset(const std::string& xmlPath, std::string* errOut) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());
    if (!result) {
        if (errOut) *errOut = result.description();
        return std::nullopt;
    }

    PresetData p;
    if (!parsePresetNode(doc.first_child(), p, errOut))
        return std::nullopt;
    return p;
}

std::vector<PresetData> loadPresetsFromDir(const std::string& dirPath) {
    std::vector<PresetData> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".xml") continue;
        if (auto p = loadPreset(entry.path().string()))
            out.push_back(std::move(*p));
    }
    return out;
}
```

with:

```cpp
std::optional<PresetData> loadPreset(const std::string& xmlPath, std::string* errOut) {
    auto p = audioplugins::common::analysis::PresetIO::load(xmlPath, errOut);
    if (!p) return std::nullopt;
    return toMtPresetData(*p);
}

std::vector<PresetData> loadPresetsFromDir(const std::string& dirPath) {
    const auto common = audioplugins::common::analysis::PresetIO::loadFromDirectory(dirPath);
    std::vector<PresetData> out;
    out.reserve(common.size());
    for (const auto& p : common) out.push_back(toMtPresetData(p));
    return out;
}
```

`resolvePreset`/`enumeratePresets` (the rest of the file, lines 92-152 in the pre-change version) are left untouched — they already only call `loadPreset`/`loadPresetsFromDir`.

- [ ] **Step 3: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_preset --output-on-failure`
Expected: PASS — same XML schema, same field mapping, same error propagation (`PresetIO::load`'s `errOut` semantics match Codex's original: pugixml parse error or the port's schema-mismatch messages).

- [ ] **Step 4: Commit**

```bash
git add core/src/preset.cpp
git commit -m "refactor(preset): delegate mt::loadPreset/loadPresetsFromDir to AudioPluginsCommon::analysis::PresetIO"
```

---

### Task 6: `mt::detectResonances` → delegate to `AudioPluginsCommon::analysis::pickResonancePeaks`

**Files:**
- Modify: `core/src/resonance_detector.cpp`

**Interfaces:**
- Consumes: `audioplugins::common::analysis::pickResonancePeaks(const float* magDb, int numBins, float sampleRate, int fftSize, float minFreqHz, float maxFreqHz, float minQ, float prominenceDbThreshold, int maxResults)` → `std::vector<audioplugins::common::analysis::ResonancePeak>` (fields: `freqHz`, `q`, `prominenceDb`) (Common `v0.5.0`).
- Produces: `mt::detectResonances(const AudioFile&)` → `std::vector<mt::ResonancePeak>` (fields: `freqHz`, `q`, `gainDb`, `enabled`) — same signature and return shape as today; `gainDb`/`enabled` (which Common's `ResonancePeak` doesn't carry) are computed locally at the boundary, exactly reproducing Codex's current formula.

**Accepted behavior change, flagged explicitly:** Common's `pickResonancePeaks` uses an O(1) prefix-sum background-level estimate ported from TrueSight, which *supersedes* (per the spec) Codex's original O(n·window) ±1-octave moving-average estimate in `detectResonances`'s current `bgDb` loop (`core/src/resonance_detector.cpp:59-70`, deleted by this task). The two algorithms are not guaranteed to produce bit-identical peak lists on every input — this is the one piece of Stage A where "regression gate" means the existing test's *assertions* still hold, not that the implementation is unchanged. `tests/test_resonance_detector.cpp`'s 5 cases were written with enough tolerance for this: white noise → "≤ 2 peaks" (loose bound), 4kHz sine → peak within ±200Hz with `gainDb < 0` and `q >= 3` (wide bounds), silence → 0 peaks. If Step 4 below shows any of these fail after the swap, that's a real finding to report, not something to paper over by loosening the test further — stop and flag it rather than adjusting the assertion to make it pass.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R test_resonance_detector --output-on-failure`
Expected: PASS (this task's real regression oracle — re-run after the change in Step 3).

- [ ] **Step 2: Replace the peak-picking body, keep the mono-downmix + spectrum + gain-mapping local**

In `core/src/resonance_detector.cpp`, delete the "Step 4: compute background level" through "Step 8: build ResonancePeak results" blocks (current lines ~53-155, i.e. everything from the `bgDb` computation through the final `results` loop) and the now-unused `Candidate` struct. Change the include:

```cpp
#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/dsp/fft.hpp"
```

to:

```cpp
#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/dsp/fft.hpp"
#include "audioplugins/common/analysis/ResonancePeakPicker.h"
```

Keep the function's Step 1 (mono mix) and Step 2/3 (`averagedMagnitudeSpectrum` + dB conversion) exactly as they are today — those already delegate to the now-aliased `mt::dsp::averagedMagnitudeSpectrum` (Task 3) and are unrelated to the background-estimate change. Replace everything from the `bgDb` loop onward with:

```cpp
    // Step 4-7: peak-pick via Common's O(1) prefix-sum background estimate
    // (supersedes this file's former ±1-octave moving-average approach).
    const auto peaks = audioplugins::common::analysis::pickResonancePeaks(
        magDb.data(), halfN, static_cast<float>(audio.sampleRate), kFftN,
        kMinFreqHz, kMaxFreqHz, kMinQ, kProminenceDb, kMaxResults);

    // Step 8: map Common's {freqHz,q,prominenceDb} onto mt::ResonancePeak's
    // {freqHz,q,gainDb,enabled} -- gainDb reproduces this file's original
    // formula (negative = attenuation), enabled defaults to true, matching
    // today's behavior exactly.
    std::vector<ResonancePeak> results;
    results.reserve(peaks.size());
    for (const auto& p : peaks) {
        ResonancePeak peak;
        peak.freqHz  = p.freqHz;
        peak.q       = p.q;
        peak.gainDb  = -std::clamp(p.prominenceDb, kMinGainDb, kMaxGainDb);
        peak.enabled = true;
        results.push_back(peak);
    }

    return results;
```

The function's existing constants (`kFftN`, `kMinFreqHz`, `kMaxFreqHz`, `kMinQ`, `kMaxResults`, `kProminenceDb`, `kMaxGainDb`, `kMinGainDb`) and its early `if (audio.numFrames == 0 ...) return {};` guard stay exactly as they are.

- [ ] **Step 3: Rebuild and run the regression oracle**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_resonance_detector --output-on-failure`
Expected: PASS. If any case fails, do not loosen the assertion — report which case failed and by how much; that's a genuine finding about the algorithm swap, not a plan-execution bug.

- [ ] **Step 4: Commit**

```bash
git add core/src/resonance_detector.cpp
git commit -m "refactor(analysis): delegate mt::detectResonances' peak-picking to AudioPluginsCommon::analysis::pickResonancePeaks"
```

---

### Task 7: `mt::dsp::LufsAnalyser` → wrap `AudioPluginsCommon::analysis::LoudnessAnalyser`

**Files:**
- Modify: `core/include/mastertweak/dsp/lufs_analyser.hpp`
- Modify: `core/src/analysis.cpp` (replace `dsp::lufs_analyser.cpp`'s deleted file's `.cpp` with a new, much shorter one, OR fold into the header — see Step 2)
- Delete: `core/src/dsp/lufs_analyser.cpp`
- Modify: `core/CMakeLists.txt` (remove `src/dsp/lufs_analyser.cpp` from the source list, add the new small `.cpp`)

**Interfaces:**
- Consumes: `audioplugins::common::analysis::LoudnessAnalyser` (`prepare(double, int)`, `measure(...)`, `measureWithLra(...)`), `audioplugins::common::analysis::LoudnessMetrics` (Common `v0.5.0`).
- Produces: `mt::dsp::LufsAnalyser` — same class name, same public methods (`prepare(float, int)`, `measure(...)`, `measureWithLra(...)`), same `mt::dsp::LoudnessMetrics` shape (`integratedLufs`, `lra`). Every call site (`analysis.cpp`'s `analyseFile()`, `tests/test_lra.cpp`) keeps compiling unchanged.

Common's `LoudnessAnalyser` batch API (`measure`/`measureWithLra`) is "ported from Codex's `mastertweak/dsp/lufs_analyser.cpp`" per the spec — same K-weighting coefficients, same 400ms/-10LU integrated gate, same 3s/-20LU LRA gate. `mt::dsp::LufsAnalyser` becomes a thin wrapper class holding one `audioplugins::common::analysis::LoudnessAnalyser` member.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R test_lra --output-on-failure`
Expected: PASS — all 7 cases in `tests/test_lra.cpp` (4 `measureWithLra` cases + 3 `deriveAdvice` LRA-offset cases; the latter aren't affected by this task, they're covered again in Task 8).

- [ ] **Step 2: Rewrite `lufs_analyser.hpp` as a thin wrapper**

```cpp
// core/include/mastertweak/dsp/lufs_analyser.hpp
#pragma once

#include "audioplugins/common/analysis/LoudnessAnalyser.h"

#include <vector>

namespace mt::dsp {

using LoudnessMetrics = audioplugins::common::analysis::LoudnessMetrics;

// Thin wrapper: forwards to AudioPluginsCommon::analysis::LoudnessAnalyser,
// which is itself ported from this file's original K-weighting/gating math
// (see Common's design spec). Kept as a distinct mt::dsp class -- rather
// than a bare alias -- only because this class historically took a float
// sampleRate in prepare() while Common's takes double; the wrapper absorbs
// that conversion so no call site needs to change.
class LufsAnalyser {
public:
    void prepare(float sampleRate, int numChannels) {
        impl_.prepare(static_cast<double>(sampleRate), numChannels);
    }

    float measure(const std::vector<std::vector<float>>& samples, int numFrames) {
        return impl_.measure(samples, numFrames);
    }

    LoudnessMetrics measureWithLra(const std::vector<std::vector<float>>& samples, int numFrames) {
        return impl_.measureWithLra(samples, numFrames);
    }

private:
    audioplugins::common::analysis::LoudnessAnalyser impl_;
};

} // namespace mt::dsp
```

- [ ] **Step 3: Delete `core/src/dsp/lufs_analyser.cpp` and remove it from `core/CMakeLists.txt`**

```bash
rm core/src/dsp/lufs_analyser.cpp
```

Remove the line `src/dsp/lufs_analyser.cpp` from `core/CMakeLists.txt`'s source list. No replacement `.cpp` is needed — the wrapper class above is entirely inline in the header.

- [ ] **Step 4: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R test_lra --output-on-failure`
Expected: PASS — same K-weighting/gating math, bit-identical algorithm.

- [ ] **Step 5: Commit**

```bash
git add core/include/mastertweak/dsp/lufs_analyser.hpp core/CMakeLists.txt
git rm core/src/dsp/lufs_analyser.cpp
git commit -m "refactor(analysis): wrap mt::dsp::LufsAnalyser around AudioPluginsCommon::analysis::LoudnessAnalyser"
```

---

### Task 8: `mt::deriveAdvice` → delegate to `AudioPluginsCommon::analysis::deriveAdvice`

**Files:**
- Modify: `core/src/advice.cpp`

**Interfaces:**
- Consumes: `audioplugins::common::analysis::deriveAdvice(const AnalysisSnapshot&, const PresetData&)` → `audioplugins::common::analysis::AdviceSet` (Common `v0.5.0`).
- Produces: `mt::deriveAdvice(const mt::AnalysisSnapshot&, const mt::PresetData&)` → `mt::AdviceSet` — same signature, same computed values for every field Common's version populates (`eq`, `mbComp`, `width`, `mixbusComp`, `saturator`, `limiter` — the exact same percentile-blend/LRA-offset algorithm, since Common's version is itself the port of this file). `resonances` stays empty on the returned `AdviceSet`, exactly matching today's behavior (`deriveAdvice` never populated it — `pipeline.cpp` sets `result.advice.resonances = detectResonances(*audio)` separately, unaffected by this task).

`mt::AnalysisSnapshot`/`mt::PresetData` and Common's equivalents have identical field layouts (confirmed in Tasks 4-5); `mt::AdviceSet`'s `eq`/`mbComp`/`width`/`mixbusComp`/`saturator`/`limiter` sub-structs (`BandEq`, `BandComp`, `MixbusComp`, `SaturatorParams`, `BandWidth`, `LimiterParams`) also match Common's field-for-field.

- [ ] **Step 1: Confirm the current regression baseline**

Run: `ctest --test-dir build -R "test_advice|test_lra" --output-on-failure`
Expected: PASS — `tests/test_advice.cpp`'s 8 cases plus `tests/test_lra.cpp`'s 3 `deriveAdvice` LRA-offset cases.

- [ ] **Step 2: Replace `advice.cpp`'s body with conversion + delegation**

Replace the entire file:

```cpp
#include "mastertweak/advice.hpp"

#include "audioplugins/common/analysis/AdviceSet.h"

namespace mt {

namespace {

namespace ca = audioplugins::common::analysis;

ca::AnalysisSnapshot toCommonSnapshot(const AnalysisSnapshot& snap) {
    ca::AnalysisSnapshot out;
    for (int i = 0; i < AnalysisSnapshot::kNumBands; ++i) {
        const auto& b = snap.bands[static_cast<size_t>(i)];
        out.bands[static_cast<size_t>(i)] = ca::BandStats{
            b.avgRmsDb, b.peakRmsDb, b.p10RmsDb, b.p50RmsDb, b.p95RmsDb, b.correlation, b.crestDb
        };
    }
    out.overallAvgDb  = snap.overallAvgDb;
    out.overallPeakDb = snap.overallPeakDb;
    out.overallCorr   = snap.overallCorr;
    out.lraLu         = snap.lraLu;
    return out;
}

ca::PresetData toCommonPreset(const PresetData& preset) {
    ca::PresetData out;
    out.name             = preset.name;
    out.description       = preset.description;
    out.bandRmsDb         = preset.bandRmsDb;
    out.bandMinCorr       = preset.bandMinCorr;
    out.bandTransientDb   = preset.bandTransientDb;
    out.overallRmsDb      = preset.overallRmsDb;
    out.overallMinCorr    = preset.overallMinCorr;
    return out;
}

AdviceSet fromCommonAdvice(const ca::AdviceSet& advice) {
    AdviceSet out;
    for (int i = 0; i < AdviceSet::kNumBands; ++i) {
        const auto is = static_cast<size_t>(i);
        out.eq[is]     = { advice.eq[is].gainDb, advice.eq[is].q, advice.eq[is].isShelf, advice.eq[is].freqHz };
        out.mbComp[is] = { advice.mbComp[is].thresholdDb, advice.mbComp[is].ratio,
                            advice.mbComp[is].attackMs, advice.mbComp[is].releaseMs };
        out.width[is]  = { advice.width[is].width };
    }
    out.mixbusComp = { advice.mixbusComp.thresholdDb, advice.mixbusComp.ratio,
                        advice.mixbusComp.attackMs, advice.mixbusComp.releaseMs, advice.mixbusComp.makeupDb };
    out.saturator  = { advice.saturator.driveDb };
    out.limiter    = { advice.limiter.targetLufsApprox, advice.limiter.ceilingDb };
    // advice.resonances is intentionally not copied -- Common's deriveAdvice()
    // never populates it either (see AdviceSet.h), matching this file's
    // existing behavior where resonances is set separately by pipeline.cpp.
    return out;
}

} // namespace

AdviceSet deriveAdvice(const AnalysisSnapshot& snap, const PresetData& preset) {
    const auto commonAdvice = ca::deriveAdvice(toCommonSnapshot(snap), toCommonPreset(preset));
    return fromCommonAdvice(commonAdvice);
}

} // namespace mt
```

This deletes the entire hand-rolled percentile/EQ/comp/width/mixbus/saturator/limiter formula block (the file's previous ~90 lines of computation) in favor of the conversion + single delegating call above.

- [ ] **Step 3: Rebuild and verify the baseline still passes**

Run: `cmake --build build --parallel && ctest --test-dir build -R "test_advice|test_lra" --output-on-failure`
Expected: PASS — Common's `deriveAdvice` is the direct port of this exact algorithm (percentile P50/P95 blend, LRA-aware limiter offset, same clamps/thresholds), so outputs are bit-for-bit identical for identical inputs.

- [ ] **Step 4: Commit**

```bash
git add core/src/advice.cpp
git commit -m "refactor(advice): delegate mt::deriveAdvice to AudioPluginsCommon::analysis::deriveAdvice"
```

---

### Task 9: Full-suite verification and cleanup

**Files:** none (verification only)

- [ ] **Step 1: Run the complete existing test suite one more time, from a clean build**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: every test in the suite passes (`test_sanity`, `test_dsp`, `test_analysis`, `test_preset`, `test_advice`, `test_lra`, `test_resonance_detector`, `test_codec_correction`, `test_target_level`, `test_pipeline`, `test_io`, `test_midi_mapping`, `test_gain_stager`, `test_preset_builder`, `test_similarity_service`) — this is the plan's overall regression gate.

- [ ] **Step 2: Build the GUI and CLI to confirm no downstream compile breakage**

```bash
cmake --build build --target mastertweak MasterTweak --parallel
```

Expected: both binaries build cleanly (GUI/CLI code never referenced any of the internals this plan touched, only the unchanged `mt::` public API, but this confirms it directly rather than assuming).

- [ ] **Step 3: Note what's explicitly out of scope**

`mt::formatAdviceMarkdown`/`core/src/report.cpp` is **not** touched by this plan. Common's ported `Report::formatAdviceMarkdown` dropped the resonance table's Gain/Active columns (Common's `ResonancePeak` has no `gainDb`/`enabled` — see Task 6), so delegating would silently change Codex's Markdown export's Resonance EQ section. No test covers `report.cpp` (`tests/` has no `test_report.cpp`), and the risk/reward of restructuring it to preserve those two columns isn't worth it for this plan. Record this as a known, deliberate gap — do not silently migrate it as a "bonus" task.

- [ ] **Step 4: Push and open a PR**

```bash
git push -u origin feat/consume-common-analysis
gh pr create --title "Consume AudioPluginsCommon dsp/analysis modules" --body "$(cat <<'EOF'
## Summary
- Phase 4 Stage A step 2 of the AudioPlugins JUCE→DPF migration roadmap: repoints Codex/MasterTweak's core/ at Common's new common/dsp + common/analysis modules (v0.5.0) instead of its own private, duplicated implementations.
- mt:: public API is unchanged everywhere; only internal implementations became aliases/thin wrappers over AudioPluginsCommon.
- One accepted behavior-adjacent change: detectResonances' background-level estimate now uses Common's O(1) prefix-sum algorithm (ported from TrueSight) instead of this file's old O(n) moving average -- existing test_resonance_detector.cpp assertions (already loosely bounded) still pass.
- report.cpp is explicitly left untouched (see plan's Task 9 note) -- Common's ported Report module dropped the resonance table's Gain/Active columns.

## Test plan
- [x] Full existing doctest suite passes (ctest --test-dir build)
- [x] CLI (mastertweak) and GUI (MasterTweak) build cleanly
EOF
)"
```

Report the PR URL back once created.
