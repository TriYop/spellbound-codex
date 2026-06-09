# BandStats Percentile Descriptors (P10/P50/P95) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add P10 / P50 / P95 of per-block RMS to `BandStats` and update `deriveAdvice()` to use a distribution-aware characteristic level, making EQ and compression advice more robust on dynamic tracks.

**Architecture:** During `analyseFile()`, raw block-level RMS values (L+R averaged, dBFS) are collected into a `std::vector<float>` per band. After the main loop the vectors are sorted and percentile indices are read. `deriveAdvice()` replaces its `(avgRmsDb + peakRmsDb) * 0.5f` blend with `(p50RmsDb + p95RmsDb) * 0.5f`. The advice test helper and individual test overrides are updated to set the new fields.

**Tech Stack:** C++20, doctest, libsndfile, CMake/Ninja.

---

## Files

| Action | Path | What changes |
|--------|------|--------------|
| Modify | `core/include/mastertweak/analysis.hpp` | Add `p10RmsDb`, `p50RmsDb`, `p95RmsDb` to `BandStats` |
| Modify | `core/src/analysis.cpp` | Move `toDb` lambda, collect block samples, compute percentiles |
| Modify | `core/src/advice.cpp` | Replace characteristic-level formula |
| Modify | `core/src/report.cpp` | Add P10/P50/P95 columns to analysis table |
| Modify | `tests/test_analysis.cpp` | Add dynamic-signal percentile test |
| Modify | `tests/test_advice.cpp` | Update `makeOnTargetSnapshot` and per-band overrides |

---

## Task 1: Add fields to `BandStats` and write a failing test

**Files:**
- Modify: `core/include/mastertweak/analysis.hpp:11-16`
- Modify: `tests/test_analysis.cpp` (append after line 93)

- [ ] **Step 1: Add the three new fields to `BandStats`**

  In `core/include/mastertweak/analysis.hpp`, replace the `BandStats` struct (lines 11-16):

  ```cpp
  struct BandStats {
      float avgRmsDb    = -100.f;  // long-term RMS over the whole file (dBFS)
      float peakRmsDb   = -100.f;  // peak-hold of the 100ms-smoothed RMS (dBFS)
      float p10RmsDb    = -100.f;  // 10th-percentile of per-block RMS (dBFS)
      float p50RmsDb    = -100.f;  // median of per-block RMS (dBFS)
      float p95RmsDb    = -100.f;  // 95th-percentile of per-block RMS (dBFS)
      float correlation =    1.f;  // integrated Pearson L/R correlation [-1, 1]
      float crestDb     =    0.f;  // average crest factor (peak/RMS) in dB
  };
  ```

- [ ] **Step 2: Write a failing percentile test**

  Append to `tests/test_analysis.cpp` (after the last `}` on line 93):

  ```cpp
  TEST_CASE("percentile descriptors: P10 in quiet zone, P95 in loud zone, P10 < P50 < P95") {
      // First half of signal is loud (0.3 amp ≈ -13.5 dBFS sine RMS).
      // Second half is quiet (0.03 amp ≈ -33.5 dBFS sine RMS).
      // 1 kHz lands in the Mids band (index 3, crossovers 500 Hz – 2 kHz).
      const int   sr        = 44100;
      const float durSec    = 4.f;
      const int   numFrames = static_cast<int>(static_cast<float>(sr) * durSec);
      const int   halfFrames = numFrames / 2;

      mt::AudioFile f;
      f.sampleRate  = sr;
      f.numChannels = 2;
      f.numFrames   = numFrames;
      f.bitDepth    = 24;
      f.samples.resize(2, std::vector<float>(static_cast<size_t>(numFrames)));
      for (int i = 0; i < numFrames; ++i) {
          const float amp = (i < halfFrames) ? 0.3f : 0.03f;
          const float s = amp * std::sin(2.f * std::numbers::pi_v<float> * 1000.f
                                          * static_cast<float>(i) / static_cast<float>(sr));
          f.samples[0][static_cast<size_t>(i)] = s;
          f.samples[1][static_cast<size_t>(i)] = s;
      }

      const auto snap = mt::analyseFile(f);
      const auto& b   = snap.bands[3];  // Mids band

      // Ordering invariants
      CHECK(b.p10RmsDb <= b.p50RmsDb);
      CHECK(b.p50RmsDb <= b.p95RmsDb);
      // Loud section (amp 0.3, ~-13.5 dBFS) dominates P95; allow ±5 dB for filter leakage
      CHECK(b.p95RmsDb > -20.f);
      // Quiet section (amp 0.03, ~-33.5 dBFS) is captured by P10
      CHECK(b.p10RmsDb < -25.f);
      // At least 10 dB separates the two extremes
      CHECK(b.p10RmsDb < b.p95RmsDb - 10.f);
  }
  ```

- [ ] **Step 3: Build and verify the test fails**

  ```bash
  cd /home/yvan/Projects/AudioPlugins/MasterTweak
  cmake --build build --parallel 2>&1 | tail -5
  ./build/tests/mastertweak_tests --test-case="percentile*"
  ```

  Expected: the test case runs but fails — `p10RmsDb`, `p50RmsDb`, `p95RmsDb` are all `-100.f` (their default), so `b.p95RmsDb > -20.f` → `-100 > -20` → FAIL.

---

## Task 2: Implement block collection and percentile computation in `analyseFile()`

**Files:**
- Modify: `core/src/analysis.cpp`

- [ ] **Step 1: Move `toDb` before the main loop**

  Currently `toDb` is defined after the main `for` loop (around line 194). Move it to just before the `for (int blockStart ...)` loop — after the working-buffer declarations and before the loop. The lambda has an empty capture (`[]`) so this is safe.

  Find this block (around line 114):
  ```cpp
  // Working buffers for one band (per-channel)
  std::vector<float> remainder0(static_cast<size_t>(kBlockSize));
  std::vector<float> remainder1(static_cast<size_t>(kBlockSize));
  std::vector<float> band0(static_cast<size_t>(kBlockSize));
  std::vector<float> band1(static_cast<size_t>(kBlockSize));

  const int numFrames = audio.numFrames;
  ```

  Add `toDb` and `bandRmsSamples` immediately after the working buffers (before `const int numFrames`):

  ```cpp
  auto toDb = [](float lin) -> float {
      return lin > 1e-7f ? 20.f * std::log10(lin) : -100.f;
  };

  std::array<std::vector<float>, kNumBands> bandRmsSamples;

  const int numFrames = audio.numFrames;
  ```

- [ ] **Step 2: Remove the now-duplicate `toDb` definition from the finalisation block**

  After the main loop, the finalisation block currently starts with:
  ```cpp
  // Convert accumulators → final snapshot values
  auto toDb      = [](float lin) { return lin > 1e-7f ? 20.f * std::log10(lin) : -100.f; };
  auto toCrestDb = [](float r)   { return r > 1.f    ? 20.f * std::log10(r)    :   0.f; };
  ```

  Delete only the `auto toDb = ...` line (keep `toCrestDb`):
  ```cpp
  // Convert accumulators → final snapshot values
  auto toCrestDb = [](float r) -> float { return r > 1.f ? 20.f * std::log10(r) : 0.f; };
  ```

- [ ] **Step 3: Collect per-block RMS inside `storeBand`**

  Inside the `storeBand` lambda (which starts with `auto storeBand = [&](...)`), after the line:
  ```cpp
  intBandSumR2[bandIdx] += static_cast<double>(rmsR) * rmsR;
  ```
  Add:
  ```cpp
  bandRmsSamples[bandIdx].push_back((toDb(rmsL) + toDb(rmsR)) * 0.5f);
  ```

- [ ] **Step 4: Compute percentiles in the finalisation loop**

  Inside the existing per-band finalisation loop (which sets `avgRmsDb`, `peakRmsDb`, `correlation`, `crestDb`), add the percentile block after the existing field assignments:

  ```cpp
  snap.bands[i].avgRmsDb    = avgRms;
  snap.bands[i].peakRmsDb   = peakRms;
  snap.bands[i].correlation = smoothCorrBand[i];
  snap.bands[i].crestDb     = avgCrest;

  // Percentile descriptors — sort collected block-RMS samples and read indices
  {
      auto& v = bandRmsSamples[i];
      std::sort(v.begin(), v.end());
      auto pct = [&](float p) -> float {
          if (v.empty()) return -100.f;
          const auto idx = static_cast<size_t>(
              std::clamp(static_cast<int>(std::floor(p * static_cast<float>(v.size()))),
                         0, static_cast<int>(v.size()) - 1));
          return v[idx];
      };
      snap.bands[i].p10RmsDb = pct(0.10f);
      snap.bands[i].p50RmsDb = pct(0.50f);
      snap.bands[i].p95RmsDb = pct(0.95f);
  }
  ```

- [ ] **Step 5: Build and verify all tests pass (including the new one)**

  ```bash
  cmake --build build --parallel 2>&1 | tail -5
  ./build/tests/mastertweak_tests
  ```

  Expected: all tests pass, including the new `percentile descriptors` case.

- [ ] **Step 6: Commit**

  ```bash
  git add core/include/mastertweak/analysis.hpp core/src/analysis.cpp tests/test_analysis.cpp
  git commit -m "feat: add P10/P50/P95 per-band RMS percentiles to BandStats"
  ```

---

## Task 3: Update `deriveAdvice()` and fix the advice tests

**Files:**
- Modify: `core/src/advice.cpp:22`
- Modify: `tests/test_advice.cpp`

The advice tests use `makeOnTargetSnapshot()` which sets `avgRmsDb` and `peakRmsDb` but not the new percentile fields. After switching `deriveAdvice()` to use `p50RmsDb` and `p95RmsDb`, those fields default to `-100.f` and all advice tests break. Both the helper and the per-test band overrides must be updated.

- [ ] **Step 1: Change the characteristic-level formula in `advice.cpp`**

  In `core/src/advice.cpp`, replace line 22:
  ```cpp
  // Before
  const float refDb = (snap.bands[bi].avgRmsDb + snap.bands[bi].peakRmsDb) * 0.5f;
  ```
  With:
  ```cpp
  const float refDb = (snap.bands[bi].p50RmsDb + snap.bands[bi].p95RmsDb) * 0.5f;
  ```

- [ ] **Step 2: Update `makeOnTargetSnapshot` in `test_advice.cpp`**

  The helper currently sets `avgRmsDb` and `peakRmsDb`. Add `p50RmsDb` and `p95RmsDb` assignments. Replace the body of the helper (lines 9-17) with:

  ```cpp
  static mt::AnalysisSnapshot makeOnTargetSnapshot(const mt::PresetData& preset) {
      mt::AnalysisSnapshot snap;
      for (size_t i = 0; i < static_cast<size_t>(mt::AnalysisSnapshot::kNumBands); ++i) {
          snap.bands[i].avgRmsDb   = preset.bandRmsDb[i];
          snap.bands[i].peakRmsDb  = preset.bandRmsDb[i];
          snap.bands[i].p50RmsDb   = preset.bandRmsDb[i];
          snap.bands[i].p95RmsDb   = preset.bandRmsDb[i];
          snap.bands[i].correlation = preset.bandMinCorr[i];
          snap.bands[i].crestDb    = preset.bandTransientDb[i];
      }
      snap.overallAvgDb  = preset.overallRmsDb;
      snap.overallPeakDb = preset.overallRmsDb;
      snap.overallCorr   = preset.overallMinCorr;
      return snap;
  }
  ```

- [ ] **Step 3: Update per-band overrides in individual test cases**

  Four test cases directly override band fields. Each must also set `p50RmsDb` and `p95RmsDb` to the same value.

  **"+6 dB excess in one band"** (currently lines 50-51):
  ```cpp
  snap.bands[3].avgRmsDb  = -14.f;
  snap.bands[3].peakRmsDb = -14.f;
  snap.bands[3].p50RmsDb  = -14.f;
  snap.bands[3].p95RmsDb  = -14.f;
  ```

  **"deficit < 0.5 dB"** (currently lines 63-64):
  ```cpp
  snap.bands[2].avgRmsDb  = -20.3f;
  snap.bands[2].peakRmsDb = -20.3f;
  snap.bands[2].p50RmsDb  = -20.3f;
  snap.bands[2].p95RmsDb  = -20.3f;
  ```

  **"excess level → multiband comp ratio increases"** (currently lines 81-82):
  ```cpp
  snap.bands[1].avgRmsDb  = -12.f;
  snap.bands[1].peakRmsDb = -12.f;
  snap.bands[1].p50RmsDb  = -12.f;
  snap.bands[1].p95RmsDb  = -12.f;
  ```

  **"Sub and Air EQ bands are marked as shelves"** (currently lines 101-104):
  ```cpp
  snap2.bands[0].avgRmsDb  = -14.f;
  snap2.bands[0].peakRmsDb = -14.f;
  snap2.bands[0].p50RmsDb  = -14.f;
  snap2.bands[0].p95RmsDb  = -14.f;
  snap2.bands[6].avgRmsDb  = -14.f;
  snap2.bands[6].peakRmsDb = -14.f;
  snap2.bands[6].p50RmsDb  = -14.f;
  snap2.bands[6].p95RmsDb  = -14.f;
  ```

- [ ] **Step 4: Build and verify all advice tests pass**

  ```bash
  cmake --build build --parallel 2>&1 | tail -5
  ./build/tests/mastertweak_tests --test-case="*advice*" --test-case="*EQ*" --test-case="*multiband*" --test-case="*limiter*" --test-case="*mixbus*"
  ```

  Expected: all test cases pass.

- [ ] **Step 5: Commit**

  ```bash
  git add core/src/advice.cpp tests/test_advice.cpp
  git commit -m "feat: derive advice from P50/P95 instead of avg/peak RMS blend"
  ```

---

## Task 4: Update the markdown export report and final verification

**Files:**
- Modify: `core/src/report.cpp:60-67`

- [ ] **Step 1: Extend the analysis table header**

  In `core/src/report.cpp`, replace the two header lines (currently lines 60-61):
  ```cpp
  md << "| Band    | RMS (dBFS) | Crest (dB) | L/R Corr |\n";
  md << "| ------- | ---------- | ---------- | -------- |\n";
  ```
  With:
  ```cpp
  md << "| Band    | Avg (dBFS) | P10 (dBFS) | P50 (dBFS) | P95 (dBFS) | Crest (dB) | L/R Corr |\n";
  md << "| ------- | ---------- | ---------- | ---------- | ---------- | ---------- | -------- |\n";
  ```

- [ ] **Step 2: Extend the data rows**

  Replace the loop body (currently lines 62-67):
  ```cpp
  for (int i = 0; i < AnalysisSnapshot::kNumBands; ++i) {
      const auto& b = snap.bands[i];
      md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
         << " | " << std::right << std::setw(10) << fmtDb(b.avgRmsDb)
         << " | " << std::setw(10) << fmtF(b.crestDb)
         << " | " << std::setw(8)  << fmtF(b.correlation, 2) << " |\n";
  }
  ```
  With:
  ```cpp
  for (int i = 0; i < AnalysisSnapshot::kNumBands; ++i) {
      const auto& b = snap.bands[i];
      md << "| " << std::left  << std::setw(7) << AnalysisSnapshot::kBandNames[i]
         << " | " << std::right << std::setw(10) << fmtDb(b.avgRmsDb)
         << " | " << std::setw(10) << fmtDb(b.p10RmsDb)
         << " | " << std::setw(10) << fmtDb(b.p50RmsDb)
         << " | " << std::setw(10) << fmtDb(b.p95RmsDb)
         << " | " << std::setw(10) << fmtF(b.crestDb)
         << " | " << std::setw(8)  << fmtF(b.correlation, 2) << " |\n";
  }
  ```

- [ ] **Step 3: Run the full test suite**

  ```bash
  cmake --build build --parallel 2>&1 | tail -5
  ctest --test-dir build --output-on-failure
  ```

  Expected: all tests pass (currently 35; the new percentile test brings it to 36).

- [ ] **Step 4: Mark TODO item as done**

  In `TODO.md`, wrap the `BandStats` percentile item in strikethrough and add `**DONE**`:

  ```markdown
  - ~~Enrichir `BandStats` avec des descripteurs de distribution (P10 / P50 / P95 du RMS par bande) en complément de `avgRmsDb` et `peakRmsDb`. Cela permettrait un algorithme d'advice plus robuste sur les morceaux très dynamiques (alternance parties calmes / denses), où la moyenne est un indicateur moins stable que la médiane ou le percentile haut. Nécessite de stocker l'histogramme des blocs RMS par bande pendant l'analyse ou de les trier en fin de passe.~~ **DONE** — `p10RmsDb`, `p50RmsDb`, `p95RmsDb` added to `BandStats`; `analyseFile()` collects per-block RMS samples and sorts at end of pass; `deriveAdvice()` uses `(p50 + p95) * 0.5f` as the characteristic level.
  ```

- [ ] **Step 5: Final commit**

  ```bash
  git add core/src/report.cpp TODO.md
  git commit -m "feat: add P10/P50/P95 to advice report table; mark TODO done"
  ```
