# MasterTweak — Guide de Création de Presets

## Introduction

Un **preset** est une collection de pistes audio de référence représentant un style ou un genre donné. MasterTweak analyse ces pistes pour extraire des cibles acoustiques (RMS, corrélation, crest), puis les utilise pour le mastering automatique.

### Pourquoi Créer des Presets ?

- **Profils genre-spécifiques** — Pop, Synthwave, Trap, Ambient, etc.
- **Profils artiste/label** — Replicar le son d'un catalogue
- **Fine-tuning** — Adapter les presets intégrés à vos goûts

### Workflow Général

1. **Ingest** — Ajouter des pistes audio à votre bibliothèque
2. **Browse** — Parcourir, éditer les métadonnées
3. **Create** — Sélectionner des pistes, générer un preset
4. **Manage** — Refiner ou supprimer des presets

## Ouvrir le Preset Builder

1. Cliquez sur **Manage Presets** dans la fenêtre principale
2. Le dialogue Preset Builder s'ouvre avec 4 onglets

![Preset Builder — 4 onglets](mockups/img/mock_pb_ingest.png)

## Onglet Ingest — Constituer la Bibliothèque

### Ajouter des Fichiers

**Bouton "Add Folder"** — Sélectionne un dossier, scanne tous les fichiers audio récursivement.

**Glisser-déposer** — Glissez des fichiers/dossiers sur la zone pour les importer.

**Formats supportés :** WAV, FLAC, AIFF, MP3, OGG

### Extraction de Métadonnées

Pour chaque fichier, MasterTweak tente d'extraire :
- **Titre** — Tags MP3/Vorbis, ou nom de fichier
- **Artiste** — Tags, ou dossier parent
- **Genre** — Tags
- **Année** — Tags

**Sources (ordre de priorité) :**
1. AcoustID (en ligne, FingerprintID database)
2. Tags embarqués (ID3, Vorbis Comments, MP4)
3. Nom de fichier / chemin

### Progression et Rapports

Une barre de progression affiche `45/120 files`. Après Ingest, un rapport résume :

```
Added: 45  Skipped: 10  Failed: 2
```

**Erreurs courantes :**
- Fichier corrompu → Skip
- Format non supporté (.m4a, .aac) → Skip
- Problème de permissions → Fail

Cliquez sur la liste d'erreurs pour voir les détails.

## Onglet Browse — Parcourir la Bibliothèque

![Browse Tab](mockups/img/mock_pb_browse.png)

### Filtrage

Trois champs texte pour chercher :
- **Title** — Recherche parmi les titres (contient, case-insensitive)
- **Artist** — Filtre par artiste
- **Genre** — Filtre par genre

Les filtres sont cumulatifs (ET logique).

### Édition Inline

Cliquez sur une cellule du tableau pour éditer titre, artiste, genre, année.

### Actions

- **Play** — Lance la lecture de la piste sélectionnée (audio preview)
- **Delete** — Supprime la piste de la bibliothèque (irréversible)

### Tableau des Colonnes

| Colonne | Contenu |
|---------|---------|
| Title | Titre de la piste |
| Artist | Artiste |
| Album | Album (optionnel) |
| Genre | Genre |
| Year | Année |
| Source | Origine métadonnées (acoustid, embedded_tags, filename) |
| Path | Chemin complet (caché, clic droit pour copier) |

## Onglet Create — Créer un Preset

![Create Preset Tab](mockups/img/mock_pb_create.png)

### 1. Nommer le Preset

**Champs :**
- **Preset Name** — e.g., "Pop - Bright", "EDM - Deep"
- **Description** — Notes (optionnel)

### 2. Sélectionner les Pistes de Référence

**Filtres :** Comme Browse, titre/artiste/genre

**Cases à cocher :** Cochez les pistes à inclure. Votre sélection persiste même si vous changez les filtres.

**Nombre de pistes recommandé :** 10–30 pistes pour une bonne représentativité.

### 3. Panneau de Statistiques

Une fois les pistes sélectionnées, des statistiques s'affichent :

```
Sub        RMS: -28.2 dB  |  Corr p10: 0.94  |  Crest med: 8.2 dB
Lows       RMS: -22.1 dB  |  Corr p10: 0.91  |  Crest med: 9.5 dB
...
Overall    RMS: -18.5 dB  |  Corr p10: 0.88
```

**Métriques :**
- **RMS** — Moyenne arithmétique (LUFS-like)
- **Corr p10** — 10e percentile de corrélation L/R (seuil de largeur)
- **Crest médian** — Médiane du crest factor (dynamique)

### 4. Auto-Discover

Bouton **Auto-Discover** — Scanne la bibliothèque entière et sélectionne automatiquement les pistes similaires au genre du preset (basé sur métadonnées).

**Utilité :** Gain de temps pour créer des presets larges.

### 5. Export

**Export Default** — Exporte vers `~/.config/MixAdvice/Presets/{preset-name}.xml`

**Save As…** — Chemin personnalisé

Le preset XML peut alors être utilisé en GUI ou CLI pour le mastering.

## Onglet Manage — Affiner les Presets

![Manage Presets Tab](mockups/img/mock_pb_manage.png)

### Lister les Presets

La partie gauche affiche tous les presets créés.

### Tableau des Pistes

Pour le preset sélectionné, tableau avec colonnes :
- **Title** — Titre
- **Artist** — Artiste
- **Genre** — Genre
- **Sub / Lows / …** — Déviation RMS par bande (colorée)
- **Remove** — Bouton pour ôter la piste du preset

### Indice de Déviation

Chaque piste affiche sa déviation RMS par rapport à la cible du preset.

**Codes couleur :**
- 🟢 Vert (< ±2 dB) — Piste exemplaire
- 🟡 Jaune (±2–4 dB) — Acceptable
- 🔴 Rouge (> ±4 dB) — Outlier, candidate à la suppression

### Actions

- **Remove Track** — Supprime une piste du preset (recalcule les stats)
- **Delete Preset** — Supprime le preset entier (fichier XML supprimé)

### Workflow de Refinement

1. Créez un preset avec 20 pistes
2. Repérez les rouges (outliers)
3. Supprimez-les un par un
4. Exportez à nouveau
5. Le preset converge vers une cohésion plus forte

## Format XML du Preset

Un preset MasterTweak est un fichier XML compatible MixAdvice.

### Schéma

```xml
<?xml version="1.0"?>
<Preset>
  <name>Pop - Bright</name>
  <description>Vocal pop moderne avec équilibre tonal clair</description>
  
  <bandRmsDb>
    <band index="0">-28.2</band>  <!-- Sub -->
    <band index="1">-22.1</band>  <!-- Lows -->
    ...
    <band index="6">-8.5</band>   <!-- Air -->
  </bandRmsDb>
  
  <bandMinCorr>
    <band index="0">0.94</band>
    ...
  </bandMinCorr>
  
  <bandTransientDb>
    <band index="0">8.2</band>
    ...
  </bandTransientDb>
  
  <overallRmsDb>-18.5</overallRmsDb>
  <overallMinCorr>0.88</overallMinCorr>
</Preset>
```

### Signification des Champs

- **bandRmsDb[7]** — Cible RMS par bande (le mastering cherche à atteindre ces niveaux)
- **bandMinCorr[7]** — Seuil de corrélation L/R (en-dessous, le preset encourage la largeur)
- **bandTransientDb[7]** — Crest median (dynamique cible, affecte l'aggressivité de compression)
- **overallRmsDb** — Cible RMS globale
- **overallMinCorr** — Corrélation globale floor

## Exemple : Créer un Preset "Synthwave"

### Étape 1 — Constituer la Bibliothèque

- Téléchargez 3–4 albums synthwave (e.g., Carpenter Brut, Perturbator)
- Importez via **Ingest**
- Vérifiez les métadonnées, éditez les genres à "Synthwave"

### Étape 2 — Filtrer et Sélectionner

1. Allez à **Create**
2. Filtrez par genre "Synthwave"
3. Cochez 12–15 pistes (variété : refrains, breakdowns, solos)

### Étape 3 — Vérifier les Stats

```
Sub   RMS: -26.8 dB  Corr: 0.92  Crest: 7.5 dB
Lows  RMS: -20.5 dB  Corr: 0.90  Crest: 8.9 dB
Mids  RMS: -16.3 dB  Corr: 0.88  Crest: 10.2 dB
...
Overall: -17.5 dB, p10 corr: 0.86
```

### Étape 4 — Exporter

Cliquez **Export Default** → `~/.config/MixAdvice/Presets/Synthwave.xml`

### Étape 5 — Utiliser en Mastering

Lancez MasterTweak, sélectionnez "Synthwave" dans le dropdown.

## Conseils de Construction

### Taille et Représentativité

- **Petit** (5–10 pistes) — Très spécifique, risque de surfit
- **Moyen** (15–25 pistes) — Bon équilibre, recommandé
- **Grand** (> 30 pistes) — Lisse, générique

### Contrôle de Qualité

- Écoutez quelques pistes avec les oreilles, vérifiez la cohésion
- Supprimez les outliers (rouges) avant export final
- Ré-écoutez le preset appliqué à un titre de test

### Itération

Presets n'est pas figé. Vous pouvez :
1. Exporter "Pop v1"
2. Utiliser, évaluer, noter les améliorations
3. Ouvrir "Manage", ajouter/supprimer des pistes
4. Ré-exporter "Pop v2"

## Format des Métadonnées dans la DB

Interne, MasterTweak stocke dans `~/.config/MasterTweak/preset_builder.db` (SQLite) :

```sql
CREATE TABLE tracks (
  id TEXT PRIMARY KEY,       -- SHA-256 hash
  path TEXT NOT NULL,
  file_size INTEGER,
  title TEXT,
  artist TEXT,
  album TEXT,
  genre TEXT,
  year INTEGER,
  source TEXT,               -- acoustid|embedded_tags|filename
  band_rms REAL[7],
  band_corr REAL[7],
  band_crest REAL[7],
  overall_rms REAL,
  overall_corr REAL,
  added_at TEXT              -- ISO 8601
);

CREATE TABLE presets (
  id TEXT PRIMARY KEY,       -- UUID
  name TEXT NOT NULL,
  description TEXT,
  band_rms REAL[7],
  band_corr REAL[7],
  band_crest REAL[7],
  overall_rms REAL,
  overall_corr REAL,
  created_at TEXT,
  updated_at TEXT
);

CREATE TABLE preset_tracks (
  preset_id TEXT,
  track_id TEXT,
  PRIMARY KEY (preset_id, track_id)
);
```

Ce schéma garantit que les analyses sont cachées et réutilisables.

## Troubleshooting

**"Metadata extraction failed"**
- Fichier corrompu ou tags mal formés
- Essayez de remplir les tags manuellement (Browse tab)

**"Statistics seem wrong"**
- Vérifiez que vous avez coché au moins 5 pistes
- Vérifiez l'intégrité des fichiers (relancez l'analyse)

**"Export creates XML but preset doesn't appear"**
- Redémarrez MasterTweak
- Vérifiez que le fichier XML est dans `~/.config/MixAdvice/Presets/`

**"Auto-Discover selects nothing"**
- Taggez vos pistes avec un genre
- Relancez Auto-Discover

## Pour Aller Plus Loin

- **Scientifique derrière les presets** — Consultez CLAUDE.md pour l'algorithme complet
- **Partager des presets** — Exportez et partagez les fichiers XML
- **Intégration dans MixAdvice** — Les presets créés ici fonctionnent aussi dans MixAdvice
