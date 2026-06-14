#!/bin/bash
set -e

DOCS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$DOCS_DIR/src"
MOCKUP_DIR="$DOCS_DIR/mockups"
IMG_DIR="$MOCKUP_DIR/img"
OUT_DIR="$DOCS_DIR"

echo "=== Generating mockups ==="
python3 "$MOCKUP_DIR/generate_mockups.py" || exit 1

echo "=== Building PDFs ==="

# Mastering guide (French)
pandoc \
  --pdf-engine=xelatex \
  --toc \
  --toc-depth=2 \
  -V title="MasterTweak — Guide de Mastering" \
  -V author="MasterTweak Team" \
  -V lang=fr-FR \
  "$SRC_DIR/mastering_guide_fr.md" \
  -o "$OUT_DIR/MasterTweak_Guide_Mastering_FR.pdf" || exit 1
echo "✓ MasterTweak_Guide_Mastering_FR.pdf"

# Preset builder guide (French)
pandoc \
  --pdf-engine=xelatex \
  --toc \
  --toc-depth=2 \
  -V title="MasterTweak — Guide de Création de Presets" \
  -V author="MasterTweak Team" \
  -V lang=fr-FR \
  "$SRC_DIR/preset_builder_guide_fr.md" \
  -o "$OUT_DIR/MasterTweak_Guide_PresetBuilder_FR.pdf" || exit 1
echo "✓ MasterTweak_Guide_PresetBuilder_FR.pdf"

# Quick start (French)
pandoc \
  --pdf-engine=xelatex \
  -V title="MasterTweak — Démarrage Rapide" \
  -V author="MasterTweak Team" \
  -V lang=fr-FR \
  "$SRC_DIR/quickstart_fr.md" \
  -o "$OUT_DIR/MasterTweak_QuickStart_FR.pdf" || exit 1
echo "✓ MasterTweak_QuickStart_FR.pdf"

# Quick start (English)
pandoc \
  --pdf-engine=xelatex \
  -V title="MasterTweak — Quick Start" \
  -V author="MasterTweak Team" \
  -V lang=en-US \
  "$SRC_DIR/quickstart_en.md" \
  -o "$OUT_DIR/MasterTweak_QuickStart_EN.pdf" || exit 1
echo "✓ MasterTweak_QuickStart_EN.pdf"

echo "=== Build complete ==="
echo "PDFs saved to $OUT_DIR/"
