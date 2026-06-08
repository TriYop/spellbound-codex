# Lossy Format Ingest (MP3/OGG + Spectral Correction) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add MP3 and OGG support to the preset-builder ingest pipeline, with a codec-correction pass that boosts per-band RMS levels to compensate for high-frequency rolloff introduced by lossy encoding.

**Architecture:** A self-contained `computeCodecCorrection(const AudioFile&)` function in `mastertweak_core` uses a windowed-FFT approach (Cooley-Tukey, Hann window, 4096 pts, 50 % overlap) to detect HF rolloff and derive per-band corrections. The corrections — always non-negative — are added to `TrackAnalysis::bandRmsDb` immediately after analysis in `IngestService::ingest()`, before the track is written to the database. No new build dependencies: the FFT is implemented inline. libsndfile 1.2.2 (already linked) decodes both OGG and MP3 natively.

**Tech Stack:** C++20, libsndfile 1.2.2 (≥1.1.0 for MPEG/mpg123), doctest, CMake/Ninja.

---

## Band Index Reference

| Index | Band    | Frequency     | Max correction |
|-------|---------|---------------|----------------|
| 0     | Sub     | < 80 Hz       | 0 dB           |
| 1     | Lows    | 80–250 Hz     | 0 dB           |
| 2     | LowMids | 250–500 Hz    | 0 dB           |
| 3     | Mids    | 500–2000 Hz   | 0 dB           |
| 4     | HiMids  | 2–6 kHz       | +0.5 dB        |
| 5     | Highs   | 6–16 kHz      | +1.5 dB        |
| 6     | Air     | > 16 kHz      | +6 dB          |

## Algorithm Overview

1. Mono-sum all channels, apply Hann window, take 4096-point FFT. Repeat with 50 % hop, average magnitude spectra.
2. **Baseline:** mean dB in 8–12 kHz. If baseline < −50 dBFS → file has no significant HF content (e.g. pure bass recording) → return all-zeros (no correction).
3. **Air correction** (band 6): `clamp(baseline − air_db, 0, 6)` where `air_db` is the mean dB in 16–20 kHz (or up to Nyquist if lower).
4. **Highs correction** (band 5): `clamp((baseline − highs2_db) * 0.5, 0, 1.5)` where `highs2_db` is the mean dB in 12–16 kHz.
5. **HiMids correction** (band 4): `clamp(air_correction * 0.08, 0, 0.5)` — psychoacoustic smear proportional to Air correction.

---

## File Map

| Action | Path | Purpose |
|--------|------|---------|
| Modify | `core/include/mastertweak/io.hpp` | Add `SourceFormat` enum + `sourceFormat` field to `AudioFile` |
| Modify | `core/src/io.cpp` | Populate `sourceFormat` from `info.format & SF_FORMAT_TYPEMASK` after `sf_open` |
| Create | `core/include/mastertweak/codec_correction.hpp` | Declare `computeCodecCorrection(const AudioFile&) → std::array<float, 7>` |
| Create | `core/src/codec_correction.cpp` | Implement: inline FFT, windowed spectrum averaging, rolloff detection, corrections |
| Modify | `core/CMakeLists.txt` | Add `codec_correction.cpp` to `mastertweak_core` sources |
| Modify | `tests/CMakeLists.txt` | Add `test_codec_correction.cpp` |
| Create | `tests/test_codec_correction.cpp` | Tests for SourceFormat detection and `computeCodecCorrection` |
| Modify | `preset_builder/src/services/ingest_service.cpp` | Add `.mp3`/`.ogg` to `isAudioExtension`; call correction for lossy formats |
| Modify | `TODO.md` | Mark lossy ingest item done |

---

### Task 1: Add `SourceFormat` to `AudioFile`

**Files:**
- Modify: `core/include/mastertweak/io.hpp`
- Modify: `core/src/io.cpp`

- [ ] **Step 1: Update `core/include/mastertweak/io.hpp`**

Replace the existing content with:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mt {

enum class SourceFormat { wav, flac, aiff, ogg, mp3, unknown };

struct AudioFile {
    std::vector<std::vector<float>> samples;  // samples[channel][frame]
    int          numChannels  = 0;
    int          numFrames    = 0;
    int          sampleRate   = 0;
    int          bitDepth     = 0;
    SourceFormat sourceFormat = SourceFormat::unknown;
};

// Read WAV / FLAC / AIFF / OGG / MP3 (anything libsndfile supports).
// Returns nullopt on failure; fills errOut if provided.
std::optional<AudioFile> readAudioFile(const std::string& path,
                                       std::string* errOut = nullptr);

struct WriteOptions {
    int  bitDepth = 24;    // 16, 24, or 32
    bool flac     = false; // false = WAV, true = FLAC
};

// Write deinterleaved float audio to WAV or FLAC.
// Returns false on failure; fills errOut if provided.
bool writeAudioFile(const std::string& path,
                    const AudioFile& audio,
                    WriteOptions opts = {},
                    std::string* errOut = nullptr);

} // namespace mt
```

- [ ] **Step 2: Populate `sourceFormat` in `core/src/io.cpp`**

Inside `readAudioFile`, after the `sf_close(sf)` + `short-read` check and before `return out;`, add:

```cpp
    // Detect source format from libsndfile major format field
    const int major = info.format & SF_FORMAT_TYPEMASK;
    if      (major == SF_FORMAT_WAV   || major == SF_FORMAT_WAVEX) out.sourceFormat = SourceFormat::wav;
    else if (major == SF_FORMAT_AIFF)  out.sourceFormat = SourceFormat::aiff;
    else if (major == SF_FORMAT_FLAC)  out.sourceFormat = SourceFormat::flac;
    else if (major == SF_FORMAT_OGG)   out.sourceFormat = SourceFormat::ogg;
    else if (major == SF_FORMAT_MPEG)  out.sourceFormat = SourceFormat::mp3;
    else                               out.sourceFormat = SourceFormat::unknown;
```

The full updated `readAudioFile` body in `core/src/io.cpp`:

```cpp
std::optional<AudioFile> readAudioFile(const std::string& path, std::string* errOut) {
    SF_INFO info{};
    SNDFILE* sf = sf_open(path.c_str(), SFM_READ, &info);
    if (!sf) {
        if (errOut) *errOut = sf_strerror(nullptr);
        return std::nullopt;
    }

    AudioFile out;
    out.numChannels = info.channels;
    out.numFrames   = static_cast<int>(info.frames);
    out.sampleRate  = info.samplerate;
    const int subtype = info.format & SF_FORMAT_SUBMASK;
    if      (subtype == SF_FORMAT_PCM_16) out.bitDepth = 16;
    else if (subtype == SF_FORMAT_PCM_24) out.bitDepth = 24;
    else if (subtype == SF_FORMAT_PCM_32 || subtype == SF_FORMAT_FLOAT) out.bitDepth = 32;
    else                                  out.bitDepth = 24;

    const int totalSamples = out.numFrames * out.numChannels;
    std::vector<float> interleaved(static_cast<size_t>(totalSamples));
    const sf_count_t read = sf_read_float(sf, interleaved.data(), totalSamples);
    sf_close(sf);

    if (read != totalSamples) {
        if (errOut) *errOut = "Short read: expected " + std::to_string(totalSamples) +
                              " samples, got " + std::to_string(read);
        return std::nullopt;
    }

    out.samples.resize(static_cast<size_t>(out.numChannels),
                       std::vector<float>(static_cast<size_t>(out.numFrames)));
    for (int f = 0; f < out.numFrames; ++f)
        for (int c = 0; c < out.numChannels; ++c)
            out.samples[static_cast<size_t>(c)][static_cast<size_t>(f)] =
                interleaved[static_cast<size_t>(f * out.numChannels + c)];

    const int major = info.format & SF_FORMAT_TYPEMASK;
    if      (major == SF_FORMAT_WAV   || major == SF_FORMAT_WAVEX) out.sourceFormat = SourceFormat::wav;
    else if (major == SF_FORMAT_AIFF)  out.sourceFormat = SourceFormat::aiff;
    else if (major == SF_FORMAT_FLAC)  out.sourceFormat = SourceFormat::flac;
    else if (major == SF_FORMAT_OGG)   out.sourceFormat = SourceFormat::ogg;
    else if (major == SF_FORMAT_MPEG)  out.sourceFormat = SourceFormat::mp3;
    else                               out.sourceFormat = SourceFormat::unknown;

    return out;
}
```

- [ ] **Step 3: Build to verify it compiles**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add core/include/mastertweak/io.hpp core/src/io.cpp
git commit -m "feat: add SourceFormat enum to AudioFile (wav/flac/aiff/ogg/mp3)"
```

---

### Task 2: `codec_correction` header, stub, and CMakeLists

**Files:**
- Create: `core/include/mastertweak/codec_correction.hpp`
- Create: `core/src/codec_correction.cpp` (stub)
- Modify: `core/CMakeLists.txt`

- [ ] **Step 1: Write `core/include/mastertweak/codec_correction.hpp`**

```cpp
#pragma once

#include "mastertweak/io.hpp"

#include <array>

namespace mt {

// Estimate per-band RMS correction (dB) for a lossy-encoded audio file.
// Uses FFT-based spectral rolloff detection. Corrections are always non-negative
// (only adds energy, never subtracts). Returns all-zeros for lossless or
// silent/bass-only files where no correction is warranted.
//
// Band indices: Sub(0) Lows(1) LowMids(2) Mids(3) HiMids(4) Highs(5) Air(6)
// Max corrections:  0      0      0        0       +0.5      +1.5     +6  dB
std::array<float, 7> computeCodecCorrection(const AudioFile& audio);

} // namespace mt
```

- [ ] **Step 2: Create stub `core/src/codec_correction.cpp`**

```cpp
#include "mastertweak/codec_correction.hpp"

namespace mt {

std::array<float, 7> computeCodecCorrection(const AudioFile& /*audio*/) {
    return {};
}

} // namespace mt
```

- [ ] **Step 3: Add to `core/CMakeLists.txt`**

Add `src/codec_correction.cpp` to the `mastertweak_core` source list, after `src/dsp/gain_stager.cpp`:

```cmake
add_library(mastertweak_core STATIC
    src/version.cpp
    src/io.cpp
    src/analysis.cpp
    src/preset.cpp
    src/advice.cpp
    src/pipeline.cpp
    src/target_level.cpp
    src/codec_correction.cpp
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

- [ ] **Step 4: Build to verify stub compiles**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add core/include/mastertweak/codec_correction.hpp \
        core/src/codec_correction.cpp \
        core/CMakeLists.txt
git commit -m "feat: add codec_correction stub (computeCodecCorrection)"
```

---

### Task 3: Tests for `SourceFormat` detection and `computeCodecCorrection`

**Files:**
- Create: `tests/test_codec_correction.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `tests/test_codec_correction.cpp`**

```cpp
#include "mastertweak/codec_correction.hpp"
#include "mastertweak/io.hpp"

#include <doctest.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>

namespace fs = std::filesystem;

// ─── test helpers ─────────────────────────────────────────────────────────────

static mt::AudioFile makeMono(int sr, float durationSec) {
    mt::AudioFile af;
    af.sampleRate  = sr;
    af.numChannels = 1;
    af.numFrames   = static_cast<int>(durationSec * static_cast<float>(sr));
    af.bitDepth    = 24;
    af.samples.assign(1, std::vector<float>(static_cast<size_t>(af.numFrames), 0.f));
    return af;
}

static void addSine(mt::AudioFile& af, float freqHz, float amp = 0.1f) {
    for (int i = 0; i < af.numFrames; ++i)
        af.samples[0][static_cast<size_t>(i)] +=
            amp * std::sin(2.f * std::numbers::pi_v<float> * freqHz
                           * static_cast<float>(i) / static_cast<float>(af.sampleRate));
}

// Linear-congruential white noise in [-1, 1]
static float lcgSample(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.f;
}

// ─── SourceFormat detection ───────────────────────────────────────────────────

TEST_CASE("readAudioFile: WAV → SourceFormat::wav") {
    const auto path = (fs::temp_directory_path() / "mt_sf_wav.wav").string();
    auto af = makeMono(44100, 0.1f);
    af.samples[0][0] = 0.5f;
    REQUIRE(mt::writeAudioFile(path, af, {24, false}));

    std::string err;
    const auto loaded = mt::readAudioFile(path, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->sourceFormat == mt::SourceFormat::wav);
}

TEST_CASE("readAudioFile: FLAC → SourceFormat::flac") {
    const auto path = (fs::temp_directory_path() / "mt_sf_flac.flac").string();
    auto af = makeMono(44100, 0.1f);
    af.samples[0][0] = 0.1f;
    REQUIRE(mt::writeAudioFile(path, af, {24, true}));

    std::string err;
    const auto loaded = mt::readAudioFile(path, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->sourceFormat == mt::SourceFormat::flac);
}

// ─── computeCodecCorrection ───────────────────────────────────────────────────

TEST_CASE("computeCodecCorrection: silence → all-zero corrections") {
    // Silence has no HF baseline; baseline < -50 dBFS → early return
    auto af = makeMono(44100, 2.f);
    // samples already zeroed by makeMono
    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c == 0.f);
}

TEST_CASE("computeCodecCorrection: file shorter than FFT size → all-zero corrections") {
    // File of only 100 frames: too short to build a single 4096-pt window
    mt::AudioFile af;
    af.sampleRate  = 44100;
    af.numChannels = 1;
    af.numFrames   = 100;
    af.samples.assign(1, std::vector<float>(100, 0.5f));
    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c == 0.f);
}

TEST_CASE("computeCodecCorrection: bass-only signal → all-zero corrections") {
    // 440 Hz sine has no HF energy; baseline (8-12 kHz) will be < -50 dBFS
    auto af = makeMono(44100, 2.f);
    addSine(af, 440.f, 0.5f);
    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c == 0.f);
}

TEST_CASE("computeCodecCorrection: rolled-off spectrum → positive Air and Highs corrections") {
    // Multi-sine from 1 kHz to 12 kHz, nothing above 12 kHz.
    // Simulates an MP3 with aggressive rolloff above 12 kHz.
    auto af = makeMono(44100, 2.f);
    for (float f : {1000.f, 2000.f, 4000.f, 6000.f, 8000.f, 10000.f, 12000.f})
        addSine(af, f, 0.1f);

    const auto corr = mt::computeCodecCorrection(af);

    // Air (band 6): must be boosted — nothing above 12 kHz
    CHECK(corr[6] > 0.f);
    CHECK(corr[6] <= 6.f);   // clamped at max

    // Highs upper half (band 5): also boosted (nothing at 12–16 kHz)
    CHECK(corr[5] > 0.f);
    CHECK(corr[5] <= 1.5f);  // clamped at max

    // HiMids (band 4): small positive correction (proportional to Air)
    CHECK(corr[4] >= 0.f);
    CHECK(corr[4] <= 0.5f);  // clamped at max

    // Low bands (0–3): no correction
    for (size_t i = 0; i < 4; ++i)
        CHECK(corr[i] == 0.f);
}

TEST_CASE("computeCodecCorrection: rolled-off spectrum hits Air clamp at 6 dB") {
    // Only low-frequency and mid-frequency content; above 12 kHz: zero energy.
    // Baseline will be well above -50 dBFS; Air will be near -100 dBFS → max clamp.
    auto af = makeMono(44100, 3.f);
    for (float f : {1000.f, 3000.f, 6000.f, 9000.f, 12000.f})
        addSine(af, f, 0.2f);

    const auto corr = mt::computeCodecCorrection(af);
    CHECK(corr[6] == doctest::Approx(6.f).epsilon(0.01f));  // hits max clamp
}

TEST_CASE("computeCodecCorrection: white noise (flat spectrum) → small Air correction") {
    // Pseudorandom white noise → approximately flat spectrum → corrections ≈ 0
    mt::AudioFile af;
    af.sampleRate  = 44100;
    af.numChannels = 1;
    af.numFrames   = 44100 * 5;  // 5 s of noise for low spectral variance
    af.samples.assign(1, std::vector<float>(static_cast<size_t>(af.numFrames)));
    uint32_t seed = 0xDEADBEEFu;
    for (float& s : af.samples[0])
        s = lcgSample(seed) * 0.5f;  // keep within ±0.5

    const auto corr = mt::computeCodecCorrection(af);
    // White noise has similar energy at all frequencies → Air correction should be small
    CHECK(corr[6] < 2.f);   // at most 2 dB variance from flat
    CHECK(corr[5] < 1.5f);  // upper Highs: within clamp
    CHECK(corr[4] < 0.5f);  // HiMids: within clamp
    // Low bands: always 0
    for (size_t i = 0; i < 4; ++i)
        CHECK(corr[i] == 0.f);
}

TEST_CASE("computeCodecCorrection: corrections are always non-negative") {
    // Even if the Air band has MORE energy than baseline (e.g. HF-boosted signal),
    // correction must be clamped at 0 — we never subtract.
    auto af = makeMono(44100, 2.f);
    // Strong HF content — add sines above 16 kHz, weak content at 8-12 kHz
    addSine(af, 8000.f, 0.01f);
    addSine(af, 17000.f, 0.5f);
    addSine(af, 19000.f, 0.5f);

    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c >= 0.f);
}

TEST_CASE("computeCodecCorrection: stereo input → same result as mono of same content") {
    // Stereo mono-summing should not change the correction estimate materially.
    auto mono = makeMono(44100, 2.f);
    for (float f : {1000.f, 4000.f, 8000.f, 10000.f})
        addSine(mono, f, 0.1f);

    // Build stereo version with identical L and R
    mt::AudioFile stereo;
    stereo.sampleRate  = 44100;
    stereo.numChannels = 2;
    stereo.numFrames   = mono.numFrames;
    stereo.samples = {mono.samples[0], mono.samples[0]};

    const auto corrMono   = mt::computeCodecCorrection(mono);
    const auto corrStereo = mt::computeCodecCorrection(stereo);

    for (size_t i = 0; i < 7; ++i)
        CHECK(corrMono[i] == doctest::Approx(corrStereo[i]).epsilon(0.01f));
}
```

- [ ] **Step 2: Add `test_codec_correction.cpp` to `tests/CMakeLists.txt`**

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
    test_preset_builder.cpp
    test_codec_correction.cpp
)
```

- [ ] **Step 3: Build and verify the tests compile**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build. All test targets compile.

- [ ] **Step 4: Run tests — expect most `computeCodecCorrection` tests to FAIL (stub returns zeros)**

```bash
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: the two `SourceFormat` tests PASS, the `silence` / `short-file` / `bass-only` tests PASS (stub returns zeros = correct answer for these), but `rolled-off spectrum` and `corrections non-negative` tests will FAIL.

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add tests/test_codec_correction.cpp tests/CMakeLists.txt
git commit -m "test: add tests for SourceFormat detection and computeCodecCorrection"
```

---

### Task 4: Implement `computeCodecCorrection`

**Files:**
- Replace: `core/src/codec_correction.cpp`

- [ ] **Step 1: Replace the stub with the full implementation**

```cpp
#include "mastertweak/codec_correction.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace mt {

// ─── FFT helpers ──────────────────────────────────────────────────────────────

static constexpr size_t kFftN = 4096;

// In-place iterative Cooley-Tukey DIT FFT (N must be a power of 2).
static void inplaceFft(std::vector<float>& re, std::vector<float>& im) {
    const size_t N = re.size();
    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < N; ++i) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    // Butterfly stages
    for (size_t len = 2; len <= N; len <<= 1) {
        const float ang     = -2.f * std::numbers::pi_v<float> / static_cast<float>(len);
        const float wBaseRe = std::cos(ang);
        const float wBaseIm = std::sin(ang);
        for (size_t i = 0; i < N; i += len) {
            float wRe = 1.f, wIm = 0.f;
            for (size_t j = 0; j < len / 2; ++j) {
                const float uRe =  re[i + j];
                const float uIm =  im[i + j];
                const float vRe =  re[i + j + len/2] * wRe - im[i + j + len/2] * wIm;
                const float vIm =  re[i + j + len/2] * wIm + im[i + j + len/2] * wRe;
                re[i + j]          = uRe + vRe;
                im[i + j]          = uIm + vIm;
                re[i + j + len/2]  = uRe - vRe;
                im[i + j + len/2]  = uIm - vIm;
                const float nwRe = wRe * wBaseRe - wIm * wBaseIm;
                wIm = wRe * wBaseIm + wIm * wBaseRe;
                wRe = nwRe;
            }
        }
    }
}

// Hann-window a signal, run FFT, return magnitude for bins [0, N/2).
static std::vector<float> magnitudeSpectrum(const float* signal, size_t N) {
    std::vector<float> re(N), im(N, 0.f);
    for (size_t i = 0; i < N; ++i) {
        const float w = 0.5f * (1.f - std::cos(2.f * std::numbers::pi_v<float>
                                                * static_cast<float>(i)
                                                / static_cast<float>(N - 1)));
        re[i] = signal[i] * w;
    }
    inplaceFft(re, im);
    std::vector<float> mag(N / 2);
    for (size_t k = 0; k < N / 2; ++k)
        mag[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
    return mag;
}

// Mean dB across FFT bins spanning [loHz, hiHz].
static float bandDb(const std::vector<float>& mag, float sr, size_t N,
                    float loHz, float hiHz) {
    const float binWidth = sr / static_cast<float>(N);
    const size_t lo = static_cast<size_t>(std::max(1.f, loHz / binWidth));
    const size_t hi = std::min(N / 2 - 1, static_cast<size_t>(hiHz / binWidth));
    if (lo > hi) return -100.f;
    float sumSq = 0.f;
    for (size_t k = lo; k <= hi; ++k) sumSq += mag[k] * mag[k];
    const float rms = std::sqrt(sumSq / static_cast<float>(hi - lo + 1));
    return rms > 1e-7f ? 20.f * std::log10(rms) : -100.f;
}

// ─── public API ───────────────────────────────────────────────────────────────

std::array<float, 7> computeCodecCorrection(const AudioFile& audio) {
    std::array<float, 7> corr{};

    if (audio.numFrames < static_cast<int>(kFftN)) return corr;  // too short

    const float sr      = static_cast<float>(audio.sampleRate);
    const float nyquist = sr / 2.f;
    if (nyquist < 8000.f) return corr;  // sample rate too low to analyse HF region

    const int    nch  = audio.numChannels;
    const size_t hop  = kFftN / 2;  // 50 % overlap

    // Accumulate averaged magnitude spectrum over all Hann windows
    std::vector<float> avgMag(kFftN / 2, 0.f);
    int nWindows = 0;

    for (size_t start = 0;
         start + kFftN <= static_cast<size_t>(audio.numFrames);
         start += hop)
    {
        // Mono-sum frame
        std::vector<float> frame(kFftN);
        for (size_t i = 0; i < kFftN; ++i) {
            float mono = 0.f;
            for (int c = 0; c < nch; ++c)
                mono += audio.samples[static_cast<size_t>(c)][start + i];
            frame[i] = mono / static_cast<float>(nch);
        }

        const auto mag = magnitudeSpectrum(frame.data(), kFftN);
        for (size_t k = 0; k < kFftN / 2; ++k)
            avgMag[k] += mag[k];
        ++nWindows;
    }

    if (nWindows == 0) return corr;
    for (float& m : avgMag) m /= static_cast<float>(nWindows);

    // Baseline: mean dB in 8–12 kHz — typically unaffected by codec rolloff
    const float baselineDb = bandDb(avgMag, sr, kFftN, 8000.f, 12000.f);
    if (baselineDb < -50.f) return corr;  // no significant HF content → skip

    // Band 6 — Air (>16 kHz): full rolloff correction
    const float airDb   = bandDb(avgMag, sr, kFftN, 16000.f, std::min(20000.f, nyquist));
    const float airCorr = std::clamp(baselineDb - airDb, 0.f, 6.f);
    corr[6] = airCorr;

    // Band 5 — Highs upper half (12–16 kHz): half-weight correction
    const float hi2Db = bandDb(avgMag, sr, kFftN, 12000.f, 16000.f);
    corr[5] = std::clamp((baselineDb - hi2Db) * 0.5f, 0.f, 1.5f);

    // Band 4 — HiMids (2–6 kHz): psychoacoustic smear, proportional to Air correction
    corr[4] = std::clamp(airCorr * 0.08f, 0.f, 0.5f);

    return corr;
}

} // namespace mt
```

- [ ] **Step 2: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 3: Run tests — all must pass**

```bash
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass. The previously failing `rolled-off spectrum`, `clamp at 6 dB`, `non-negative`, and `stereo` tests now pass.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add core/src/codec_correction.cpp
git commit -m "feat: implement computeCodecCorrection (FFT-based HF rolloff detection)"
```

---

### Task 5: Wire into `IngestService` — add MP3/OGG support

**Files:**
- Modify: `preset_builder/src/services/ingest_service.cpp`

- [ ] **Step 1: Add `.mp3` / `.ogg` / `.oga` to `isAudioExtension`**

Replace the `isAudioExtension` function:

```cpp
static bool isAudioExtension(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".wav"  || ext == ".flac" || ext == ".aiff" || ext == ".aif"
        || ext == ".mp3"  || ext == ".ogg"  || ext == ".oga";
}
```

- [ ] **Step 2: Add `#include` for codec_correction and apply correction in `ingest()`**

At the top of `ingest_service.cpp`, add the codec_correction include after the existing includes:

```cpp
#include "mastertweak/codec_correction.hpp"
```

Inside `IngestService::ingest()`, replace:

```cpp
        const auto snap     = mt::analyseFile(*audio);
        const auto analysis = toTrackAnalysis(snap);
```

with:

```cpp
        const auto snap = mt::analyseFile(*audio);
        auto analysis   = toTrackAnalysis(snap);

        // Boost per-band RMS to compensate for lossy codec rolloff
        if (audio->sourceFormat == mt::SourceFormat::mp3 ||
            audio->sourceFormat == mt::SourceFormat::ogg) {
            const auto corr = mt::computeCodecCorrection(*audio);
            for (size_t i = 0; i < 7; ++i)
                analysis.bandRmsDb[i] += corr[i];
        }
```

- [ ] **Step 3: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 4: Run full test suite**

```bash
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (ingest changes are integration-level; no existing unit tests break).

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/src/services/ingest_service.cpp
git commit -m "feat: add MP3/OGG ingest with spectral rolloff correction"
```

---

### Task 6: Update TODO and final verification

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Add lossy ingest to TODO.md**

Add a new completed entry in `TODO.md` (e.g. below the Preset Builder sub-project B line):

```markdown
  - ~~Sub-project C: MP3 / OGG ingest with FFT-based spectral rolloff correction~~ **DONE** — `computeCodecCorrection()` in `mastertweak_core`; Air (±6 dB), Highs (±1.5 dB), HiMids (±0.5 dB); baked into `bandRmsDb` before DB write.
```

- [ ] **Step 2: Final full build and test run**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: clean build, all tests pass.

- [ ] **Step 3: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add TODO.md
git commit -m "docs: mark lossy format ingest (MP3/OGG + spectral correction) as done"
```
