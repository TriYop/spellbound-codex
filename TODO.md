# Next features and evolutions to implement

- ~~Sort presets alphabetically~~ **DONE** — `enumeratePresets()` in `preset.cpp` sorts results case-insensitively before returning.

- ~~Ensure optimal gain staging before/between each part of the mastering chain to ensure best audio treatment quality~~ **DONE** — `dsp::GainStager` (restoreRms after multiband comp, trimPeak after mixbus comp); per-stage levels in `MasterResult::gainStages`.

- ~~Process initial gain staging before analysis to avoid +12dB on all EQ bands on quiet tracks instead of tracking real level adjustments.~~ **DONE** — `applyPreGain()` in `pipeline.cpp` normalizes to `preset.overallRmsDb` before `analyseFile()` in both `renderFile()` and `analyseOnly()`.


- ~~Make the GUI sound-engineer friendly~~ **IN PROGRESS** — Section layout done (ChainPanel: 6 bypassable QGroupBox sections in a 3-tier grid, inline analysis readouts per section). Remaining: custom knob/fader widgets, VU meter.

- ~~Add mastering setting to set overall target level (dBUFS + peak) depending on the usage. These are built-in presets based on industry standards.~~ **DONE** — `TargetLevelProfile` + `LufsAnalyser` (EBU R128) + LUFS normalisation in render pipeline + `TargetLevelCombo` GUI widget + `--target-level` / `--list-targets` CLI flags.

- Copy MixAdvice preset editor feature into MasterTweak. (The scanner itself should be distributed standalone). This would allow the user to create a specific tuned profile for the track he is mastering.

