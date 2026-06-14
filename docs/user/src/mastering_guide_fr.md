# MasterTweak — Guide de Mastering

## Table des Matières

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Interface Principale](#interface-principale)
4. [Ouvrir un Fichier Audio](#ouvrir-un-fichier-audio)
5. [Sélection du Preset](#sélection-du-preset)
6. [Analyse Automatique](#analyse-automatique)
7. [Chaîne DSP — Vue d'Ensemble](#chaîne-dsp--vue-densemble)
8. [Profils de Loudness](#profils-de-loudness)
9. [Formats de Sortie](#formats-de-sortie)
10. [Render et Save As](#render-et-save-as)
11. [Transport et Écoute](#transport-et-écoute)
12. [Export Advice](#export-advice)
13. [Contrôle MIDI](#contrôle-midi)
14. [Interface en Ligne de Commande (CLI)](#interface-en-ligne-de-commande-cli)
15. [Troubleshooting](#troubleshooting)

---

## Introduction

MasterTweak est une application de **mastering automatique hors ligne** (offline) conçue pour traiter rapidement votre musique avec les standards de loudness des plateformes modernes.

### Ce que fait MasterTweak

- **Analyse automatique** de votre fichier audio pour extraire le RMS (intensité moyenne), la corrélation stéréo et le facteur de crête (crest factor)
- **Application intelligente d'une chaîne DSP** : égalisation paramétrique 7 bandes, compression multibande, saturation, contrôle stéréo, compression de mixbus et limitation true-peak
- **Rendu hors ligne** sans contrainte temps réel — qualité maximale, pas de latence
- **Prévisualisation** en temps réel : après le rendu, écoutez le résultat via miniaudio avant d'exporter
- **Profils de loudness prédéfinis** pour Spotify (-14 LUFS), Apple Music (-16 LUFS), YouTube, CD, et broadcast

### Formats d'Entrée et Sortie

- **Formats d'entrée :** WAV, FLAC, AIFF (via libsndfile)
- **Formats de sortie :** WAV ou FLAC, 16/24/32 bits
- **Spécification audio :** Stereo ou Mono ; fréquence d'échantillonnage arbitraire (gérée automatiquement)

### Prérequis

- **OS :** Linux, macOS, Windows
- **Dépendances de runtime :** libsndfile, Qt6 (pour l'interface graphique)
- **Disque :** au moins 2 × la taille du fichier audio source (pour les fichiers temporaires)

---

## Installation

### Compilation depuis le code source

#### Sur Linux

```bash
# Installer les dépendances
sudo apt install cmake ninja-build build-essential git curl \
    libsndfile1-dev qt6-base-dev

# Télécharger et compiler
git clone https://github.com/user/MasterTweak.git
cd MasterTweak
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

#### Sur macOS

```bash
brew install cmake ninja qt@6

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build --parallel
```

### Emplacements des binaires

Après compilation, trouvez les exécutables ici :

- **Interface graphique :** `build/gui/MasterTweak`
- **Interface ligne de commande :** `build/cli/mastertweak`

### Structure de fichiers

```
~/.config/MasterTweak/
├── presets/                 # Fichiers XML de presets utilisateur
├── history/                 # Historique de fichiers ouverts récemment
└── preset_builder.db        # Base de données du générateur de presets
```

Les presets intégrés se trouvent dans le répertoire `presets/` du code source.

---

## Interface Principale

![](mockups/img/mock_main_window.png)

L'interface graphique de MasterTweak se compose de **5 zones principales** :

### Zone 1 : Barre de menu et fichier
- **File → Open** : charge un fichier audio
- **File → Save As** : exporte le fichier mastered
- **File → Recent** : accès rapide aux fichiers récents
- **Help → About** : informations de version

### Zone 2 : Sélecteur de preset
Dropdown menu affichant :
- **Presets intégrés :** Rock, Pop, Hip-Hop, Ambient, Electronic, Acoustic, Classical, Custom
- **Presets utilisateur :** tous les XMLs présents dans `~/.config/MasterTweak/presets/`

Changez le preset à tout moment ; l'analyse se relance automatiquement.

### Zone 3 : Panneaux d'analyse
Affiche en temps réel :
- **RMS par bande** (7 barres : Sub, Lows, LowMids, Mids, HiMids, Highs, Air)
- **Corrélation stéréo** (L/R correlation) 0.0–1.0
- **Facteur de crête** (crest factor) en dB

### Zone 4 : Contrôles de DSP
- **EQ Paramétrique :** 7 sliders pour gain per-band (±12 dB)
- **Compression Multibande :** checkbox pour on/off, ratio variable
- **Saturateur :** slider de drive (0–6 dB)
- **Largeur Stéréo :** slider global per-band (−1.0 à +1.0)
- **Limiteur :** LUFS target (dropdown 8 profils), ceiling (−1.0 dBTP)

### Zone 5 : Transport et rendu
- **Play/Stop** : lire le fichier courant (ou version mastered après rendu)
- **Render** : lance l'analyse + chaîne DSP → sauvegarde temporaire
- **A/B** : compare avant/après
- **VU Meter** : visualisation temps réel du niveau de sortie

---

## Ouvrir un Fichier Audio

### Workflow

1. Cliquez sur **File → Open** ou pressez `Ctrl+O`
2. Une boîte de dialogue standard système s'affiche
3. Sélectionnez votre fichier (WAV, FLAC ou AIFF)
4. Confirmez : le fichier est chargé

### Processus automatique après chargement

- Analyse immédiate de la waveform via `SevenBandAnalyser`
- Calcul du RMS, corrélation et crest factor
- Affichage dans les panneaux d'analyse (Zone 3)
- Préparation du DSP avec les paramètres du preset courant

### Formats supportés

| Format | Extension | Fréquences d'échantillonnage | Canaux |
|--------|-----------|------------------------------|--------|
| WAV    | `.wav`    | Toutes (44.1 kHz, 48 kHz, 96 kHz, etc.) | Mono/Stéréo |
| FLAC   | `.flac`   | Toutes                      | Mono/Stéréo |
| AIFF   | `.aiff`   | Toutes                      | Mono/Stéréo |

---

## Sélection du Preset

### Presets intégrés vs personnalisés

MasterTweak inclut 8 presets optimisés pour des genres :

| Preset | Cible | RMS global | Transients |
|--------|-------|-----------|-----------|
| **Rock** | Rock/Metal | −13 dBRMS | Agressifs |
| **Pop** | Synthpop/Mainstream | −14 dBRMS | Lisses |
| **Hip-Hop** | Trap/Boom-Bap | −12 dBRMS | Élevés |
| **Ambient** | Ambient/Chillout | −16 dBRMS | Très lisses |
| **Electronic** | Electro/Techno | −13 dBRMS | Basses dures |
| **Acoustic** | Folk/Acoustique | −15 dBRMS | Naturels |
| **Classical** | Classique/Orchestre | −14 dBRMS | Très lisses |
| **Custom** | Utilisateur | Variable | Variable |

### Workflow de sélection

1. Ouvrez le dropdown **Preset** (Zone 2)
2. Choisissez un preset
3. L'analyse est **relancée automatiquement** avec les nouveaux paramètres
4. Les panneaux de DSP affichent les nouveaux gains/ratios recommandés

### Importer un preset personnalisé

1. Construisez un XML via **MixAdvice** (preset builder Python) ou manuellement
2. Copie-le dans `~/.config/MasterTweak/presets/`
3. Redémarrez MasterTweak ou cliquez **File → Refresh Presets**
4. Le preset apparaît dans le dropdown

---

## Analyse Automatique

MasterTweak analyse chaque fichier audio chargé pour extraire **trois métriques** utilisées par la chaîne DSP :

### RMS (Root Mean Square)

Le **RMS** est l'intensité moyenne du signal par bande de fréquence.

```
RMS[i] = sqrt( (1/N) * Σ(sample[n])² )  pour la bande i
```

- Calculé sur chaque bande via des filtres Linkwitz-Riley 4ème ordre
- Plage : 0 dB (silence) à −∞ dB (très faible)
- MasterTweak vise un RMS cible défini dans le preset pour l'égalisation

### Corrélation Stéréo (L/R Correlation)

La corrélation mesure la similitude entre canaux gauche et droit.

```
corr = <L[n] × R[n]> / (RMS_L × RMS_R)
```

- Plage : 0.0 (indépendant, stéréo large) à 1.0 (identique, mono)
- Valeurs faibles (< 0.3) indiquent des problèmes de phase
- MasterTweak utilise la corrélation pour ajuster la **largeur stéréo** (voir section 7e)

### Facteur de Crête (Crest Factor)

Le crest factor est la différence entre le **pic** et le **RMS moyen**.

```
crest_factor = peak_amplitude / RMS
```

- Plage typique : 6–18 dB
- Musique très compressée : ~6 dB
- Musique classique : ~18 dB
- MasterTweak l'utilise pour :
  - Calculer le **gain du saturateur** (plus de crête = moins de drive)
  - Déterminer le **plafond du limiteur** (ceiling)

### Tableau de mesure et seuils

| Métrique | Min | Max | Diagnostic |
|----------|-----|-----|-----------|
| RMS bande | −∞ dB | −20 dB | Bande très faible |
| Corrélation | 0.0 | 1.0 | 0.3–0.7 = stéréo normal |
| Crest factor | 6 dB | 24 dB | Classique, très dynamique |

---

## Chaîne DSP — Vue d'Ensemble

![](mockups/img/mock_dsp_chain.png)

MasterTweak applique une **chaîne DSP de 8 étapes** après analyse. Chaque étape traite le signal stéréo et peut être contrôlée via l'interface graphique ou les flags CLI.

### Flux de signal

```
Audio d'entrée (WAV/FLAC/AIFF)
       ↓
[1] Détection de résonance (FFT, optionnel)
       ↓
[2] Égalisation paramétrique 7 bandes
       ↓
[3] Compresseur multibande (7 compresseurs parallèles)
       ↓
[4] Saturateur (tanh soft-clip)
       ↓
[5] Contrôle stéréo (M/S decode → width per-band → encode)
       ↓
[6] Compresseur de mixbus (glue global)
       ↓
[7] Limiteur true-peak (2-pass, lookahead, 4× oversampling)
       ↓
[8] Dithering (TPDF, 16-bit output)
       ↓
Audio de sortie mastered
```

### Étape 1 : Détection de Résonance (Optionnel)

![](mockups/img/mock_resonance_detect.png)

**Objectif :** détecter les pics de résonance via FFT et appliquer des réductions précises.

**Processus :**
1. FFT sur 4096 samples avec fenêtre Hann
2. Détection des pics > seuil (−20 dB en-dessous du pic global)
3. Affichage de checkboxes pour chaque pic détecté
4. L'utilisateur peut activer/désactiver chaque réduction

**Exemple :** si la FFT détecte un pic à 87 Hz et 2.5 kHz, deux réductions d'EQ (bell, Q=2.0) sont proposées.

---

### Étape 2 : Égalisation Paramétrique 7 Bandes

![](mockups/img/mock_parametric_eq.png)

L'**égalisation paramétrique** corrige le bilan spectral en comparant le RMS mesuré avec la cible du preset.

#### Formule de gain

```
gain[i] = clamp(preset.bandRmsDb[i] − measured.avgRms[i], ±12 dB)
```

- Si `|gain| < 0.5 dB` → gain arrondi à 0 dB (inaudible, économise CPU)
- Si gain > 0 dB → amplification
- Si gain < 0 dB → atténuation

#### Structure des 7 bandes

| Bande | Fréquences | Type | Crossovers | Q |
|-------|-----------|------|-----------|---|
| **Sub** | 20–80 Hz | Shelf basse | 80 Hz | 0.7–2.0 |
| **Lows** | 80–250 Hz | Bell | 80/250 Hz | 0.7–2.0 |
| **LowMids** | 250–500 Hz | Bell | 250/500 Hz | 0.7–2.0 |
| **Mids** | 500–2 kHz | Bell | 500/2k Hz | 0.7–2.0 |
| **HiMids** | 2–6 kHz | Bell | 2k/6k Hz | 0.7–2.0 |
| **Highs** | 6–16 kHz | Bell | 6k/16k Hz | 0.7–2.0 |
| **Air** | 16 kHz+ | Shelf haute | 16 kHz | 0.7–2.0 |

#### Paramètre Q dynamique

Le facteur Q est ajusté selon la magnitude du gain :

```
Q = 0.7 + |gain| × (2.0 − 0.7) / 12.0
```

- Gains faibles → Q = 0.7 (large, naturel)
- Gains forts → Q = 2.0 (étroit, ciblé)

**Interface :** 7 sliders horizontaux, chacun contrôlant le gain de −12 à +12 dB, avec feedback temps réel.

---

### Étape 3 : Compresseur Multibande

![](mockups/img/mock_multiband_comp.png)

Le **compresseur multibande** applique 7 compresseurs indépendants (un par bande) pour contrôler les dynamiques sans affecter les autres bandes.

#### Architecture

```
Signal stéréo
    ↓
[Splitter Linkwitz-Riley 4ème ordre en 7 bandes]
    ↓
Bande 1 → [RMS detector] → [Compresseur] → Sortie 1
Bande 2 → [RMS detector] → [Compresseur] → Sortie 2
...
Bande 7 → [RMS detector] → [Compresseur] → Sortie 7
    ↓
[Sommateur]
    ↓
Signal stéréo compressé
```

#### Formule de threshold et ratio

Pour chaque bande `i` :

```
threshold[i] = preset.bandRmsDb[i] − 3 dB
excess = max(0, inputRms − threshold[i])
ratio[i] = clamp(1.0 + excess × 0.25, 1.1, 8.0)
```

**Exemple :** si `bandRmsDb[i] = −10 dB` et `inputRms = −5 dB` (5 dB d'excès) :
- `threshold = −13 dB`
- `excess = −5 − (−13) = 8 dB`
- `ratio = 1.0 + 8 × 0.25 = 3.0 : 1`

#### Gains attack/release

L'**attack time** et **release time** sont dérivés de `bandTransientDb[i]` et de l'index de bande :

```
attack_ms[i]  = 20 + i × 5      # 20 ms (sub) → 50 ms (air)
release_ms[i] = 100 + i × 20    # 100 ms (sub) → 220 ms (air)
```

Les bandes basses réagissent plus vite (transients importants) ; les hautes réagissent plus lentement.

#### Interface

- Checkbox global **Multiband Compression : On/Off**
- 7 sliders pour ajuster le ratio par bande (1.0–8.0)
- Affichage temps réel de la réduction de gain (GR) pour chaque bande

---

### Étape 4 : Saturateur

![](mockups/img/mock_saturator.png)

Le **saturateur** ajoute du caractère et du "glue" subtil via une fonction tanh.

#### Formule

```
gain_db = clamp(avg(bandTransientDb) − avg(measuredCrest), 0, 6)
output = tanh(input × 10^(gain_db/20))
```

- **Input :** résultat du compresseur multibande
- **Fonction :** tanh soft-clip (saturation lisse, non-aliasing)
- **Drive :** 0–6 dB, ajusté automatiquement selon le crest factor

**Interprétation :**
- Musique très compressée (crest faible) → plus de drive → saturation perceptible
- Musique dynamique (crest élevé) → moins de drive → saturation subtle

#### Interface

- Slider **Saturation Drive** (0–6 dB)
- Callback temps réel : changement du drive relance le rendu

---

### Étape 5 : Contrôle Stéréo (Largeur Stéréo)

![](mockups/img/mock_stereo_width.png)

Le **contrôle stéréo** ajuste la largeur M/S (mid/side) de manière indépendante par bande.

#### Algorithme M/S

```
M = (L + R) / 2     # Mono (centre)
S = (L − R) / 2     # Stéréo (côtés)

# Ajuster S par un facteur width[i] per-bande i
M_out = M
S_out = S × width[i]

L_out = (M_out + S_out) / 2
R_out = (M_out − S_out) / 2
```

#### Logique d'ajustement dynamique

```
headroom[i] = 1.0 − bandMinCorr[i]

if bandMinCorr[i] < 0.3:
    # Corrélation très faible = problème de phase
    width[i] = 0.5        # Réduire la stéréo pour "coller" les canaux
elif bandMinCorr[i] > 0.8:
    # Corrélation très haute = signal presque mono
    width[i] = 1.0 + headroom[i] × 0.5   # Augmenter la largeur si headroom
else:
    # Normal
    width[i] = 1.0
```

#### Interface

- 7 sliders verticaux, chacun pour une bande (−1.0 = mono, 0.0 = pas de change, +1.0 = 2× stéréo)
- Affichage de la corrélation mesurée par bande (petite jauge)
- Option **Auto Width** : applique la logique dynamique ci-dessus

---

### Étape 6 : Compresseur de Mixbus

![](mockups/img/mock_mixbus_comp.png)

Le **compresseur de mixbus** applique un "glue" global au signal mastered entier, similaire à un limiteur de chaîne stéréo en studio.

#### Formule (identique à multibande, mais sur le global)

```
threshold = preset.overallRmsDb − 3 dB
excess = max(0, measuredRms − threshold)
ratio = clamp(1.0 + excess × 0.25, 1.1, 4.0)   # Max 4:1 (moins que multibande)

attack_ms  = 30
release_ms = 150
```

- **RMS detector :** mesure le RMS global (tous les 512 samples)
- **Réduction de gain :** lissée exponentiellement
- **Sortie :** compressée, prête pour le limiteur

#### Interface

- Checkbox **Mixbus Compression : On/Off**
- Slider **Ratio** (1.0–4.0)
- Affichage temps réel du GR (réduction de gain) global

---

### Étape 7 : Limiteur True-Peak

![](mockups/img/mock_limiter.png)

Le **limiteur** est la dernière couche de protection, garantissant que le signal ne dépasse jamais le plafond LUFS cible.

#### Architecture 2-pass

MasterTweak utilise un algorithme **2-pass hors ligne** pour le limiteur :

**Pass 1 (forward):**
- Analyse le signal, détecte les pics lookahead
- Calcule les courbes de réduction de gain nécessaires

**Pass 2 (backward):**
- Applique les réductions de gain avec release exponentielle
- Garantit un comportement lisse et musical

#### Détection true-peak

Le vrai pic est détecté via **4× oversampling** interne :

```
vrai_pic_dBFS = max( |L_upsampled| , |R_upsampled| )
```

#### Formule LUFS → dBFS

```
# À partir du preset et de la mesure globale
target_lufs = preset.overallRmsDb + crest_offset
ceiling_dbfs = −1.0   # Limite brickwall

gain_reduction = clamp(vrai_pic − ceiling_dbfs, 0, ∞)
```

#### Interface

- Dropdown **LUFS Target :** 8 profils (Spotify, Apple, YouTube, etc.)
- Slider **Ceiling (dBTP)** : −1.0 à −6.0 dB (lecture seule, −1.0 standard)
- Affichage temps réel : headroom en-dessous du ceiling

---

### Étape 8 : Dithering

![](mockups/img/mock_dither.png)

Le **dithering TPDF** (Triangular Probability Density Function) est appliqué avant quantisation en 16 bits pour réduire les artefacts de troncature.

```
dither_signal = triangular_noise(−0.5, +0.5)
output_16bit = quantize( float_output + dither_signal, 16 bits)
```

- **Actif automatiquement** si bit depth de sortie ≤ 16 bits
- **Désactif** pour 24/32 bits (pas de bénéfice auditif)

---

## Profils de Loudness

MasterTweak inclut **8 profils de loudness** pour les plateformes et médias courants. Chacun définit le **RMS global cible** et les **caractéristiques de transient** optimales.

### Tableau des profils

| Plateforme | LUFS Target | Crest Target | Cas d'usage |
|-----------|-----------|------------|-----------|
| **Spotify** | −14 LUFS | ~6 dB | Music streaming (le plus courant) |
| **Apple Music** | −16 LUFS | ~7 dB | iTunes, Apple Music |
| **YouTube** | −13 LUFS | ~8 dB | YouTube Music, vidéos musicales |
| **Amazon Music** | −14 LUFS | ~6 dB | Amazon Music Unlimited |
| **CD Audio** | −9 LUFS | ~8 dB | Compact Disc (réf. audiophile) |
| **Broadcast (EBU)** | −23 LUFS | ~10 dB | Radio, TV, streaming vidéo |
| **Podcast (ACX)** | −16 LUFS | ~8 dB | Audiobooks, podasts |
| **Cassette/Vinyl** | −12 LUFS | ~10 dB | Formats analogiques |

### Sélection du profil

1. Ouvrez le dropdown **LUFS Target** (Zone 4)
2. Choisissez votre plateforme cible
3. Le limiteur ajuste automatiquement le plafond cible
4. La chaîne DSP est relancée avec les nouveaux seuils

### Cas d'usage mixtes

Si vous visez plusieurs plateformes (par ex. Spotify + CD) :
- Rendez d'abord pour **Spotify** (−14 LUFS)
- Rendez ensuite pour **CD** (−9 LUFS) avec un nouveau fichier de sortie
- Les deux fichiers respecteront les standards de chaque plateforme

---

## Formats de Sortie

### Choix de format

MasterTweak supporte **WAV et FLAC** en sortie, avec 3 profondeurs de bit :

| Format | Profondeur | Cas d'usage | Taille |
|--------|-----------|-----------|--------|
| **WAV 16-bit** | PCM 16 bits | CD Audio, archivage standard | ~10 MB/min |
| **WAV 24-bit** | PCM 24 bits | Archivage haute résolution, pro-audio | ~15 MB/min |
| **WAV 32-bit** | PCM 32 bits (float) | Édition ultérieure (en pratique rare) | ~20 MB/min |
| **FLAC 16-bit** | FLAC 16 bits sans perte | Distribution lossless comprimée | ~4 MB/min |
| **FLAC 24-bit** | FLAC 24 bits sans perte | Archivage haute résolution, Tidal HiFi | ~7 MB/min |

### Recommandations

- **Spotify :** WAV 16-bit ou FLAC 16-bit (Spotify ré-encode de toute façon)
- **CD :** WAV 16-bit obligatoire
- **Archivage personnel :** FLAC 24-bit (sans perte, comprimé)
- **Pro-audio ultérieur :** WAV 24-bit (préservation de la qualité, pas d'interférence avec dither)

### Gestion des canaux

- **Stéréo :** 2 canaux (standard)
- **Mono :** 1 canal (détecté automatiquement ; égalisation et compression adaptées)

---

## Render et Save As

### Workflow de rendu

1. **Ouvrez** un fichier audio (**File → Open**)
2. **Choisissez** le preset (dropdown)
3. *(Optionnel)* **Ajustez** les paramètres DSP (sliders, checkboxes)
4. Cliquez **Render** (Zone 5)
5. L'application lance l'**analyse + chaîne DSP** et sauvegarde un fichier temporaire
6. Après rendu, cliquez **Play** pour écouter le résultat

### Save As

1. Après rendu, cliquez **File → Save As** ou `Ctrl+Shift+S`
2. Choisissez le **format** (WAV ou FLAC)
3. Choisissez la **profondeur de bit** (16, 24, ou 32 bits)
4. Entrez le **nom du fichier**
5. Confirmez : le fichier est écrit sur disque

### Convention de nommage par défaut

MasterTweak génère automatiquement un nom suggéré basé sur le fichier source :

```
source.wav  →  source_mastered.wav
track.flac  →  track_mastered_spotify.flac   (si profile = Spotify)
```

### Fichiers temporaires

Pendant le rendu, un fichier temporaire est créé dans `$TMPDIR` ou `C:\Temp` (Windows) :

```
~/.tmp/MasterTweak_XXXXXX.wav
```

Ce fichier est supprimé après **Save As** réussi ou à la fermeture de l'application.

---

## Transport et Écoute

![](mockups/img/mock_transport.png)

### Boutons de transport

| Bouton | Touche | Fonction |
|--------|--------|----------|
| **Play** | Espace | Lance la lecture du fichier (source ou version mastered après rendu) |
| **Stop** | Espace (double appui) | Arrête la lecture et réinitialise le curseur au début |
| **Pause** | P | Met en pause (curseur reste à la même position) |
| **<<** | Flèche gauche | Saute 5 secondes en arrière |
| **>>** | Flèche droite | Saute 5 secondes en avant |
| **A/B** | B | Bascule entre la version **avant** (source) et **après** (mastered) |

### Scrub (défilement du curseur)

- **Clic sur la timeline :** avance/recule au point cliqué
- **Glissé du curseur :** permet d'écouter en "scrub" continu

### VU Meter (Jauge de niveau)

Une jauge verticale affiche le **niveau de sortie en temps réel** :

- **Zone verte** : −20 à 0 dB (normal)
- **Zone jaune** : −6 à −1 dB (approche du plafond)
- **Zone rouge** : −1 à +0 dB (écrêtage, ne devrait pas survenir après limiteur)

Lectures affichées en dBFS (décibels relatifs au max de l'échelle).

### Marqueurs d'erreur

Si le limiteur détecte des pics dépassant le ceiling :
- La jauge passe au **rouge clignotant**
- Un message d'avertissement s'affiche en bas de la fenêtre

---

## Export Advice

MasterTweak peut exporter un **rapport textuel en Markdown** de l'analyse et des recommendations appliquées.

### Workflow

1. Après analyse (chargement du fichier), cliquez **File → Export Advice**
2. Choisissez le nom du fichier (suggestion : `<track>_advice.md`)
3. Le fichier est écrit sur disque

### Contenu du rapport

```markdown
# MasterTweak Analysis Report

**File:** track.wav  
**Preset:** Spotify  
**Date:** 2026-06-14  

## Measurements

### RMS per Band (dBRMS)
- Sub (20–80 Hz)       : −18.5 dB → Target −14 dB → +3.5 dB gain
- Lows (80–250 Hz)     : −12.0 dB → Target −12 dB → 0 dB gain
- LowMids (250–500 Hz) : −11.2 dB → Target −10 dB → +0.8 dB gain
...

### Stereo Correlation (per band)
- Sub      : 0.92 (very correlated, mono-like)
- Lows     : 0.85 (good stereo)
...

### Crest Factor
- Global : 7.2 dB (well-compressed)

## Recommendations

### EQ Gains Applied
- Sub shelf : +3.5 dB (Q=1.5)
- Lows bell : 0 dB (no change)
...

### Multiband Compression
- Sub  : ratio 1.5:1, threshold −17 dB
- Lows : ratio 1.0:1 (bypassed, no excess)
...

### Saturation Drive
- Estimated : 1.2 dB (subtle, music already compressed)

### Stereo Width
- Sub      : 0.5× (collapsed, correlation too high)
...

### Limiter Ceiling
- Target LUFS : −14 LUFS (Spotify)
- Ceiling dBTP : −1.0 (brickwall)
- Headroom    : 0.3 dB (tight!)

## Conclusion

This track is well-balanced. Minor EQ lift in sub/low-mids will match Spotify targets. No aggressive processing needed.
```

### Cas d'usage

- **Documentation :** gardez trace de ce qui a été appliqué à chaque morceau
- **Comparaison :** comparez les rapports de plusieurs versions/presets
- **Référence externe :** partagez avec un collègue producteur/mix engineer

---

## Contrôle MIDI

MasterTweak supporte le **contrôle MIDI** pour piloter les paramètres DSP sans souris.

### Configuration MIDI

1. Allez à **Edit → MIDI Setup** (interface graphique)
2. Sélectionnez votre **périphérique MIDI** dans la liste
3. Cliquez sur chaque paramètre et tournez/déplacez votre contrôleur pour apprendre l'assignation
4. Les mappages sont sauvegardés automatiquement dans `~/.config/MasterTweak/midi_mappings.json`

### Mappages par défaut (si disponible)

Si vous utilisez une **surface de 8 faders** (Korg Nano, Behringer FCB1010, etc.) :

| Fader | Paramètre |
|-------|-----------|
| Fader 1 | EQ Gain Sub |
| Fader 2 | EQ Gain Lows |
| Fader 3 | EQ Gain LowMids |
| Fader 4 | EQ Gain Mids |
| Fader 5 | EQ Gain HiMids |
| Fader 6 | EQ Gain Highs |
| Fader 7 | EQ Gain Air |
| Fader 8 | Saturation Drive |

| Bouton | Paramètre |
|--------|-----------|
| Button 1 | Multiband Comp On/Off |
| Button 2 | Mixbus Comp On/Off |
| Button 3 | Stereo Width Auto/Manual |
| Button 4 | Play/Stop Transport |

### Plages MIDI

- **CC 0–127 :** mappé à 0–100 % du paramètre (bénéfice : résolution fine)
- **Note On/Off :** pour les boutons (127 = activé, 0 = désactivé)
- **Pitch Bend :** non utilisé (réservé pour future expansion)

---

## Interface en Ligne de Commande (CLI)

MasterTweak inclut une **interface CLI** complète pour le traitement batch et l'intégration dans des workflows de production.

### Utilisation basique

```bash
./mastertweak [options] <input_file>
```

### Options principales

#### Fichier d'entrée et sortie

```bash
mastertweak --input track.wav --output track_out.wav
mastertweak track.flac                # output → track_mastered.wav (défaut)
```

#### Sélection du preset

```bash
mastertweak --preset Spotify track.wav
mastertweak --preset /path/to/custom.xml track.wav
mastertweak --preset-dir ~/.config/MasterTweak/presets/ --preset CustomGenre track.wav
```

#### Contrôle LUFS

```bash
mastertweak --lufs-target spotify track.wav      # −14 LUFS (Spotify)
mastertweak --lufs-target apple track.wav        # −16 LUFS (Apple)
mastertweak --lufs-target cd track.wav           # −9 LUFS (CD)
mastertweak --lufs-target custom:-12 track.wav   # −12 LUFS (personnalisé)
```

#### Format de sortie

```bash
mastertweak --output-format wav --bit-depth 16 track.wav
mastertweak --output-format flac --bit-depth 24 track.wav
mastertweak -F wav -B 32 track.wav                # Abréviations (wav 32-bit)
```

#### Overrides DSP

Tous les overrides déclenchent une **pré-analyse automatique** pour adapter les seuils.

```bash
# Override EQ : +3 dB Sub, 0 dB Lows, −2 dB Highs
mastertweak --override-eq "3,0,0,0,0,-2,0" track.wav

# Override compression multibande : ratio 2:1 pour toutes les bandes
mastertweak --override-multiband-ratio 2.0 track.wav

# Override saturation drive : 2.5 dB
mastertweak --override-saturation-drive 2.5 track.wav

# Override limiteur ceiling : −2 dB (lieu de −1 dB standard)
mastertweak --override-limiter-ceiling -2 track.wav
```

#### Analyse uniquement (pas de rendu)

```bash
mastertweak --analyze-only track.wav
# Affiche les métriques RMS, corrélation, crest factor et quitte
```

#### Export du rapport Advice

```bash
mastertweak --export-advice report.md track.wav
# Rend ET exporte un rapport Markdown
```

#### Paramètres de verbosité/debug

```bash
mastertweak --verbose track.wav       # Affiche timings, GR, etc.
mastertweak --debug track.wav         # Dump complet (fichiers intermédiaires)
```

### Exemples complets

#### Exemple 1 : Mastering standard Spotify

```bash
./mastertweak \
  --input album_01.wav \
  --output album_01_spotify.wav \
  --preset Spotify \
  --lufs-target spotify \
  --output-format wav \
  --bit-depth 16
```

#### Exemple 2 : Batch processing (boucle shell)

```bash
#!/bin/bash
for track in *.wav; do
  echo "Processing $track..."
  ./mastertweak \
    --input "$track" \
    --output "${track%.wav}_mastered.wav" \
    --preset Rock \
    --verbose
done
```

#### Exemple 3 : Analyse + rapport uniquement

```bash
./mastertweak \
  --input live_recording.flac \
  --analyze-only \
  --export-advice live_recording_report.md \
  --preset Acoustic
```

#### Exemple 4 : Override EQ personnalisé

```bash
./mastertweak \
  --input vocal.wav \
  --preset Pop \
  --override-eq "2,1,0,-1,-2,3,2" \
  --override-saturation-drive 0.5 \
  --output vocal_edited.wav
```

### Codes de sortie

| Code | Signification |
|------|--------------|
| 0 | Succès |
| 1 | Erreur fichier d'entrée (non trouvé, format invalide) |
| 2 | Erreur preset (non trouvé, XML invalide) |
| 3 | Erreur écriture fichier (permissions, disque plein) |
| 4 | Erreur DSP (underflow, allocation mémoire) |
| 5 | Utilisateur annulé (Ctrl+C) |

---

## Troubleshooting

### Problème : "Preset not found" (CLI)

**Cause :** le fichier preset XML n'existe pas ou n'est pas au bon endroit.

**Solutions :**
1. Vérifiez le chemin : `ls ~/.config/MasterTweak/presets/`
2. Vérifiez le nom exact du preset (sensible à la casse)
3. Utilisez un chemin absolu : `--preset /full/path/to/preset.xml`
4. Listing des presets disponibles : `mastertweak --list-presets`

### Problème : "Output file is clipping" (rouge limiteur)

**Cause :** le limiteur détecte que le signal approche ou dépasse le ceiling cible.

**Solutions :**
1. Abaissez le **LUFS target** d'un cran (ex. Spotify −14 → Custom −15)
2. Réduisez le **saturation drive** (slider ou `--override-saturation-drive 0`)
3. Augmentez le **limiter ceiling** légèrement (ex. −1 dB → −1.5 dB)
4. Vérifiez que l'audio source n'est pas déjà excessivement compressé

### Problème : "Audio crackles or artifacts" (craquements/artefacts)

**Cause :** dépassement DSP interne, débordement de buffer, ou fréquence d'échantillonnage incompatible.

**Solutions :**
1. Vérifiez la **fréquence d'échantillonnage** du fichier source (44.1, 48, 96 kHz ?)
2. Essayez un **preset différent** (certains sont plus agressifs)
3. Réduisez les **overrides DSP** (gain EQ, saturation drive)
4. Relancez l'application (peut nettoyer les buffers internes)
5. Contactez le support avec la ligne de commande exacte

### Problème : "Preset loading fails in GUI"

**Cause :** fichier XML corrompu ou schéma incompatible.

**Solutions :**
1. Vérifiez que le XML est valide (ouvrez-le dans un éditeur texte)
2. Vérifiez les balises requises : `<bandRmsDb>` (7 valeurs), `<bandTransientDb>` (7 valeurs), `<overallRmsDb>` (1 valeur)
3. Vérifiez le chemin absolu du fichier
4. Créez un nouveau preset via le **preset builder** de MixAdvice

### Problème : "Rendez très lent" (prise de temps anormale)

**Cause :** le fichier est très long ou les ressources système sont épuisées.

**Métriques attendues :**
- Fichier 3 min @ 48 kHz stéréo 24-bit : ~2–5 secondes
- Fichier 10 min @ 96 kHz stéréo 24-bit : ~5–10 secondes

**Solutions :**
1. Fermez les autres applications (navigateur, DAW, etc.)
2. Réduisez la **profondeur de bit de sortie** (32-bit → 24-bit ou 16-bit)
3. Si possible, ré-échantillonnez le fichier à 48 kHz avant le mastering
4. Utilisez le **CLI en batch mode** (peut être parallélisé avec GNU Parallel)

### Problème : "Qt error on Linux" (erreur Qt à l'ouverture de la GUI)

**Cause :** Qt6 ne trouve pas les plugins de plateforme ou de thème.

**Solutions :**
1. Vérifiez que Qt6 est installé : `apt install qt6-base-dev`
2. Définissez `QT_QPA_PLATFORM_PLUGIN_PATH` :
   ```bash
   export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins
   ./build/gui/MasterTweak
   ```
3. Utilisez le CLI à la place (aucune dépendance graphique)

### Problème : "Missing libsndfile on macOS"

**Cause :** libsndfile n'est pas installé.

**Solution :**
```bash
brew install libsndfile
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Problème : "MIDI controller not recognized"

**Cause :** le périphérique MIDI n'est pas détecté par la couche ALSA (Linux) ou CoreMIDI (macOS).

**Solutions :**
1. **Linux :**
   ```bash
   # Vérifiez les ports MIDI disponibles
   aconnect -l
   # Connectez manuellement le périphérique
   aconnect 20:0 128:0  # exemples de numéros de port
   ```
2. **macOS :**
   - Audio MIDI Setup → Ouvrez la salle MIDI
   - Vérifiez que le périphérique apparaît et est connecté
3. Essayez un autre port USB
4. Mettez à jour le firmware du contrôleur

---

## Appendice : Architecture Technique (Pour Utilisateurs Avancés)

### Allocation mémoire et performance

MasterTweak pré-alloue **buffers de travail** lors du chargement du fichier pour éviter les allocations temps réel pendant le rendu :

```
Buffer size = max(file length, 1 MB)
```

Pour un fichier 100 MB, environ 200 MB de RAM (source + sortie + DSP) sont utilisés.

### Précision interne et dithering

- **Traitement interne :** `double` (64-bit float) pour le limiteur et le mixbus
- **Étapes DSP individuelles :** `float` (32-bit) suffisant, sauf indication contraire
- **Dithering :** automatique 16-bit, désactif ≥24-bit

### Format de fichier temporaire

Les fichiers temporaires sont en **WAV 32-bit float**, permettant une re-render sans perte de précision entre les passes.

---

## Contact et Support

- **Documentation :** [https://github.com/user/MasterTweak/wiki](https://github.com/user/MasterTweak/wiki)
- **Issues GitHub :** [https://github.com/user/MasterTweak/issues](https://github.com/user/MasterTweak/issues)
- **Email support :** yvan.janet@gmail.com

---

*Dernière mise à jour : 2026-06-14*

*MasterTweak v1.0 — Offline Auto-Mastering for the Modern Producer*
