#!/usr/bin/env python3
"""Generate all 17 interface mockups for MasterTweak documentation."""

import os
import sys
from PIL import Image, ImageDraw, ImageFont
import textwrap

MOCKUP_DIR = os.path.dirname(os.path.abspath(__file__))
IMG_DIR = os.path.join(MOCKUP_DIR, "img")
os.makedirs(IMG_DIR, exist_ok=True)

# Colors
BG_COLOR = (30, 30, 30)        # #1e1e1e dark
TEXT_COLOR = (220, 220, 220)   # light gray
ACCENT_COLOR = (42, 157, 143)  # teal
LABEL_COLOR = (255, 193, 7)    # amber/gold
BORDER_COLOR = (80, 80, 80)    # medium gray

def draw_header(draw, width, y, text):
    """Draw a section header."""
    draw.text((20, y), text, fill=ACCENT_COLOR, font=None)
    draw.line([(20, y + 25), (width - 20, y + 25)], fill=BORDER_COLOR, width=1)
    return y + 40

def draw_button(draw, x, y, width, height, text, enabled=True):
    """Draw a button."""
    color = ACCENT_COLOR if enabled else (100, 100, 100)
    draw.rectangle([(x, y), (x + width, y + height)], outline=color, width=2)
    draw.text((x + 10, y + 8), text, fill=color, font=None)

def draw_knob(draw, x, y, radius=30, label=""):
    """Draw a rotary knob."""
    draw.ellipse([(x - radius, y - radius), (x + radius, y + radius)],
                 outline=ACCENT_COLOR, width=2)
    draw.line([(x, y - radius + 5), (x, y - radius + 15)], fill=ACCENT_COLOR, width=2)
    if label:
        draw.text((x - 30, y + radius + 10), label, fill=TEXT_COLOR, font=None)

def draw_fader(draw, x, y, height=120, label=""):
    """Draw a vertical fader."""
    draw.rectangle([(x - 8, y), (x + 8, y + height)], outline=BORDER_COLOR, width=1, fill=(50, 50, 50))
    draw.rectangle([(x - 8, y + height // 2 - 4), (x + 8, y + height // 2 + 4)], fill=ACCENT_COLOR)
    if label:
        draw.text((x - 40, y + height + 10), label, fill=TEXT_COLOR, font=None)

def generate_main_window():
    """Generate annotated main window mockup."""
    width, height = 900, 800
    img = Image.new('RGB', (width, height), BG_COLOR)
    draw = ImageDraw.Draw(img)

    y = 20

    # Title bar
    draw.rectangle([(0, 0), (width, 40)], fill=(40, 40, 40))
    draw.text((20, 10), "MasterTweak", fill=ACCENT_COLOR, font=None)

    # Input file row
    y = 50
    draw.text((20, y), "1. Open audio...", fill=LABEL_COLOR, font=None)
    draw_button(draw, 140, y - 5, 120, 35, "Open audio")
    draw.text((280, y), "song.wav", fill=TEXT_COLOR, font=None)

    # Preset row
    y = 100
    draw.text((20, y), "2. Preset:", fill=LABEL_COLOR, font=None)
    draw.rectangle([(120, y - 5), (300, y + 35)], outline=ACCENT_COLOR, width=1, fill=(50, 50, 50))
    draw.text((130, y + 8), "Pop (bundled)", fill=TEXT_COLOR, font=None)

    # Render row
    y = 150
    draw_button(draw, 20, y, 100, 35, "Render")
    draw_button(draw, 130, y, 100, 35, "Save As...")
    draw_button(draw, 240, y, 140, 35, "Export Advice")
    draw.text((400, y + 10), "Bit depth: 24-bit  ☑ FLAC", fill=TEXT_COLOR, font=None)

    # Chain panel header
    y = 210
    draw.text((20, y), "DSP Chain", fill=ACCENT_COLOR, font=None)
    draw.line([(20, y + 25), (width - 20, y + 25)], fill=BORDER_COLOR, width=1)

    y += 40
    draw.text((20, y), "3. Eq Résonance  [✓] [✓] [ ]", fill=TEXT_COLOR, font=None)
    y += 35
    draw.text((20, y), "4. Eq Paramétrique  Sub: +2dB  Lows: -1dB  ...", fill=TEXT_COLOR, font=None)
    y += 35
    draw.text((20, y), "5. Multiband Comp  threshold, ratio per band", fill=TEXT_COLOR, font=None)
    y += 35
    draw.text((20, y), "6. Saturator  drive: 2.5dB", fill=TEXT_COLOR, font=None)
    y += 35
    draw.text((20, y), "7. Stereo Width  per-band correlation", fill=TEXT_COLOR, font=None)
    y += 35
    draw.text((20, y), "8. Mixbus Comp  thresh: -20dB, ratio: 4:1", fill=TEXT_COLOR, font=None)
    y += 35
    draw.text((20, y), "9. Limiter  ceiling: -1.0dBTP", fill=TEXT_COLOR, font=None)

    y += 50
    draw.text((20, y), "Transport", fill=ACCENT_COLOR, font=None)
    y += 30
    draw_button(draw, 20, y, 80, 30, "▶ Play")
    draw_button(draw, 110, y, 80, 30, "⏸ Stop")
    draw_button(draw, 200, y, 80, 30, "A/B")
    draw.text((300, y + 5), "Position: 1:23.5", fill=TEXT_COLOR, font=None)

    img.save(os.path.join(IMG_DIR, "mock_main_window.png"))
    print("✓ mock_main_window.png")

def generate_rack_units():
    """Generate individual RackUnit mockups."""
    width = 900

    # Resonance EQ
    img = Image.new('RGB', (width, 150), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Resonance EQ", fill=ACCENT_COLOR, font=None)
    draw.text((20, 45), "Detected resonances:", fill=TEXT_COLOR, font=None)
    draw.text((40, 65), "☑ 120 Hz (Q=8.5)  ☑ 2.4 kHz  ☑ 5.2 kHz", fill=TEXT_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_rack_resonance.png"))
    print("✓ mock_rack_resonance.png")

    # Parametric EQ
    img = Image.new('RGB', (width, 180), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Parametric EQ (7-Band)", fill=ACCENT_COLOR, font=None)
    y = 50
    labels = ["Sub", "Lows", "LowMid", "Mids", "HiMid", "Highs", "Air"]
    x_positions = [50, 150, 250, 350, 450, 550, 650]
    for i, (label, xpos) in enumerate(zip(labels, x_positions)):
        draw_knob(draw, xpos, y, radius=25, label=label)
        draw.text((xpos - 20, y + 50), f"+2.1dB", fill=LABEL_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_rack_eq.png"))
    print("✓ mock_rack_eq.png")

    # Multiband Compressor
    img = Image.new('RGB', (width, 200), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Multiband Compressor", fill=ACCENT_COLOR, font=None)
    y = 50
    draw.text((20, y), "Band", fill=TEXT_COLOR, font=None)
    draw.text((100, y), "Threshold", fill=TEXT_COLOR, font=None)
    draw.text((250, y), "Ratio", fill=TEXT_COLOR, font=None)
    y += 30
    for i, label in enumerate(["Sub", "Lows", "LowMid"]):
        draw.text((20, y), label, fill=TEXT_COLOR, font=None)
        draw.text((100, y), "-18dB", fill=TEXT_COLOR, font=None)
        draw.text((250, y), "3.2:1", fill=ACCENT_COLOR, font=None)
        y += 30
    img.save(os.path.join(IMG_DIR, "mock_rack_mb.png"))
    print("✓ mock_rack_mb.png")

    # Saturator
    img = Image.new('RGB', (width, 140), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Saturator", fill=ACCENT_COLOR, font=None)
    draw.text((20, 50), "Drive:", fill=TEXT_COLOR, font=None)
    draw_knob(draw, 150, 60, radius=25, label="Drive")
    draw.text((150 - 30, 100), "2.5 dB", fill=LABEL_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_rack_sat.png"))
    print("✓ mock_rack_sat.png")

    # Stereo Width
    img = Image.new('RGB', (width, 200), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Stereo Width (per-band)", fill=ACCENT_COLOR, font=None)
    y = 50
    for i, label in enumerate(["Sub", "Lows", "LowMid", "Mids"]):
        draw.text((20, y), label, fill=TEXT_COLOR, font=None)
        draw_fader(draw, 150, y - 10, height=80, label="")
        y += 40
    img.save(os.path.join(IMG_DIR, "mock_rack_width.png"))
    print("✓ mock_rack_width.png")

    # Mixbus Compressor
    img = Image.new('RGB', (width, 160), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Mixbus Compressor", fill=ACCENT_COLOR, font=None)
    y = 50
    draw.text((20, y), "Threshold:", fill=TEXT_COLOR, font=None)
    draw_knob(draw, 200, y + 15, radius=20)
    draw.text((200 - 30, y + 50), "-20dB", fill=LABEL_COLOR, font=None)
    draw.text((350, y), "Ratio:", fill=TEXT_COLOR, font=None)
    draw_knob(draw, 500, y + 15, radius=20)
    draw.text((500 - 30, y + 50), "4:1", fill=LABEL_COLOR, font=None)
    draw.text((650, y), "Makeup:", fill=TEXT_COLOR, font=None)
    draw_knob(draw, 800, y + 15, radius=20)
    draw.text((800 - 30, y + 50), "+8.5dB", fill=LABEL_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_rack_mixbus.png"))
    print("✓ mock_rack_mixbus.png")

    # Limiter
    img = Image.new('RGB', (width, 160), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Limiter (True-Peak, 4× OS)", fill=ACCENT_COLOR, font=None)
    y = 50
    draw.text((20, y), "Target LUFS:", fill=TEXT_COLOR, font=None)
    draw.text((200, y), "-14.0", fill=LABEL_COLOR, font=None)
    draw.text((20, y + 35), "Ceiling (dBTP):", fill=TEXT_COLOR, font=None)
    draw_fader(draw, 200, y + 20, height=60)
    draw.text((200 - 30, y + 85), "-1.0", fill=LABEL_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_rack_limiter.png"))
    print("✓ mock_rack_limiter.png")

def generate_transport_and_dialogs():
    """Generate transport and dialog mockups."""
    width = 900

    # Transport widget
    img = Image.new('RGB', (width, 120), BG_COLOR)
    draw = ImageDraw.Draw(img)
    y = 10
    draw_button(draw, 20, y, 80, 40, "▶ Play")
    draw_button(draw, 110, y, 80, 40, "⏸ Stop")
    draw_button(draw, 200, y, 80, 40, "A/B Compare")
    draw.text((300, y + 15), "Position: 1:23.5 / 3:45.2", fill=TEXT_COLOR, font=None)
    draw.text((20, 65), "VU Meter: L -6dB  R -6.5dB", fill=LABEL_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_transport.png"))
    print("✓ mock_transport.png")

    # MIDI Settings dialog
    img = Image.new('RGB', (500, 250), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.rectangle([(0, 0), (500, 250)], outline=ACCENT_COLOR, width=2)
    draw.text((20, 15), "MIDI Settings", fill=ACCENT_COLOR, font=None)
    y = 50
    draw.text((20, y), "Input Port:", fill=TEXT_COLOR, font=None)
    draw.rectangle([(200, y - 5), (450, y + 25)], outline=BORDER_COLOR, width=1)
    draw.text((210, y + 5), "nanoKONTROL2 In", fill=TEXT_COLOR, font=None)
    y += 50
    draw.text((20, y), "Output Port:", fill=TEXT_COLOR, font=None)
    draw.rectangle([(200, y - 5), (450, y + 25)], outline=BORDER_COLOR, width=1)
    draw.text((210, y + 5), "nanoKONTROL2 Out", fill=TEXT_COLOR, font=None)
    y += 60
    draw_button(draw, 200, y, 80, 35, "OK")
    draw_button(draw, 300, y, 100, 35, "Cancel")
    img.save(os.path.join(IMG_DIR, "mock_midi_dialog.png"))
    print("✓ mock_midi_dialog.png")

def generate_preset_builder_tabs():
    """Generate preset builder dialog tab mockups."""
    width = 900

    # Ingest tab
    img = Image.new('RGB', (width, 250), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Ingest Tab — Add Audio Files", fill=ACCENT_COLOR, font=None)
    draw_button(draw, 20, 45, 100, 35, "Add Folder")
    draw_button(draw, 130, 45, 100, 35, "Stop")
    y = 95
    draw.text((20, y), "Progress: 45/120 files", fill=TEXT_COLOR, font=None)
    draw.rectangle([(20, y + 25), (880, y + 35)], outline=BORDER_COLOR, width=1, fill=(60, 60, 60))
    draw.rectangle([(20, y + 25), (20 + (37.5), y + 35)], fill=ACCENT_COLOR)
    y += 50
    draw.text((20, y), "Added: 45  Skipped: 10  Failed: 2", fill=TEXT_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_pb_ingest.png"))
    print("✓ mock_pb_ingest.png")

    # Browse tab
    img = Image.new('RGB', (width, 300), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Browse Tab — Library Filter & Search", fill=ACCENT_COLOR, font=None)
    y = 45
    draw.text((20, y), "Title:", fill=TEXT_COLOR, font=None)
    draw.rectangle([(100, y - 5), (300, y + 25)], outline=BORDER_COLOR, width=1)
    draw.text((110, y + 5), "search...", fill=(100, 100, 100), font=None)
    y += 40
    draw.text((20, y), "Results: 23 tracks", fill=LABEL_COLOR, font=None)
    y += 35
    draw.text((20, y), "Title", fill=TEXT_COLOR, font=None)
    draw.text((200, y), "Artist", fill=TEXT_COLOR, font=None)
    draw.text((400, y), "Genre", fill=TEXT_COLOR, font=None)
    y += 25
    draw.text((20, y), "Song A", fill=TEXT_COLOR, font=None)
    draw.text((200, y), "Artist X", fill=TEXT_COLOR, font=None)
    draw.text((400, y), "Pop", fill=TEXT_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_pb_browse.png"))
    print("✓ mock_pb_browse.png")

    # Create Preset tab
    img = Image.new('RGB', (width, 320), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Create Preset Tab — Build from Tracks", fill=ACCENT_COLOR, font=None)
    y = 45
    draw.text((20, y), "Preset Name:", fill=TEXT_COLOR, font=None)
    draw.rectangle([(200, y - 5), (450, y + 25)], outline=BORDER_COLOR, width=1)
    draw.text((210, y + 5), "Pop - Bright", fill=TEXT_COLOR, font=None)
    y += 40
    draw.text((20, y), "Selected tracks: 15", fill=LABEL_COLOR, font=None)
    y += 35
    draw.text((20, y), "Statistics:", fill=ACCENT_COLOR, font=None)
    y += 25
    draw.text((20, y), "Sub: -28.2 dB  Lows: -22.1 dB  LowMid: -18.5 dB", fill=TEXT_COLOR, font=None)
    y += 20
    draw.text((20, y), "Mids: -16.2 dB  HiMid: -14.8 dB  Highs: -12.5 dB  Air: -8.3 dB", fill=TEXT_COLOR, font=None)
    y += 40
    draw_button(draw, 20, y, 120, 35, "Export")
    draw_button(draw, 150, y, 120, 35, "Save As...")
    img.save(os.path.join(IMG_DIR, "mock_pb_create.png"))
    print("✓ mock_pb_create.png")

    # Manage Presets tab
    img = Image.new('RGB', (width, 300), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "Manage Presets Tab — Remove Tracks / Delete", fill=ACCENT_COLOR, font=None)
    y = 50
    draw.text((20, y), "Presets:", fill=TEXT_COLOR, font=None)
    draw.rectangle([(20, y + 25), (250, y + 130)], outline=BORDER_COLOR, width=1, fill=(50, 50, 50))
    draw.text((30, y + 35), "Pop - Bright", fill=ACCENT_COLOR, font=None)
    draw.text((30, y + 60), "Rock - Heavy", fill=TEXT_COLOR, font=None)
    y += 155
    draw_button(draw, 20, y, 120, 35, "Delete Preset")
    img.save(os.path.join(IMG_DIR, "mock_pb_manage.png"))
    print("✓ mock_pb_manage.png")

def generate_diagrams():
    """Generate reference diagrams."""
    width = 900

    # DSP Flow diagram
    img = Image.new('RGB', (width, 400), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "DSP Signal Flow", fill=ACCENT_COLOR, font=None)

    stages = ["File (WAV)", "Analysis", "EQ Res", "EQ 7B", "MB Comp", "Saturator",
              "Width", "Mixbus", "Limiter", "Dither", "Output"]
    y = 80
    x_step = 75
    x_start = 30

    for i, stage in enumerate(stages):
        x = x_start + i * x_step
        draw.rectangle([(x - 25, y - 15), (x + 25, y + 15)], outline=ACCENT_COLOR, width=1)
        draw.text((x - 20, y - 5), stage if len(stage) < 8 else stage[:6], fill=TEXT_COLOR, font=None)
        if i < len(stages) - 1:
            draw.line([(x + 30, y), (x_start + (i + 1) * x_step - 30, y)], fill=BORDER_COLOR, width=1)

    draw.text((20, y + 80), "Parameters controlable per stage via GUI or CLI override", fill=TEXT_COLOR, font=None)
    draw.text((20, y + 110), "Bypass flags: eq, resonance, multiband, saturator, width, mixbuscomp, limiter, dither",
              fill=TEXT_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_dsp_flow.png"))
    print("✓ mock_dsp_flow.png")

    # CLI help
    img = Image.new('RGB', (width, 400), BG_COLOR)
    draw = ImageDraw.Draw(img)
    draw.text((20, 10), "CLI Usage Example", fill=ACCENT_COLOR, font=None)
    y = 50
    draw.text((20, y), "$ mastertweak song.wav --preset Pop --output song_master.wav", fill=LABEL_COLOR, font=None)
    y += 30
    draw.text((20, y), "$ mastertweak song.wav --preset Rock --bit-depth 16 --output-format flac", fill=LABEL_COLOR, font=None)
    y += 30
    draw.text((20, y), "$ mastertweak --batch '*.wav' --output-dir ./mastered --preset Pop", fill=LABEL_COLOR, font=None)
    y += 30
    draw.text((20, y), "$ mastertweak song.wav --preset Pop --analyze-only", fill=LABEL_COLOR, font=None)
    y += 30
    draw.text((20, y), "$ mastertweak --list-targets", fill=LABEL_COLOR, font=None)
    y += 50
    draw.text((20, y), "Override examples:", fill=ACCENT_COLOR, font=None)
    y += 25
    draw.text((20, y), "$ mastertweak --override-eq lows:-3,highs:+2", fill=LABEL_COLOR, font=None)
    y += 25
    draw.text((20, y), "$ mastertweak --override-limiter-ceiling -2.5", fill=LABEL_COLOR, font=None)
    img.save(os.path.join(IMG_DIR, "mock_cli_help.png"))
    print("✓ mock_cli_help.png")

def main():
    """Generate all mockups."""
    print("Generating MasterTweak documentation mockups...")

    try:
        generate_main_window()
        generate_rack_units()
        generate_transport_and_dialogs()
        generate_preset_builder_tabs()
        generate_diagrams()

        print(f"\n✓ All 17 mockups generated to {IMG_DIR}/")
        return 0
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
