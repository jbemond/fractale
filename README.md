Voici deux programmes qui résultent de l’agrégation d’une multitude de petites routines. J’ai développé ces outils dans ma jeunesse, à une époque où je travaillais dans le monde du jeu vidéo. Ils ont beaucoup évolué au fil du temps, tant sur le plan technique qu’en termes de langage de programmation.

Les premières versions ont été écrites sur Amstrad CPC, en BASIC puis en assembleur Z80, vers 1986. Elles ont ensuite été réécrites, toujours sur la même machine, en Turbo Pascal 3.01A pour CP/M, puis portées sur PC en Turbo Pascal versions 3 et 4, avec de l’assembleur pour MS-DOS vers 1988-1989. Enfin, une nouvelle génération a été développée en C ANSI (Turbo C et QuickC) vers 1989-1990.

Les codes présentés ici ont été testés et compilés sans erreur avec gcc 13.3 sous Linux Debian 13 et avec Code::Blocks 25.03 sous Windows 10.


# Fractale « plasma » et rendu isométrique — C ANSI C89

Ce dépôt contient deux programmes en **C ANSI C89 pur**, sans dépendances externes :
- `plasma.c` : génère une fractale de type « plasma » (Diamond–Square). Sorties possibles : ASCII dans le terminal ou **heightmap** normalisée 0..1 pour un rendu 3D.
- `iso.c` : lit une heightmap 0..1 et produit une image **PPM** en vue isométrique (colonnes extrudées), avec occlusion gérée par ordre de peinture.

---

## 1) Compilation

Compiler avec un compilateur C standard :

```sh
cc -std=c89 -Wall -Wextra -O2 plasma.c -o plasma -lm
cc -std=c89 -Wall -Wextra -O2 iso.c    -o iso
```

> Remarque : pour `plasma.c`, l’édition de liens avec `-lm` est indispensable (fonctions `floor`, `pow`).

---

## 2) Générer un **plasma** en ASCII (aperçu)

Commande minimale (20×20, valeurs par défaut) :

```sh
./plasma
```

Exemple plus lisible (60×30, léger lissage, gamma 1,2) :

```sh
./plasma -x 60 -y 30 -s 42 -a 1.0 -k 0.6 -f 1,2 -g 1.2
```

Explications rapides :
- Plus **`-a`** (amplitude) est grande et **`-k`** (decay) proche de 1, plus la surface est rugueuse et détaillée.
- Le filtre **`-f rayon,passes`** adoucit la surface après génération.
- La correction **`-g gamma`** affine le contraste de l’ASCII. La heightmap exportée n’est pas affectée par la palette ASCII.

---

## 3) Obtenir la **carte d’altitude** (heightmap 0..1)

Pour produire une grille de `y` lignes contenant chacune `x` valeurs flottantes dans **[0,1]** :

```sh
./plasma -x 64 -y 48 --only-values > hmap.txt
```

Remarques :
- `--only-values` n’imprime **que** les valeurs, sans ASCII.
- `--values` imprime d’abord l’ASCII, puis une ligne vide, puis la grille.
- Le format est directement lisible par `iso` (un `double` par cellule, séparé par des espaces).

Astuces qualité pour heightmap :
- Lissage : `-f 1,2` ou `-f 2,1` adoucit sans trop lisser les détails.
- Réglage global : `-a 1.0 -k 0.6` est un bon point de départ.
- Reproductibilité : changez **`-s`** (graine) pour de nouvelles variantes, et **conservez-la** pour régénérer exactement la même heightmap plus tard.

---

## 4) Options détaillées de `plasma`

```
-x, --width N        Largeur de la grille (défaut 20)
-y, --height N       Hauteur de la grille (défaut 20)
-s, --seed N         Graine aléatoire (unsigned long, reproductible)
-a, --amplitude R    Amplitude initiale des décalages (double, ex. 1.0)
-k, --decay R        Décroissance par niveau (double, 0..1, ex. 0.6)
-f, --filter r,p     Lissage box-blur, rayon r, passé p fois (ex. -f 1,2)
-p, --palette CHARS  Chaîne pour le rendu ASCII du plus clair au plus dense
-g, --gamma R        Correction gamma (> 0, ex. 1.2)
    --values         Imprime aussi la grille normalisée (après l’ASCII)
    --only-values    N’imprime que la grille normalisée (sans ASCII)
-h, --help           Aide
```

Notes importantes :
- **Taille interne** : l’algorithme génère une grille carrée de taille `2^n + 1` couvrant au moins la zone demandée, puis **rééchantillonne** vers `x × y`.
- **Normalisation** : les valeurs sont ramenées dans `[0,1]` avant export.
- **Limites** : un garde‑fou empêche des allocations gigantesques.

Exemples utiles :
```sh
# Relief doux, grand format
./plasma -x 200 -y 120 -s 7  -a 0.8 -k 0.55 -f 2,2 --only-values > hmap.txt

# Relief très fracturé
./plasma -x 200 -y 120 -s 99 -a 1.5 -k 0.85           --only-values > hmap.txt
```

---

## 5) Rendu 3D **isométrique** avec `iso.c`

`iso` lit une heightmap 0..1 (format produit par `plasma --only-values`) et crée une image **PPM** (P6) ombrée en niveaux de gris. L’occlusion est gérée en peignant du fond vers l’avant (ordre `x + y`).

Commande de base :

```sh
./iso -x 64 -y 48 -i hmap.txt -o iso.ppm -tw 16 -th 8 -zs 80
```

Visualiser le PPM :
```sh
eog iso.ppm
# ou convertir en PNG si ImageMagick est installé
convert iso.ppm iso.png
```

### Options `iso`

```
-x N             Largeur de la grille (doit correspondre aux données)
-y N             Hauteur de la grille
-i PATH          Fichier d’entrée (sinon stdin)
-o PATH          Fichier PPM de sortie (défaut iso.ppm)
-tw N            Largeur des tuiles isométriques (défaut 16)
-th N            Hauteur des tuiles isométriques (défaut 8)
-zs N            Échelle verticale, hauteur max des colonnes (défaut 64)
-bg r,g,b        Couleur de fond 0..255,0..255,0..255 (défaut 16,16,24)
```

Recommandations :
- Ratio classique : `-tw 16 -th 8`. Augmentez `-tw` pour un effet plus étalé.
- Ajustez `-zs` pour l’amplitude verticale perçue.
- Un léger lissage côté `plasma` (`-f 1,2`) réduit l’aliasing et donne un rendu plus organique.

---

## 6) Pipelines et exemples rapides

### Pipeline direct sans fichier intermédiaire
```sh
./plasma -x 96 -y 72 -s 99 -a 1.0 -k 0.65 -f 1,2 --only-values \
| ./iso -x 96 -y 72 -i - -o iso.ppm -tw 16 -th 8 -zs 90
```

### Fond plus sombre et colonnes plus hautes
```sh
./iso -x 64 -y 48 -i hmap.txt -o iso.ppm -tw 18 -th 9 -zs 120 -bg 8,8,12
```

---

## 7) Qualité, astuces et dépannage

- **Reproductibilité** : gardez la graine (`-s`) et les paramètres utilisés pour régénérer exactement la même heightmap et le même rendu.
- **Lissage** : `-f 1,2` avant export aide à réduire l’aliasing pour le rendu isométrique.
- **Contraste ASCII** : jouez sur `-g` (gamma) et `-p` (palette). La heightmap exportée est indépendante de la palette ASCII.
- **Performances** : attention aux très grandes tailles ; ces programmes sont mono‑thread.
- **PPM** : format simple non compressé ; convertissez ensuite en PNG/JPEG si nécessaire.

Erreurs fréquentes :
- « référence indéfinie vers `floor`/`pow` » → recompiler `plasma` avec `-lm`.
- « Fichier trop court ou invalide » dans `iso` → la grille ne contient pas assez de valeurs ou les dimensions `-x/-y` ne correspondent pas. Vérifiez `x`, `y` et `hmap.txt`.
- Rendu noir ou très plat → heightmap quasi constante. Augmentez `-a`, rapprochez `-k` de 1, ou réduisez le lissage `-f`.

---

## 8) Résumé commandes clés

```sh
# ASCII rapide
./plasma -x 80 -y 40 -s 12 -a 1.0 -k 0.6 -f 1,1 -g 1.2

# Heightmap prête pour iso
./plasma -x 128 -y 96 -s 2025 -a 1.2 -k 0.7 -f 2,2 --only-values > hmap.txt

# Rendu isométrique
./iso -x 128 -y 96 -i hmap.txt -o iso.ppm -tw 20 -th 10 -zs 120 -bg 12,12,18
```

<<<<<<< HEAD
Bon rendu !
=======
## 9) Générateur de relief géographique avec niveau d’eau (`geo.c`)

`geo.c` produit une heightmap 0..1 de type Diamond–Square puis applique une montée des eaux paramétrable. Deux modes sont disponibles : inondation depuis les bords (océan et baies seulement) ou remplissage de toutes les dépressions inférieures au niveau choisi (lacs compris). Le programme peut sortir une image **PPM** colorée ou une grille de valeurs utilisable par `iso`.

### Compilation

```sh
cc -std=c89 -Wall -Wextra -O2 geo.c -o geo -lm
```

### Principes

- Génération d’une grille interne de taille `2^n + 1`, puis rééchantillonnage bilinéaire vers `x × y`.
- Lissage optionnel par flou 3×3 répété `p` fois.
- Eau :
  - `--from-edge` : l’eau « entre » par les bords et progresse seulement dans les cellules `≤ niveau` connectées (océan, estuaires, baies).
  - `--fill-all` : toutes les cellules `≤ niveau` deviennent eau, même sans connexion aux bords (lacs fermés).
  - `--seed x,y` : point de départ supplémentaire pour inonder une zone interne.
- Sorties :
  - valeurs texte 0..1 sur `stdout` (par défaut) ;
  - PPM couleur avec `-o map.ppm` ;
  - `--values-with-water` remplace la hauteur des cellules d’eau par le niveau choisi, pratique pour une extrusion régulière dans `iso`.

### Options

```
-x N                largeur de la carte (défaut 64)
-y N                hauteur de la carte (défaut 48)
-s N                graine RNG (entier non signé)
-a R                amplitude initiale (défaut 1.0)
-k R                rugosité 0..1, plus proche de 1 ⇒ plus de détails (défaut 0.65)
-f N                passes d’adoucissement 3×3 (entier, défaut 0)

--sea R             active l’eau au niveau R (0..1)
--from-edge         inonde depuis les bords (comportement conseillé pour l’océan)
--fill-all          marque en eau toute cellule ≤ niveau, sans connectivité
--seed x,y          ajoute une source d’inondation interne
--values-with-water imprime la heightmap « remplie » (eau = niveau constant)

-o PATH             écrit un PPM couleur (défaut map.ppm si fourni)
--no-values         n’imprime pas la grille texte
-h                  aide
```

### Exemples

Carte couleur avec océan au niveau 0,45, légère douceur :
```sh
./geo -x 256 -y 192 -s 42 -a 1 -k 0.65 -f 2 --sea 0.45 --from-edge -o map.ppm
```

Heightmap pour `iso`, avec zones d’eau aplaties au niveau choisi :
```sh
./geo -x 128 -y 96 -s 42 -a 1 -k 0.65 -f 1 --sea 0.50 --from-edge --values-with-water > hmap.txt
./iso -x 128 -y 96 -i hmap.txt -o iso.ppm -tw 16 -th 8 -zs 80
```

Remplir toutes les dépressions (incluant les lacs fermés) :
```sh
./geo -x 200 -y 150 -s 123 --sea 0.40 --fill-all -o map.ppm
```

Pipeline direct sans fichier intermédiaire :
```sh
./geo -x 96 -y 72 -s 7 -a 1 -k 0.65 -f 1 --sea 0.48 --from-edge --values-with-water \
| ./iso -x 96 -y 72 -i - -o iso.ppm -tw 16 -th 8 -zs 90
```

### Palette et rendu

- Eau : dégradé bleu plus sombre avec la profondeur.
- Terre : sable, vert, roche, neige selon l’altitude.
- Les frontières terre / eau sont légèrement assombries pour marquer les côtes.

### Conseils

- Pour un rendu isométrique plus propre, utiliser un lissage modéré : `-f 1` ou `-f 2`.
- Pour davantage de relief, augmenter `-a` et rapprocher `-k` de 1.
- Conserver la graine `-s` et les paramètres pour une reproductibilité parfaite.

>>>>>>> 419d005 (sync total)
