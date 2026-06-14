# Guide de démarrage rapide MasterTweak (Français)

Bienvenue dans MasterTweak ! Ce guide vous accompagne dans le flux complet : charger un fichier audio, choisir un preset, et exporter une version maîtrisée en seulement 7 étapes simples.

---

## Démarrage en 7 Étapes

### Étape 1 : Lancer MasterTweak

Double-cliquez sur l'icône de l'application MasterTweak sur votre bureau ou dans le dossier Applications.

Sur **Linux**, lancez depuis le terminal :
```bash
./MasterTweak
```

La fenêtre principale s'affiche avec un projet vide. Vous verrez :
- **Navigateur de fichiers** (gauche)
- **Sélecteur de preset** (liste déroulante en haut à gauche)
- **Panneau de paramètres personnalisés** (droite) avec des curseurs pour l'EQ, la compression, la saturation, etc.
- **Affichage de forme d'onde** (centre, grisé jusqu'au chargement d'audio)
- **Bouton Rendu** (en bas à droite, désactivé jusqu'au chargement d'audio)

### Étape 2 : Ouvrir un fichier audio

1. Cliquez sur **Ouvrir un fichier** ou utilisez **Fichier → Ouvrir**.
2. Naviguez jusqu'à votre fichier WAV, FLAC ou AIFF.
3. Cliquez sur **Ouvrir**.

Le fichier est chargé et analysé automatiquement. Vous verrez :
- La forme d'onde affichée dans le panneau central
- **Analyse de fréquence 7 bandes** en arrière-plan (les pics rouges montrent le RMS mesuré par bande)
- **Résultats d'analyse** affichés sous la forme d'onde (RMS par bande, loudness, dynamique, etc.)
- **Bouton Rendu** devient **activé** (bleu clair)

### Étape 3 : Choisir un preset

Un preset est une recette de mastering : il définit la loudness cible, les cibles EQ, la forme de la compression et le caractère dynamique.

Dans la **liste déroulante Preset** (en haut à gauche), sélectionnez l'un des presets groupés :

| Preset | Cas d'usage |
|--------|------------|
| **Hip-Hop / Rap** | Agressif, percutant. Médiums bas serrés, graves forts, limitation puissante. |
| **Rock** | Équilibré, riche en transitoires. Aigus clairs, graves contrôlés. |
| **Électronique / EDM** | Brillant, stéréo large. Graves hauts passés, sommet aéré. |
| **Pop** | Commercial, lisse. Médiums chauds, comprimé, loudness moderne. |
| **Jazz / Acoustique** | Ouvert, dynamique. EQ minimal, compression légère. |
| **Classique** | Naturel, de référence. Saturation minimale, réponse linéaire. |
| **Podcast / Voix** | Clair, intelligible. Boost de la bande de présence (2–6 kHz), comp multibande sur les sifflantes. |

**Conseil :** Commencez par un preset qui correspond à votre genre. L'analyse s'alimente automatiquement dans la chaîne DSP, donc vous ne devez rien modifier—mais vous pouvez si vous le souhaitez.

### Étape 4 : Vérifier les paramètres (aucune modification requise)

Vous n'êtes pas obligé de changer quoi que ce soit. Chaque paramètre affiche :
- **Valeur actuelle** (position du curseur)
- **Recommandation par défaut** (calculée à partir de votre audio + preset)
- **Plage** (par exemple, −12 à +12 dB)

Si vous voulez écouter comment sonne le conseil par défaut :
- Cliquez sur **Aperçu** (ou appuyez sur la barre d'espace)
- L'audio se rend en temps réel et s'émet par vos haut-parleurs
- Écoutez la comparaison A/B (voir Étape 6)

Pour personnaliser (optionnel) :
- Glissez n'importe quel curseur pour remplacer la recommandation
- L'aperçu se réaffiche quand vous relâchez le curseur
- Vous pouvez toujours cliquer sur **Réinitialiser aux valeurs par défaut** pour revenir en arrière

### Étape 5 : Cliquer sur Rendu

Une fois satisfait de l'aperçu (ou prêt à accepter les paramètres par défaut), cliquez sur le grand bouton **Rendu** (en bas à droite).

Une boîte de dialogue de progression s'affiche :
- « Rendu en cours… 50% complets »
- Rendu en deux passes : analyser + appliquer la chaîne DSP
- Cela peut prendre 10–30 secondes selon la longueur du fichier et le CPU

Une fois terminé :
- **Bouton Rendu** devient **« Terminé ! Prêt à enregistrer »** (vert)
- **Bouton Exporter** devient **activé** (en haut à droite)

### Étape 6 : Écouter et comparer (A/B)

Avant d'enregistrer, vous pouvez faire un A/B de l'avant et après :

- **Affichage de forme d'onde** affiche la **version originale (bleu) vs. maîtrisée (rouge)** côte à côte
- Cliquez sur **Bascule A/B** (ou appuyez sur **T**) pour basculer entre :
  - Écoute du fichier **original**
  - Écoute du fichier **maîtrisé** (du tampon de rendu en mémoire)
  - Chacun lit les 10 premières secondes en boucle pour que vous puissiez bien écouter

**À quoi prêter attention :**
- Les graves sont plus serrés, plus présents (mais pas boueux)
- Le coup de pied/caisse/voix s'adapte bien au mix
- Les aigus sont brillants mais pas durs
- La loudness globale est cohérente entre les pistes
- Les dynamiques sont contrôlées sans sonner comprimées
- L'image stéréo est ouverte et équilibrée

Si vous voulez affiner et re-renderer, ajustez simplement les curseurs et cliquez à nouveau sur **Rendu**.

### Étape 7 : Enregistrer

Quand vous êtes satisfait du résultat :

1. Cliquez sur **Exporter** (ou **Fichier → Exporter**)
2. Choisissez les paramètres de sortie (voir ci-dessous)
3. Sélectionnez un nom de fichier et un emplacement
4. Cliquez sur **Enregistrer**

Le fichier maîtrisé est écrit sur le disque.

---

## Options courantes

### Format de sortie

En bas de la boîte de dialogue d'export, vous verrez :

- **Profondeur de bits :**
  - **16 bits** (qualité CD, plus petit fichier, utilisé pour la distribution)
  - **24 bits** (qualité studio, fichier plus volumineux, utilisé pour l'archivage)
  - **32 bits flottant** (qualité de mixage, rarement nécessaire pour la sortie finale)

- **Format :**
  - **WAV** (universel, non comprimé)
  - **FLAC** (sans perte, compression 30–50%, recommandé pour l'archivage)

### Plateforme de loudness cible

Sélectionnez la plateforme sur laquelle votre piste maîtrisée sera livrée. Cela contrôle le plafond du limiteur :

| Plateforme | Cible (LUFS) | Notes |
|-----------|--------------|-------|
| **Spotify** | −14 LUFS | Normalisé par l'algorithme ; excessif de dépasser. |
| **Apple Music** | −16 LUFS | Norme de loudness héritée. |
| **YouTube** | −13 LUFS | Plateforme vidéo ; légèrement plus loud que Spotify. |
| **Streaming (Générique)** | −14 LUFS | Milieu de terrain sûr pour tous les streamers. |
| **Radiodiffusion** | −23 LUFS | Radio, TV, podcasts. Beaucoup plus silencieux. |
| **CD / Référence** | −9 LUFS | Non comprimé, dynamique, pour archivage ou écoute audiophile. |

**Fonctionnement :** Le plafond du limiteur se déplace pour correspondre à votre plateforme cible. Une cible plus élevée (par ex. −9 LUFS pour CD) permet des pics plus forts ; une cible plus basse (par ex. −23 LUFS pour la radiodiffusion) écrase davantage les dynamiques.

### Bouton Exporter le conseil

Cliquez sur **Exporter le conseil** pour enregistrer un rapport Markdown détaillé de ce que MasterTweak a fait :
- RMS mesuré, corrélation et facteur de crête par bande
- Gains et valeurs Q de l'EQ appliqués
- Ratios et seuils de compression
- Lecteur de saturation
- Paramètres du limiteur
- Avertissements (par ex., « Facteur de crête élevé ; considérez plus de compression »)

Ceci est utile pour :
- Apprendre ce que le DSP a fait
- Répliquer les paramètres dans votre DAW
- Dépanner (si quelque chose ne sonne pas bien)

---

## Contrôle MIDI (optionnel)

Si vous disposez d'un contrôleur MIDI (clavier, mixeur, boutons), vous pouvez mapper des curseurs physiques aux paramètres MasterTweak.

### Configuration

1. Ouvrez **Paramètres → MIDI**.
2. Sélectionnez votre appareil MIDI dans la liste (par ex., « Akai APC40 », « Behringer FCB1010 »).
3. Cliquez sur **Apprendre** à côté de chaque paramètre.
4. Déplacez le curseur/bouton correspondant sur votre matériel.
5. MasterTweak enregistre le mappage.
6. Cliquez sur **Enregistrer**.

### Mappages

Une fois configuré, vous pouvez contrôler en temps réel lors de l'aperçu :

| Paramètre | Plage | Utilisation |
|-----------|-------|------------|
| Gain EQ (par bande) | −12 à +12 dB | Affinez le punch des graves, la présence, l'air |
| Seuil comp multibande (par bande) | −30 à 0 dB | Contrôlez quand la compression s'active |
| Lecteur de saturation | 0 à 6 dB | Ajoutez un caractère harmonique (simulation tape, tube) |
| Plafond du limiteur | −1 à −6 dBTP | Limite de loudness d'urgence |
| Largeur stéréo | 0 à 200% | Étroit (mono) à large (exagéré) |

---

## Tableau de référence des bandes de fréquence

MasterTweak utilise une séparation 7 bandes avec des fréquences de croisement fixes. Apprenez ce que chaque bande fait :

| Bande | Plage | Croisements | Objectif |
|-------|-------|-----------|---------|
| **Sub** | 20–80 Hz | — | Graves profonds, fondamentales du kick, rumble sub-bass. |
| **Lows** | 80–250 Hz | 80 Hz | Corps du kick, corps de la basse, chaleur. |
| **LowMids** | 250–500 Hz | 250 Hz | Zone boueuse, clarté de la basse, corps de la guitare/voix. |
| **Mids** | 500 Hz–2 kHz | 500 Hz | Présence vocale, clarté de l'instrument. |
| **HiMids** | 2–6 kHz | 2 kHz | Présence, sifflante, bord de l'instrument. |
| **Highs** | 6–16 kHz | 6 kHz | Brillance, hi-hat, cymbales, air. |
| **Air** | 16 kHz+ | 16 kHz | Ultra-aigus, pétillement, zone d'aliasing. |

**Quand booster/couper :**
- Boostez une bande si elle mesure trop faible (RMS bas) par rapport à la cible du preset
- Coupez une bande si elle mesure trop fort ou dure
- Utilisez des filtres cloche étroits (pas des étagères) pour un contrôle chirurgical
- Sub et Air fonctionnent souvent mieux comme des étagères (larges, lisses)

---

## Avancé : Étapes de contournement

Dans la CLI ou via des variables d'environnement, vous pouvez désactiver des étapes DSP entières pour le dépannage ou une utilisation créative :

```bash
./mastertweak --bypass-eq input.wav output.wav
./mastertweak --bypass-comp input.wav output.wav
./mastertweak --bypass-sat input.wav output.wav
./mastertweak --bypass-width input.wav output.wav
./mastertweak --bypass-limiter input.wav output.wav
```

**Cas d'usage :**
- `--bypass-eq` : Écoutez la réponse non-EQ ; déboguez si l'EQ du preset est mauvais.
- `--bypass-comp` : Écoutez les dynamiques non comprimées.
- `--bypass-sat` : Supprimez le caractère harmonique (rendez-le propre/stérile).
- `--bypass-width` : Réduisez à mono (vérifiez la compatibilité mono).
- `--bypass-limiter` : Laissez l'audio écrêter pour voir la sortie DSP brute (risqué).

---

## Mode CLI : Traitement par lots

Pour le mastering par lots ou les rendus sans surveillance, utilisez la CLI :

```bash
mastertweak --input song1.wav --output song1-mastered.wav --preset "Pop"
mastertweak --input song2.flac --output song2-mastered.flac --preset "Hip-Hop / Rap"
```

**Drapeaux courants :**
- `--preset <name>` — Utilisez un preset groupé
- `--output-format wav` ou `flac` — Par défaut est WAV
- `--bit-depth 16` ou `24` — Par défaut est 16
- `--target-platform spotify` — Plafond de loudness ; par défaut est « Streaming (Generic) »
- `--analyze-only` — Imprimez l'analyse, passez le rendu (utile pour le débogage)
- `--override-eq <band> <gain>` — Remplacez le gain EQ sur une seule bande (0–6)
- `--override-sat-drive <db>` — Remplacez le lecteur de saturation

**Exemple de script par lots :**
```bash
#!/bin/bash
for file in *.wav; do
  mastertweak --input "$file" --output "mastered/$file" --preset "Pop"
done
```

---

## Dépannage rapide

| Problème | Cause | Solution |
|---------|-------|----------|
| **Fichier ne s'ouvre pas** | Format non supporté ou fichier corrompu. | Confirmez que le fichier est WAV, FLAC ou AIFF. Essayez de convertir avec FFmpeg : `ffmpeg -i input.mp3 output.wav` |
| **Aucune forme d'onde** | Fichier décodé mais codec de lecture manquant. | Vérifiez les paramètres audio du système ; assurez-vous que PulseAudio ou ALSA fonctionne. |
| **Le rendu est très lent** | Fichier volumineux (>30 min) ou CPU lent. | Attendu pour les gros fichiers. Fermez les autres applications. La version Release est plus rapide que Debug. |
| **La sortie sonne distordue** | Le plafond du limiteur est trop bas pour le contenu. | Augmentez la plateforme cible ou augmentez manuellement le curseur du plafond du limiteur. |
| **La sortie est trop silencieuse** | La plateforme de loudness est trop conservatrice. | Réduisez la plateforme cible (par ex., de « Radiodiffusion » à « Streaming »). |
| **Crash lors du rendu** | Mémoire insuffisante ou fichier corrompu. | Libérez la RAM. Redémarrez l'application. Essayez un fichier test plus petit. |
| **Le lire A/B saute** | Sous-débordement du tampon du dispositif audio. | Augmentez la taille du tampon dans Paramètres → Audio. |

---

## Prochaines étapes

Ce guide couvre l'essentiel. Pour des explorations plus approfondies :

- **Manuel utilisateur complet** (`docs/user/manual_fr.md`) — Explications détaillées des paramètres, référence d'algorithme DSP, flux de travail de construction de preset
- **Guide du Preset Builder** (`docs/user/preset_builder_fr.md`) — Créez des presets personnalisés à partir de zéro en utilisant les outils Python de MasterTweak
- **Référence CLI** (`docs/user/cli_reference_fr.md`) — Tous les drapeaux de ligne de commande et exemples de scripts
- **FAQ** (`docs/user/faq_fr.md`) — Questions et réponses courantes

Bon mastering !

---

**MasterTweak Version 1.0** — Application autonome hors ligne de mastering automatique, piloté par des presets MixAdvice.  
Pour les rapports de bugs ou les demandes de fonctionnalités, consultez la page des issues du GitHub du projet.
