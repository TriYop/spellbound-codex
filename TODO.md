# Next features and evolutions to implement

- ~~Sort presets alphabetically~~ **DONE** — `enumeratePresets()` in `preset.cpp` sorts results case-insensitively before returning.

- ~~Ensure optimal gain staging before/between each part of the mastering chain to ensure best audio treatment quality~~ **DONE** — `dsp::GainStager` (restoreRms after multiband comp, trimPeak after mixbus comp); per-stage levels in `MasterResult::gainStages`.

- ~~Process initial gain staging before analysis to avoid +12dB on all EQ bands on quiet tracks instead of tracking real level adjustments.~~ **DONE** — `applyPreGain()` in `pipeline.cpp` normalizes to `preset.overallRmsDb` before `analyseFile()` in both `renderFile()` and `analyseOnly()`.


- ~~Make the GUI sound-engineer friendly~~ **DONE** — Section layout (ChainPanel: 6 bypassable sections), knob/fader widgets (RotaryKnob + VerticalFader), VU meter (stereo classic VU, 300 ms ballistics, integrated in TransportWidget).

- ~~Add mastering setting to set overall target level (dBUFS + peak) depending on the usage. These are built-in presets based on industry standards.~~ **DONE** — `TargetLevelProfile` + `LufsAnalyser` (EBU R128) + LUFS normalisation in render pipeline + `TargetLevelCombo` GUI widget + `--target-level` / `--list-targets` CLI flags.

- Copy MixAdvice preset editor feature into MasterTweak. (The scanner itself should be distributed standalone). This would allow the user to create a specific tuned profile for the track he is mastering.
  - ~~Sub-project A: `preset_builder_core` static lib (domain model, StatsService, ExportService, IngestService, SQLite adapters)~~ **DONE**
  - Sub-project B: Qt6 Preset Builder dialog (Ingest / Browse+Tag / Create Preset screens) — spec + plan pending.

- I would love to control my knobs, faders, transport and switches from my MIDI control surface (korg nanokontrol 2 ; behringer X-Touch Mini (in GM Mode)). Viewing the knobs values directly on the control surface would also be great
