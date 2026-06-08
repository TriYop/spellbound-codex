# MasterTweak

Offline auto-mastering application for audio engineers. Analyses a track against a genre preset, derives a full mastering chain, and renders the result — all without a DAW.

Available as a command-line tool and a Qt6 desktop GUI. Both share the same `mastertweak_core` static library.

---

## Features

- **Preset-driven mastering** — presets encode genre-specific targets (RMS per band, transient character, stereo width, correlation floor). MasterTweak derives every DSP parameter automatically from the gap between measured analysis and preset targets.
- **8-stage DSP chain** — resonance EQ → broadband EQ → multiband compression → saturation → stereo width → mixbus compression → loudness normalisation → true-peak limiting → dither.
- **Platform loudness targets** — built-in profiles for Spotify, YouTube, Apple Music, Tidal, Amazon, CD, EBU R128, ATSC A/85.
- **Batch mode** — glob-based batch processing with a shared preset and output directory.
- **Advice export** — Markdown report of every derived parameter, usable as a reference in any DAW.
- **MIDI control surface** — GUI knobs and faders map to a Korg nanoKONTROL2 or Behringer X-Touch Mini (bidirectional CC feedback).
- **Preset Builder** — ingest a reference library, tag tracks, and create genre presets by statistical averaging. Supports MP3/OGG with FFT-based spectral rolloff correction.

---

## Quick Start

### Build dependencies (Debian/Ubuntu)

```bash
sudo apt install cmake ninja-build build-essential git curl \
    libsndfile1-dev qt6-base-dev libasound2-dev libtag1-dev
```

### Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Binaries: `build/cli/mastertweak` and `build/gui/MasterTweak`.

CLI-only (no Qt required):

```bash
cmake -B build -G Ninja -DMASTERTWEAK_BUILD_GUI=OFF
```

### Run

```bash
# Master a single file with a bundled preset
./build/cli/mastertweak track.wav --preset Rock

# Analyse only — print advice without rendering
./build/cli/mastertweak track.wav --preset Rock --analyze-only

# Target Spotify loudness (-14 LUFS / -1 dBTP)
./build/cli/mastertweak track.wav --preset Pop --target-level spotify

# Batch-process a folder
./build/cli/mastertweak --batch '*.wav' --preset Jazz --output-dir mastered/
```

---

## Audio Pipeline

The pipeline runs entirely offline in double precision where headroom matters. There are no real-time constraints; preview uses render-then-play via miniaudio.

```
Input file (WAV / FLAC / AIFF / MP3 / OGG)
        │
        ▼
  Pre-gain staging          normalise to preset's overall RMS target so the
                            analyser sees spectral imbalance, not loudness offset

        │
        ▼
  Seven-band analysis       LR4 cascaded crossovers at 80 / 250 / 500 / 2k / 6k / 16k Hz
                            per band: long-term RMS, peak-hold RMS, crest factor,
                            Pearson L/R correlation

        │
        ▼
  Advice derivation         pure function: (analysis, preset) → AdviceSet
                            all derived parameters; overrideable from GUI or CLI

        │
        ▼
  ┌─ DSP chain ────────────────────────────────────────────────────────────────┐
  │                                                                            │
  │  1. Resonance EQ        FFT-based detection of narrow resonances           │
  │                         (Q > 3, 80 Hz–16 kHz, up to 8 peaks);             │
  │                         narrow bell corrections, −3 to −12 dB             │
  │                                                                            │
  │  2. Broadband EQ        7-band parametric (Sub+Air: shelves;               │
  │                         Lows/Lo-Mid/Mids/Hi-Mid/Highs: bells);            │
  │                         gain = clamp(preset − measured, ±12 dB),          │
  │                         Q scaled to gain magnitude (0.7–2.0)              │
  │                                                                            │
  │  3. Multiband compressor 7 independent bands (same LR4 crossovers);       │
  │                         threshold = band target − 3 dB;                   │
  │                         ratio driven by excess energy; attack/release      │
  │                         derived from per-band transient target            │
  │     ↳ Gain staging      RMS restore to pre-chain reference level          │
  │                                                                            │
  │  4. Saturator           tanh soft-clip; drive = f(measured crest −        │
  │                         preset transient target), clamped 0–6 dB          │
  │                                                                            │
  │  5. Stereo width        M/S per band; widen where correlation headroom     │
  │                         exists, narrow where below the preset floor        │
  │                                                                            │
  │  6. Mixbus compressor   broadband RMS compressor; threshold and ratio      │
  │                         driven by overall excess; release tracks excess    │
  │     ↳ Gain staging      peak trim to −3 dBFS before limiter               │
  │                                                                            │
  │  7. LUFS normalisation  EBU R128 integrated measurement; trim to target   │
  │                         (optional, platform profile or manual value)       │
  │                                                                            │
  │  8. True-peak limiter   lookahead brickwall; 4× oversampling for          │
  │                         inter-sample peak detection; ceiling −1.0 dBTP    │
  │                                                                            │
  │  9. Dither              TPDF for 16-bit output; bypassed for 24/32-bit    │
  │                                                                            │
  └────────────────────────────────────────────────────────────────────────────┘
        │
        ▼
  Output file (WAV 16/24/32-bit or FLAC)
```

Every stage is independently bypassable (GUI checkbox or `--bypass <stage>`).

---

## CLI Reference

```
mastertweak [input] --preset <name|path> [options]
```

| Option | Description |
|---|---|
| `--preset, -p` | Preset name (bundled or in `~/.config/MixAdvice/Presets/`) or path to XML |
| `--output, -o` | Output file path (default: `<input>_master.wav`) |
| `--bit-depth` | 16, 24, or 32 (default: 24) |
| `--output-format` | `wav` or `flac` |
| `--target-level` | Platform loudness profile (see `--list-targets`) |
| `--list-targets` | Print built-in loudness profiles and exit |
| `--bypass` | Bypass one or more stages: `resonance eq multiband saturator width mixbuscomp limiter dither` |
| `--analyze-only` | Print analysis + advice, do not render |
| `--batch` | Glob pattern for batch processing (e.g. `'*.wav'`) |
| `--output-dir` | Destination for batch output |
| `--override-eq` | Per-band EQ override: `lows:-3,highs:+2` |
| `--override-limiter-ceiling` | True-peak ceiling in dBTP |
| `--override-sat-drive` | Saturator drive in dB (0–6) |
| `-v, --verbose` | Print per-stage details |

### Built-in loudness targets

| Platform | LUFS | Peak ceiling |
|---|---|---|
| Spotify / YouTube / Amazon / Tidal | −14 | −1.0 dBTP |
| Apple Music | −16 | −1.0 dBTP |
| CD / Download | −9 | −0.1 dBTP |
| Broadcast EBU R128 | −23 | −1.0 dBTP |
| Broadcast ATSC A/85 | −24 | −2.0 dBTP |

---

## Preset System

Presets are MixAdvice-compatible XML files encoding genre-specific targets:

```xml
<preset name="Rock" description="...">
  <bandRmsDb>    -18 -16 -20 -19 -22 -24 -30 </bandRmsDb>
  <bandMinCorr>   0.9  0.8  0.7  0.6  0.5  0.4  0.3 </bandMinCorr>
  <bandTransientDb> 6   8   10   12   14   16   18 </bandTransientDb>
  <overallRmsDb>-14</overallRmsDb>
  <overallMinCorr>0.7</overallMinCorr>
</preset>
```

Preset search order: `<binary dir>/presets/` → `~/.config/MixAdvice/Presets/`.

---

## Preset Builder (GUI)

The Preset Builder dialog (`Manage Presets` button) creates custom presets from a reference library:

1. **Ingest** — drag-and-drop folders; analyses each track (RMS, crest, correlation per band); MP3/OGG files get FFT-based spectral rolloff correction applied before storing. Parallel processing up to hardware concurrency.
2. **Browse & Tag** — filter by genre/artist, assign tags, play tracks (if file is available on disk), fetch metadata from AcoustID or embedded ID3 tags.
3. **Auto-Discover** — clusters the library by spectral similarity; proposes preset candidates with suggested names.
4. **Create Preset** — select a group of tracks; statistical averaging produces `bandRmsDb`, `bandTransientDb`, etc.; export to XML.

The preset builder stores data in `~/.config/MasterTweak/preset_builder.db` (SQLite), independent of MixAdvice.

---

## Architecture

```
mastertweak_core/          Pure C++20 static library. No Qt, no JUCE.
  include/mastertweak/
    io.hpp                 libsndfile + miniaudio wrapper (read WAV/FLAC/AIFF/MP3/OGG)
    preset.hpp             PresetData + XML loader (pugixml, MixAdvice-compatible schema)
    analysis.hpp           SevenBandAnalyser + ResonancePeak + detectResonances()
    advice.hpp             AdviceSet + deriveAdvice() — pure function
    pipeline.hpp           analyseOnly() / renderFile()
    report.hpp             formatAdviceMarkdown()
    target_level.hpp       TargetLevelProfile + built-in profiles
    dsp/
      fft.hpp              Radix-2 Cooley-Tukey FFT utility
      biquad.hpp           RBJ cookbook biquads (bell, shelf, LP, HP)
      linkwitz_riley.hpp   4th-order LR crossover
      parametric_eq.hpp    7-band cascaded EQ
      resonance_eq.hpp     Variable-band narrow-bell resonance corrector
      multiband_comp.hpp   7-band split → compress → sum
      compressor.hpp       RMS-detector compressor (attack/release IIR)
      mixbus_comp.hpp      Broadband mixbus compressor
      saturator.hpp        tanh soft-clip
      stereo_width.hpp     M/S per-band width
      limiter.hpp          Lookahead brickwall, 4× oversampling true-peak
      dither.hpp           TPDF dither
      lufs_analyser.hpp    EBU R128 K-weighted integrated LUFS
      gain_stager.hpp      RMS restore / peak trim utilities

cli/                       CLI11 frontend
gui/                       Qt6 frontend + MIDI controller + Preset Builder
preset_builder/            Hexagonal domain (ports & adapters): Track, Preset,
                           IngestService, StatsService, SimilarityService, ExportService
tests/                     doctest unit tests
third_party/               pugixml, miniaudio, CLI11, doctest, picosha2, RtMidi
presets/                   Bundled genre preset XMLs
```

---

## Tests

```bash
ctest --test-dir build --output-on-failure
# or directly:
./build/tests/mastertweak_tests
```
