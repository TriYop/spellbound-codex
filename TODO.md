# Next features and evolutions to implement

- ~~Sort presets alphabetically~~ **DONE** — `enumeratePresets()` in `preset.cpp` sorts results case-insensitively before returning.

- ~~Ensure optimal gain staging before/between each part of the mastering chain to ensure best audio treatment quality~~ **DONE** — `dsp::GainStager` (restoreRms after multiband comp, trimPeak after mixbus comp); per-stage levels in `MasterResult::gainStages`.

- ~~Process initial gain staging before analysis to avoid +12dB on all EQ bands on quiet tracks instead of tracking real level adjustments.~~ **DONE** — `applyPreGain()` in `pipeline.cpp` normalizes to `preset.overallRmsDb` before `analyseFile()` in both `renderFile()` and `analyseOnly()`.


- ~~Make the GUI sound-engineer friendly~~ **DONE** — Section layout (ChainPanel: 6 bypassable sections), knob/fader widgets (RotaryKnob + VerticalFader), VU meter (stereo classic VU, 300 ms ballistics, integrated in TransportWidget).

- ~~Add mastering setting to set overall target level (dBUFS + peak) depending on the usage. These are built-in presets based on industry standards.~~ **DONE** — `TargetLevelProfile` + `LufsAnalyser` (EBU R128) + LUFS normalisation in render pipeline + `TargetLevelCombo` GUI widget + `--target-level` / `--list-targets` CLI flags.

- Copy MixAdvice preset editor feature into MasterTweak. (The scanner itself should be distributed standalone). This would allow the user to create a specific tuned profile for the track he is mastering.
  - ~~Sub-project A: `preset_builder_core` static lib (domain model, StatsService, ExportService, IngestService, SQLite adapters)~~ **DONE**
  - ~~Sub-project B: Qt6 Preset Builder dialog (Ingest / Browse+Tag / Create Preset screens)~~ **DONE**
  - ~~Sub-project C: MP3 / OGG ingest with FFT-based spectral rolloff correction~~ **DONE** — `computeCodecCorrection()` in `mastertweak_core`; Air (±6 dB), Highs (±1.5 dB), HiMids (±0.5 dB); baked into `bandRmsDb` before DB write.

- ~~Ingest files in parallel, up to the number of available CPU cores~~ **DONE** — `IngestService` thread pool (`std::thread::hardware_concurrency()` workers); hash/decode/analyse runs lock-free, SQLite and MetadataProvider serialised behind a single `std::mutex`.

- ~~I would love to control my knobs, faders, transport and switches from my MIDI control surface (korg nanokontrol 2 ; behringer X-Touch Mini (in GM Mode)). Viewing the knobs values directly on the control surface would also be great~~ **DONE** — `MidiController` (RtMidi 6.0.0/ALSA), `MidiMapping` (nanoKONTROL2 + X-Touch Mini GM defaults), bidirectional CC feedback, `MidiSettingsDialog` with QSettings persistence and startup auto-reconnect.

- Auto-discover similar tracks
  - a button named "auto-discover" (in the "Manage presets" window, on the "Create preset" panel) starts the similarity analysis.
  - similarity should be considered as the sum of all analysis parameters distances. Threshold to define a group must be defined by the user and should default to some relevant value according to the kind of data handled.
  - In an auto-discover session, each track should not be used in more than one preset but can belong to none.
  - tracks with very low distance should be grouped as a preset candidate. The user can refine the selection by removing tracks from selection, but not adding other tracks.
  - All discovered groups should be displayed in a list on the left of the window and
  - A relevant preset name based on track genre, artist, artist main genre and subgenre, and track year if available.
  - Once the preset info are set, it can be saved and the next preset is tried to be discovered. 
  - A cancel button on the window allows to close the auto-discover window and dismisses all unsaved groups.

- From presets manager > Browse/Tag, allow to play a track if it exists on mounted filesystem.
  - User selects a track in the list then pushes a play button on the same row as the "delete selected" one. The play button is at mid-width of the window.
  - If the selected track's file exists on mounted filesystem, then the play button is active ; otherwise, it is inactive

- From presets manager > Browse/Tag, a button allows to fetch track data from any available source such as ID3 tags (if relevant) or from online sources 

- On main window, the render row should be between the preset row and the advanced params override panel, leaving the transport panel at the bottom.

- ~~On the render row, a button should allow exporting the advices parameters to an markdown file in order to apply them in any other audio software such as individual DAW plugins and compare/preview/tweak the results.~~ **DONE** — "Export Advice…" button in the render row calls `mt::formatAdviceMarkdown()` (core) and saves a `.md` file with measured analysis values and all derived mastering parameters. 

