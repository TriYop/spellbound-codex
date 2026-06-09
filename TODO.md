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
  - **Clustering algorithm investigation:** evaluate a KNN (K-Nearest Neighbours) approach as the grouping engine. Each track's feature vector = `[bandRmsDb[7], bandTransientDb[7], bandMinCorr[7], overallRmsDb]` (21 dimensions). KNN builds a mutual-proximity graph; connected components where every node's K nearest neighbours are within a distance threshold form preset candidates. Compare against the simpler "sum-of-distances + manual threshold" baseline on the existing library before committing to one approach. Key open questions: choice of K, distance metric (Euclidean vs. cosine vs. weighted), and whether to normalise dimensions before comparison.
  - similarity should be considered as the sum of all analysis parameters distances. Threshold to define a group must be defined by the user and should default to some relevant value according to the kind of data handled.
  - In an auto-discover session, each track should not be used in more than one preset but can belong to none.
  - tracks with very low distance should be grouped as a preset candidate. The user can refine the selection by removing tracks from selection, but not adding other tracks.
  - All discovered groups should be displayed in a list on the left of the window and
  - A relevant preset name based on track genre, artist, artist main genre and subgenre, and track year if available.
  - Once the preset info are set, it can be saved and the next preset is tried to be discovered. 
  - A cancel button on the window allows to close the auto-discover window and dismisses all unsaved groups.

- ~~From presets manager > Browse/Tag, allow to play a track if it exists on mounted filesystem.~~ **DONE** — Play ▶ button added to the Browse/Tag toolbar (centred between Delete and track count). Enabled only when a selected track's file exists on disk (`QFileInfo::exists`). Clicking loads and plays via the `TransportWidget` embedded at the bottom of the Preset Builder dialog.

- From presets manager > Browse/Tag, a button allows to fetch track data from any available source such as ID3 tags (if relevant) or from online sources 

- On main window, the render row should be between the preset row and the advanced params override panel, leaving the transport panel at the bottom.

- ~~On the render row, a button should allow exporting the advices parameters to an markdown file in order to apply them in any other audio software such as individual DAW plugins and compare/preview/tweak the results.~~ **DONE** — "Export Advice…" button in the render row calls `mt::formatAdviceMarkdown()` (core) and saves a `.md` file with measured analysis values and all derived mastering parameters. 

- ~~Enrichir `BandStats` avec des descripteurs de distribution (P10 / P50 / P95 du RMS par bande) en complément de `avgRmsDb` et `peakRmsDb`. Cela permettrait un algorithme d'advice plus robuste sur les morceaux très dynamiques (alternance parties calmes / denses), où la moyenne est un indicateur moins stable que la médiane ou le percentile haut. Nécessite de stocker l'histogramme des blocs RMS par bande pendant l'analyse ou de les trier en fin de passe.~~ **DONE** — `p10RmsDb`, `p50RmsDb`, `p95RmsDb` added to `BandStats`; `analyseFile()` collects per-block RMS samples and sorts at end of pass; `deriveAdvice()` uses `(p50 + p95) * 0.5f` as the characteristic level.

- Ajouter le LRA (Loudness Range EBU R128) global à l'analyse. Le `LufsAnalyser` existant ne calcule que le LUFS intégré (blocs 400 ms, gate −10 LU) ; le LRA requiert des blocs de 3 s, gate −20 LU, et P95 − P10 de la distribution de loudness courte durée. Cet indicateur permettrait d'adapter le comportement du limiteur final selon la macro-dynamique du morceau : Pop hyperlimitée (LRA ~3 LU) vs musique de film (LRA ~20 LU). Touche : `LufsAnalyser`, `AnalysisSnapshot`, `TrackAnalysis`, colonne `lra` dans `track_analysis`, et câblage dans l'advice.

