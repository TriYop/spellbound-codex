# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MasterTweak is a standalone **offline auto-mastering** application:

- **Input:** WAV / FLAC / AIFF.
- **Profile:** a MixAdvice preset XML — one of the bundled genre presets, or a user XML produced externally by MixAdvice's Python `preset-builder` and dropped in `~/.config/MixAdvice/Presets/`.
- **Output:** mastered WAV / FLAC rendered offline at high quality (no real-time constraints during render).
- **Frontends:** Qt6 GUI + CLI, both driving the same `mastertweak_core` static lib.
- **Preview:** post-render playback via miniaudio (re-render on parameter change).

MasterTweak is **independent of MixAdvice as a codebase** — no shared code, no submodule, no JUCE. The only contract is MixAdvice's preset XML schema (`bandRmsDb[7]`, `bandMinCorr[7]`, `bandTransientDb[7]`, `overallRmsDb`, `overallMinCorr`, `name`, `description`). The 7-band layout (Sub, Lows, LowMids, Mids, HiMids, Highs, Air, crossovers at 80 / 250 / 500 / 2 k / 6 k / 16 k Hz) is reimplemented here from MixAdvice's `BandConfig.h`. The advice algorithm is reimplemented from `MixAdvice/Source/PluginEditor.cpp:537–665`.

## Build Commands

### Linux prerequisites (one-time)

```bash
sudo apt install cmake ninja-build build-essential git curl \
    libsndfile1-dev libtag-dev libsqlite3-dev qt6-base-dev pkg-config
```

### Configure / build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

CLI and GUI binaries land in `build/cli/mastertweak` and `build/gui/MasterTweak`.

### Tests

```bash
ctest --test-dir build --output-on-failure
# or
./build/tests/mastertweak_tests
```

### Headless build (CI / CLI-only environments)

```bash
cmake -B build -G Ninja -DMASTERTWEAK_BUILD_GUI=OFF
```

### Release tarball *(Phase 9, not yet wired)*

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack
```

## Architecture

### Layout

```
core/                     # static lib, pure C++20, no Qt, no JUCE
  include/mastertweak/
    io.hpp                # libsndfile wrapper: AudioFile, read/write WAV/FLAC/AIFF
    preset.hpp            # PresetData + XML loader (pugixml) matching MixAdvice schema
    analysis.hpp          # SevenBandAnalyser: LR cascade, per-band RMS/corr/crest
    advice.hpp            # AdviceSet + deriveAdvice(analysis, preset) → AdviceSet
    pipeline.hpp          # load → analyse → derive → render → save
    dsp/
      biquad.hpp          # cookbook biquads (bell, low/high shelf)
      linkwitz_riley.hpp  # 4th-order LR crossover
      parametric_eq.hpp   # 7-band cascaded biquads driven by AdviceSet
      compressor.hpp      # RMS-detector compressor
      multiband_comp.hpp  # split → 7× compressor → sum
      mixbus_comp.hpp
      saturator.hpp       # tanh / soft-clip
      stereo_width.hpp    # M/S decode → per-band width → encode
      limiter.hpp         # lookahead brickwall, true-peak via 4× oversampling
      dither.hpp          # TPDF dither for 16-bit output
  src/                    # implementations
cli/                      # CLI (CLI11)
gui/                      # Qt6 frontend
tests/                    # doctest unit tests
third_party/              # vendored single-header deps (pugixml 1.14, miniaudio 0.11.21,
                          #  CLI11 v2.4.2, doctest v2.4.11)
presets/                  # bundled MixAdvice preset XMLs (optional)
```

### Data flow

```
file → libsndfile (f64) → SevenBandAnalyser
                                ↓
                       {RMS, L/R corr, crest}[7]
                                ↓
                  deriveAdvice(analysis, preset) → AdviceSet
                                ↓                       ↑
                       (optional user overrides from GUI/CLI)
                                ↓
   ParametricEQ → MultibandComp → Saturator → StereoWidth → MixbusComp → Limiter → Dither
                                ↓
                       libsndfile write → out.wav
                                ↓
                       miniaudio plays out.wav (preview)
```

### AdviceSet algorithm

Ported from MixAdvice. Identical formulas so MasterTweak's `--analyze-only` output agrees with MixAdvice's Markdown export for the same input + preset:

- **Per-band EQ:** `gain = clamp(preset.bandRmsDb[i] − measured.avgRms[i], ±12 dB)`; zero if `|gain| < 0.5 dB`; `Q` scaled to gain magnitude (0.7–2.0); Sub & Air = shelves, others = bell.
- **Per-band multiband comp:** `threshold = preset.bandRmsDb[i] − 3 dB`; `ratio = clamp(1 + excess × 0.25, 1.1, 8.0)`; attack/release from `bandTransientDb[i]` and band index.
- **Mixbus comp:** same shape against `overallRmsDb`.
- **Saturator drive:** function of `(measuredCrest − preset.bandTransientDb)` averaged across bands; clamp [0, 6 dB].
- **Per-band stereo width:** drive width up where `bandMinCorr[i]` has headroom; pull in when correlation falls below the floor.
- **Limiter target:** `targetLufs ≈ preset.overallRmsDb + crest_offset`; ceiling = −1.0 dBTP.

### Key design constraints

- **No JUCE.** **No Python runtime dependency.** Single binary per frontend.
- **Internal sample format:** `double` for the limiter and multiband sum (headroom-critical); `float` elsewhere is fine.
- **Two-pass limiter:** lookahead requires the render pipeline to buffer ahead of the writer; structure the pipeline as a streaming consumer chain rather than per-block.
- **Preset XML compatibility:** MasterTweak's loader must accept the MixAdvice XML format unchanged so reference-built XMLs from `~/.config/MixAdvice/Presets/` work without conversion.
- **No real-time DSP variant in v1.** Preview = render-then-play. The DSP code can be written for offline simplicity (variable block size, double precision, allocations OK in `prepare()`).
