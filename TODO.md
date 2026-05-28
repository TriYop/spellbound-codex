# Next features and evolutions to implement

- ~~Sort presets alphabetically~~ **DONE** — `enumeratePresets()` in `preset.cpp` sorts results case-insensitively before returning.

- Ensure optimal gain staging before/between each part of the mastering chain to ensure best audio treatment quality

- ~~Process initial gain staging before analysis to avoid +12dB on all EQ bands on quiet tracks instead of tracking real level adjustments.~~ **DONE** — `applyPreGain()` in `pipeline.cpp` normalizes to `preset.overallRmsDb` before `analyseFile()` in both `renderFile()` and `analyseOnly()`.


- Make the GUI sound-engineer friendly
  - use knobs / faders like buttons instead of spinnig inputs (still need to display precise value but easier to manipulate, still able to set a control value by double-clicking on value and typing it)
  - clearly separate each mastering chain component
  - EQ:
    - EQ level -> faders ; EQ Q filter -> rotary knob ; 
  - multiband compressor : 
    - per-band controls: rotary knobs
  - bus compressor :
    - all controls = rotary knobs
  - saturator: rotary knob
  - limiter : 
    - threashold -> fader
    - other controls -> rotary (if visibles)
 
  - add VU meter for playback
  
- ~~Add mastering setting to set overall target level (dBUFS + peak) depending on the usage. These are built-in presets based on industry standards.~~ **DONE** — `TargetLevelProfile` + `LufsAnalyser` (EBU R128) + LUFS normalisation in render pipeline + `TargetLevelCombo` GUI widget + `--target-level` / `--list-targets` CLI flags.

- Copy MixAdvice preset editor feature into MasterTweak. (The scanner itself should be distributed standalone). This would allow the user to create a specific tuned profile for the track he is mastering.

