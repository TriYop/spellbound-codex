# MasterTweak Quick Start Guide (English)

Welcome to MasterTweak! This guide walks you through the complete workflow: open an audio file, choose a preset, and export a mastered version in just 7 simple steps.

---

## Getting Started in 7 Steps

### Step 1: Launch MasterTweak

Double-click the MasterTweak application icon on your desktop or in your Applications folder.

On **Linux**, run from the terminal:
```bash
./MasterTweak
```

The main window appears with an empty project. You will see:
- **File browser** panel (left)
- **Preset selector** (top-left dropdown)
- **Parameter override panel** (right) with sliders for EQ, compression, saturation, etc.
- **Waveform display** (center, greyed out until you load audio)
- **Render button** (bottom-right, disabled until audio is loaded)

### Step 2: Open an Audio File

1. Click **Open File** or use **File → Open**.
2. Navigate to your WAV, FLAC, or AIFF file.
3. Click **Open**.

The file is loaded and analyzed automatically. You will see:
- Waveform displayed in the center panel
- **7-band frequency analysis** in the background (red spikes show measured RMS per band)
- **Analysis results** displayed below the waveform (RMS per band, loudness, dynamics, etc.)
- **Render button** becomes **enabled** (bright blue)

### Step 3: Choose a Preset

A preset is a recipe for mastering—it defines target loudness, EQ targets, compression shape, and dynamic character.

In the **Preset dropdown** (top-left), select one of the bundled presets:

| Preset | Use Case |
|--------|----------|
| **Hip-Hop / Rap** | Aggressive, punchy. Tight low mids, loud low end, strong limiting. |
| **Rock** | Balanced, transient-heavy. Clear highs, controlled lows. |
| **Electronic / EDM** | Bright, wide stereo. High-passed subs, airy top end. |
| **Pop** | Commercial, smooth. Warm mids, compressed, modern loudness. |
| **Jazz / Acoustic** | Open, dynamic. Minimal EQ, gentle compression. |
| **Classical** | Natural, reference. Minimal saturation, linear response. |
| **Podcast / Voice** | Clear, intelligible. Boost presence band (2–6 kHz), multiband comp on sibilants. |

**Tip:** Start with a preset that matches your genre. The analysis automatically feeds into the DSP chain, so you don't need to tweak anything—but you can if you want to.

### Step 4: Review Parameters (No Changes Required)

You are not required to change anything. Each parameter shows:
- **Current value** (slider position)
- **Default recommendation** (calculated from your audio + preset)
- **Range** (e.g., −12 to +12 dB)

If you want to listen to how the default advice sounds:
- Click **Preview** (or press Space)
- The audio renders in real-time and plays through your speakers
- Listen to the A/B comparison (see Step 6)

To customize (optional):
- Drag any slider to override the recommendation
- The preview re-renders when you release the slider
- You can always click **Reset to Defaults** to go back

### Step 5: Click Render

Once you are satisfied with the preview (or ready to accept the defaults), click the large **Render** button (bottom-right).

A progress dialog appears:
- "Rendering… 50% complete"
- Two-pass rendering: analyze + apply DSP chain
- This may take 10–30 seconds depending on file length and CPU

When done:
- **Render button** changes to **"Done! Ready to save"** (green)
- **Export button** becomes **enabled** (top-right)

### Step 6: Listen and Compare (A/B)

Before saving, you can A/B the before and after:

- **Waveform view** shows the **original (blue) vs. mastered (red)** side-by-side
- Click **A/B Toggle** (or press **T**) to switch between:
  - Playing the **original** file
  - Playing the **mastered** file (from the in-memory render buffer)
  - Each plays the first 10 seconds on repeat so you can listen closely

**What to listen for:**
- Bass is tighter, more present (but not muddy)
- Kick/snare/vocals sit well in the mix
- High end is bright but not harsh
- Overall loudness is consistent across tracks
- Dynamics are controlled without sounding compressed
- Stereo image is open and balanced

If you want to tweak and re-render, just adjust sliders and click **Render** again.

### Step 7: Save

When you are happy with the result:

1. Click **Export** (or **File → Export**)
2. Choose output settings (see below)
3. Pick a filename and location
4. Click **Save**

The mastered file is written to disk.

---

## Common Options

### Output Format

At the bottom of the export dialog, you will see:

- **Bit Depth:**
  - **16-bit** (CD quality, smallest file, used for distribution)
  - **24-bit** (studio quality, larger file, used for archiving)
  - **32-bit float** (mixing quality, rarely needed for final output)

- **Format:**
  - **WAV** (universal, uncompressed)
  - **FLAC** (lossless, 30–50% compression, recommended for archiving)

### Target Loudness Platform

Select the platform your mastered track will be delivered to. This controls the limiter ceiling:

| Platform | Target (LUFS) | Notes |
|----------|---------------|-------|
| **Spotify** | −14 LUFS | Normalized by algorithm; overkill to exceed this. |
| **Apple Music** | −16 LUFS | Legacy loudness standard. |
| **YouTube** | −13 LUFS | Video platform; slightly louder than Spotify. |
| **Streaming (Generic)** | −14 LUFS | Safe middle ground for all streamers. |
| **Broadcast** | −23 LUFS | Radio, TV, podcasts. Much quieter. |
| **CD / Reference** | −9 LUFS | Uncompressed, dynamic, for archival or audiophile playback. |

**How it works:** The limiter's ceiling moves to match your target platform. A higher target (e.g., −9 LUFS for CD) allows louder peaks; a lower target (e.g., −23 LUFS for broadcast) squashes dynamics more.

### Export Advice Button

Click **Export Advice** to save a detailed Markdown report of what MasterTweak did:
- Measured RMS, correlation, and crest factor per band
- EQ gains and Q values applied
- Compression ratios and thresholds
- Saturator drive
- Limiter settings
- Warnings (e.g., "High crest factor; consider more compression")

This is useful for:
- Learning what the DSP did
- Replicating settings in your DAW
- Troubleshooting (if something doesn't sound right)

---

## MIDI Control (Optional)

If you have a MIDI controller (keyboard, mixer, knobs), you can map physical faders to MasterTweak parameters.

### Setup

1. Open **Settings → MIDI**.
2. Select your MIDI device from the list (e.g., "Akai APC40", "Behringer FCB1010").
3. Click **Learn** next to each parameter.
4. Move the corresponding slider/knob on your hardware.
5. MasterTweak records the mapping.
6. Click **Save**.

### Mappings

Once set up, you can control in real-time while previewing:

| Parameter | Range | Use |
|-----------|-------|-----|
| EQ Gain (per band) | −12 to +12 dB | Tweak low-end punch, presence, air |
| Multiband Comp Threshold (per band) | −30 to 0 dB | Control when compression kicks in |
| Saturator Drive | 0 to 6 dB | Add harmonic character (tape, tube simulation) |
| Limiter Ceiling | −1 to −6 dBTP | Emergency loudness limit |
| Stereo Width | 0 to 200% | Narrow (mono) to wide (exaggerated) |

---

## Frequency Band Reference

MasterTweak uses a 7-band split with fixed crossovers. Learn what each band does:

| Band | Range | Crossovers | Purpose |
|------|-------|-----------|---------|
| **Sub** | 20–80 Hz | — | Deep bass, kick fundamentals, sub-bass rumble. |
| **Lows** | 80–250 Hz | 80 Hz | Kick body, bass body, warmth. |
| **LowMids** | 250–500 Hz | 250 Hz | Mud zone, bass clarity, guitar/vocal body. |
| **Mids** | 500 Hz–2 kHz | 500 Hz | Vocal presence, instrument clarity. |
| **HiMids** | 2–6 kHz | 2 kHz | Presence, sibilance, instrument edge. |
| **Highs** | 6–16 kHz | 6 kHz | Brightness, hi-hat, cymbals, air. |
| **Air** | 16 kHz+ | 16 kHz | Ultra-highs, sparkle, aliasing zone. |

**When to boost/cut:**
- Boost a band if it measures too quiet (low RMS) vs. the preset target
- Cut a band if it measures too loud or harsh
- Use narrow bell filters (not shelves) for surgical control
- Sub and Air often work better as shelves (broad, smooth)

---

## Advanced: Bypass Stages

In the CLI or via environment variables, you can disable entire DSP stages for troubleshooting or creative use:

```bash
./mastertweak --bypass-eq input.wav output.wav
./mastertweak --bypass-comp input.wav output.wav
./mastertweak --bypass-sat input.wav output.wav
./mastertweak --bypass-width input.wav output.wav
./mastertweak --bypass-limiter input.wav output.wav
```

**Use cases:**
- `--bypass-eq`: Hear unEQ'd response; debug if preset EQ is wrong.
- `--bypass-comp`: Hear uncompressed dynamics.
- `--bypass-sat`: Remove harmonic character (make it clean/sterile).
- `--bypass-width`: Collapse to mono (check mono compatibility).
- `--bypass-limiter`: Let audio clip to see raw DSP output (risky).

---

## CLI Mode: Batch Processing

For batch mastering or unattended renders, use the CLI:

```bash
mastertweak --input song1.wav --output song1-mastered.wav --preset "Pop"
mastertweak --input song2.flac --output song2-mastered.flac --preset "Hip-Hop / Rap"
```

**Common flags:**
- `--preset <name>` — Use a bundled preset
- `--output-format wav` or `flac` — Default is WAV
- `--bit-depth 16` or `24` — Default is 16
- `--target-platform spotify` — Loudness ceiling; default is "Streaming (Generic)"
- `--analyze-only` — Print analysis, skip render (useful for debugging)
- `--override-eq <band> <gain>` — Override EQ gain on a single band (0–6)
- `--override-sat-drive <db>` — Override saturator drive

**Example batch script:**
```bash
#!/bin/bash
for file in *.wav; do
  mastertweak --input "$file" --output "mastered/$file" --preset "Pop"
done
```

---

## Quick Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| **File won't open** | Unsupported format or corrupted file. | Confirm file is WAV, FLAC, or AIFF. Try converting with FFmpeg: `ffmpeg -i input.mp3 output.wav` |
| **No waveform appears** | File decoded but playback codec missing. | Check system audio settings; ensure PulseAudio or ALSA is working. |
| **Render is very slow** | Large file (>30 min) or slow CPU. | Expected for large files. Close other apps. Release version is faster than Debug. |
| **Output sounds distorted** | Limiter ceiling is too low for the content. | Raise target platform or manually increase limiter ceiling slider. |
| **Output is too quiet** | Loudness platform is too conservative. | Lower target platform (e.g., from "Broadcast" to "Streaming"). |
| **Crash on render** | Out of memory or corrupted file. | Free up RAM. Restart app. Try a smaller test file. |
| **A/B playback skips** | Audio device buffer underrun. | Increase buffer size in Settings → Audio. |

---

## Next Steps

This guide covers the essentials. For deeper dives:

- **Full User Manual** (`docs/user/manual_en.md`) — Detailed parameter explanations, DSP algorithm reference, preset-building workflow
- **Preset Builder Guide** (`docs/user/preset_builder_en.md`) — Create custom presets from scratch using MasterTweak's Python tools
- **CLI Reference** (`docs/user/cli_reference_en.md`) — All command-line flags and scripting examples
- **FAQ** (`docs/user/faq_en.md`) — Common questions and answers

Happy mastering!

---

**MasterTweak Version 1.0** — Standalone offline auto-mastering, driven by MixAdvice presets.  
For bug reports or feature requests, visit the project GitHub issues page.
