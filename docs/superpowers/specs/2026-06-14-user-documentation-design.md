---
name: user-documentation-design
description: Design spec for MasterTweak complete user documentation — 4 PDF files (mastering guide FR, preset builder guide FR, quick start FR+EN)
metadata:
  type: project
---

# MasterTweak — User Documentation Design Spec

## Context

MasterTweak is a standalone offline auto-mastering application with:
- A Qt6 GUI (main window + preset builder dialog)
- A CLI (`mastertweak`)
- Core DSP engine (7-band EQ, multiband comp, saturator, stereo width, mixbus comp, limiter, dither)
- A preset builder system (track library, preset creation from reference tracks)

The user requested complete user documentation covering all features, split into two functional families (mastering vs preset creation), plus quick start guides in French and English.

## Deliverables

| File | Language | Min pages | Audience |
|---|---|---|---|
| `docs/user/MasterTweak_Guide_Mastering_FR.pdf` | French | 15 | Musicians who want to master their tracks |
| `docs/user/MasterTweak_Guide_PresetBuilder_FR.pdf` | French | 12 | Users who want to create custom presets |
| `docs/user/MasterTweak_QuickStart_FR.pdf` | French | 4 | New users — first mastering session |
| `docs/user/MasterTweak_QuickStart_EN.pdf` | English | 4 | New users — first mastering session |

## Technical Stack

- **Text authoring:** Markdown (pandoc-flavoured)
- **Mockups:** Python script generating PNG images (matplotlib + PIL rectangles/text)
- **PDF compilation:** `pandoc --pdf-engine=xelatex` with custom LaTeX template
- **Fonts:** DejaVu / Liberation (system fonts, good UTF-8 coverage)
- **Source files:** `docs/user/src/` — Markdown + LaTeX template + mockup generator script
- **Build script:** `docs/user/build.sh`

## Mockups to Generate

Each mockup is a 900×N px PNG, dark background (#1e1e1e), Qt-style rendering.

| ID | Description |
|---|---|
| `mock_main_window.png` | Full main window annotated with callouts (numbers 1–9) |
| `mock_chain_full.png` | Full DSP chain (all RackUnits stacked) |
| `mock_rack_resonance.png` | Resonance EQ RackUnit |
| `mock_rack_eq.png` | Parametric EQ RackUnit (7 band knobs) |
| `mock_rack_mb.png` | Multiband compressor RackUnit |
| `mock_rack_sat.png` | Saturator RackUnit |
| `mock_rack_width.png` | Stereo Width RackUnit |
| `mock_rack_mixbus.png` | Mixbus compressor RackUnit |
| `mock_rack_limiter.png` | Limiter RackUnit |
| `mock_transport.png` | Transport widget with VU meter, A/B button, scrub slider |
| `mock_midi_dialog.png` | MIDI Settings dialog |
| `mock_pb_ingest.png` | PresetBuilder — Ingest tab |
| `mock_pb_browse.png` | PresetBuilder — Browse tab |
| `mock_pb_create.png` | PresetBuilder — Create Preset tab |
| `mock_pb_manage.png` | PresetBuilder — Manage Presets tab |
| `mock_cli_help.png` | CLI help output in terminal style |
| `mock_dsp_flow.png` | DSP signal flow diagram |

## Mastering Guide — Chapter Outline (FR, ≥15 pages)

1. **Introduction** — what MasterTweak does, prerequisites, supported formats (0.5 p)
2. **Installation** — build from source, binary layout, system requirements (0.5 p)
3. **Interface principale** — annotated screenshot of the main window (1 p)
4. **Ouvrir un fichier audio** — formats supportés, glisser-déposer, libsndfile (0.5 p)
5. **Sélectionner un preset** — presets intégrés, presets utilisateur, rechargement (1 p)
6. **Analyse automatique** — SevenBandAnalyser, bandes de fréquences, lecture des résultats (1.5 p)
7. **Chaîne DSP** — vue d'ensemble du signal flow (1 p)
   - 7a. EQ de résonance — détection, cases à cocher (1 p)
   - 7b. EQ paramétrique 7 bandes — gains, Q automatique, shelves Sub/Air (1.5 p)
   - 7c. Compresseur multibande — seuil, ratio par bande, transients (1.5 p)
   - 7d. Saturateur — drive, tanh/soft-clip (0.5 p)
   - 7e. Largeur stéréo — M/S, par bande (1 p)
   - 7f. Compresseur de mixbus — seuil, ratio, makeup (1 p)
   - 7g. Limiteur — LUFS cible, plafond dBTP, oversampling (1 p)
8. **Profils de loudness** — table des 8 plateformes (Spotify, YouTube…) (0.5 p)
9. **Formats de sortie** — bit depth 16/24/32, WAV/FLAC (0.5 p)
10. **Render et Save As** — workflow complet, nom de fichier automatique (0.5 p)
11. **Transport et écoute** — play/stop, A/B, scrub, VU mètre (1 p)
12. **Export Advice** — rapport Markdown, utilité (0.5 p)
13. **Contrôle MIDI** — configuration port, mappings CC, transport, feedback (1 p)
14. **Interface en ligne de commande (CLI)** — tous les flags, exemples (2 p)
    - Mode single-file
    - Mode batch
    - Analyze-only
    - Bypass de stages
    - Overrides manuels (EQ, limiteur, saturateur)
    - Profils de loudness en CLI

**Total estimé : 18–20 pages**

## Preset Builder Guide — Chapter Outline (FR, ≥12 pages)

1. **Introduction** — pourquoi créer des presets, concept de bibliothèque de référence (1 p)
2. **Ouvrir le Preset Builder** — bouton Manage Presets, dialogue dédié (0.5 p)
3. **Onglet Ingest — Constitution de la bibliothèque** (2 p)
   - Ajout par dossier ou glisser-déposer
   - Extraction de métadonnées (AcoustID, tags embarqués, nom de fichier)
   - Barre de progression, rapport d'erreurs
   - Formats supportés (WAV, FLAC, AIFF, MP3, OGG)
4. **Onglet Browse — Parcourir la bibliothèque** (2 p)
   - Filtres titre/artiste/genre
   - Tableau des pistes (colonnes)
   - Édition inline des métadonnées
   - Lecture d'un fichier
   - Suppression de pistes
5. **Onglet Create — Créer un preset** (3 p)
   - Nom et description
   - Sélection de pistes (filtre + cases à cocher)
   - Panneau de statistiques (7 bandes + overall)
   - Calcul des stats (mean RMS, p10 correlation, median crest)
   - Auto-discover
   - Export vers `~/.config/MixAdvice/Presets/`
   - Save As — chemin personnalisé
6. **Onglet Manage — Gérer les presets** (2 p)
   - Liste des presets existants
   - Tableau des pistes avec déviation colorée
   - Retirer une piste d'un preset
   - Supprimer un preset
7. **Format XML des presets** — schéma, champs, compatibilité MixAdvice (1 p)
8. **Workflow recommandé** — exemple pas-à-pas de création d'un preset genre (1 p)

**Total estimé : 12–14 pages**

## Quick Start Guides — Outline (FR + EN, 4–5 pages each)

Both documents have the same structure, just translated:

1. **Étape 1 / Step 1 — Lancer MasterTweak** — binary name, first launch
2. **Étape 2 / Step 2 — Choisir un fichier audio** — click Open audio, supported formats
3. **Étape 3 / Step 3 — Sélectionner un preset** — dropdown, genre presets
4. **Étape 4 / Step 4 — Lire l'analyse** — what the advice means
5. **Étape 5 / Step 5 — Lancer le rendu** — Render button, progress, output path
6. **Étape 6 / Step 6 — Écouter et comparer** — play, A/B toggle
7. **Étape 7 / Step 7 — Sauvegarder** — Save As
8. **Références rapides** — platform loudness table, band frequency table, bypass list

## LaTeX Template Features

- Custom title page with version/date
- Auto table of contents
- Chapter/section headers with teal accent color (#2a9d8f)
- Callout boxes (tip, warning, note) via LaTeX `tcolorbox`
- Full-page mockup images with captions
- Frequency reference table (Sub 20–80 Hz, Lows 80–250 Hz, etc.)
- Footer: "MasterTweak v1.0 — Documentation"

## Build Process

```
docs/user/
  src/
    mastering_guide_fr.md
    preset_builder_guide_fr.md
    quickstart_fr.md
    quickstart_en.md
    template.latex
  mockups/
    generate_mockups.py   ← produces all PNG files
    img/                  ← generated PNGs
  build.sh                ← runs generate_mockups.py then 4× pandoc
```

`build.sh` sequence:
1. `python3 mockups/generate_mockups.py` — generate all PNGs
2. `pandoc --pdf-engine=xelatex -o ... mastering_guide_fr.md`
3. `pandoc --pdf-engine=xelatex -o ... preset_builder_guide_fr.md`
4. `pandoc --pdf-engine=xelatex -o ... quickstart_fr.md`
5. `pandoc --pdf-engine=xelatex -o ... quickstart_en.md`

Output PDFs land in `docs/user/`.

## Self-Review Checklist

- [ ] No TBDs or incomplete sections in any outline
- [ ] All DSP stages covered in mastering guide
- [ ] All 4 preset builder tabs covered
- [ ] All CLI flags documented
- [ ] Platform loudness table matches code (8 platforms)
- [ ] Band frequency table covers all 7 bands with crossover values
- [ ] Quick starts are self-contained (no reference to other guides)
- [ ] Mockup list covers all features mentioned in text
