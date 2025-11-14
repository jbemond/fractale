/*
 * Fractale ASCII 20x20 (triangle de Sierpinski via "chaos game")
 * C ANSI (C89) uniquement : stdio.h et stdlib.h, pas de VLA ni d'extensions.
 *
 * Paramètres contrôlant l'aléatoire :
 *  - SEED        : graine pour srand, pour rendre la sortie reproductible
 *  - ITERATIONS  : nombre d'itérations du chaos game
 *  - RATIO_NUM / RATIO_DEN : fraction de rapprochement vers le sommet choisi
 *  - W0, W1, W2  : poids de choix des sommets (biais de probabilités)
 *
 * Compilation :
 *   cc -std=c89 -Wall -Wextra -O2 fractale.c -o fractale
 * Exécution :
 *   ./fractale
 */

#include <stdio.h>
#include <stdlib.h>

/* Taille demandée : 20x20 */
#define WIDTH  20
#define HEIGHT 20

/* ====== Paramètres contrôlables ====== */
static unsigned long SEED       = 12345UL; /* graine pour srand */
static long          ITERATIONS = 5000L;   /* plus c'est grand, plus c'est net */
static int           RATIO_NUM  = 1;       /* numerateur du ratio (ex. 1) */
static int           RATIO_DEN  = 2;       /* denominateur du ratio (ex. 2 => milieu) */
static int           W0 = 1, W1 = 1, W2 = 1; /* poids des sommets (1,1,1 = équitable) */
static int           WARMUP     = 10;      /* itérations ignorées au début */

/* Jeu de caractères pour densité (du plus clair au plus foncé) */
static const char *PALETTE = " .:-=+*#%@";

/* Sélectionne un sommet 0, 1 ou 2 selon des poids entiers positifs */
static int choose_vertex(int w0, int w1, int w2) {
    int sum = w0 + w1 + w2;
    int r, t;

    if (sum <= 0) sum = 1; /* sécurité minimale */
    r = rand() % sum;
    t = w0;
    if (r < t) return 0;
    t += w1;
    if (r < t) return 1;
    return 2;
}

int main(void) {
    /* Compteur de touches par cellule pour une palette plus lisible */
    int hits[HEIGHT][WIDTH];
    int x, y, vx[3], vy[3];
    long i;
    int row, col;
    int maxhit = 0;
    const char *pal = PALETTE;
    int pal_len = 0;

    /* Longueur de la palette */
    while (pal[pal_len] != '\0') pal_len++;

    /* Initialisation de la grille de compteurs */
    for (row = 0; row < HEIGHT; ++row) {
        for (col = 0; col < WIDTH; ++col) {
            hits[row][col] = 0;
        }
    }

    /* Définition des 3 sommets du triangle dans la grille 20x20 */
    /* Bas-gauche, bas-droit, haut-centre */
    vx[0] = 0;           vy[0] = HEIGHT - 1;
    vx[1] = WIDTH - 1;   vy[1] = HEIGHT - 1;
    vx[2] = WIDTH / 2;   vy[2] = 0;

    /* Graine pour l'aléatoire contrôlé */
    srand((unsigned)SEED);

    /* Point de départ au centre approximatif */
    x = WIDTH / 2;
    y = HEIGHT / 2;

    /* Boucle principale du chaos game */
    for (i = 0; i < ITERATIONS; ++i) {
        int v = choose_vertex(W0, W1, W2);

        /* Rapprochement rationnel vers le sommet choisi, en entier */
        x = x + ( (vx[v] - x) * RATIO_NUM ) / RATIO_DEN;
        y = y + ( (vy[v] - y) * RATIO_NUM ) / RATIO_DEN;

        /* On laisse "chauffer" quelques itérations avant de tracer */
        if (i >= WARMUP) {
            if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
                hits[y][x]++;
                if (hits[y][x] > maxhit) maxhit = hits[y][x];
            }
        }
    }

    /* Rendu ASCII avec palette en fonction de la densité */
    for (row = 0; row < HEIGHT; ++row) {
        for (col = 0; col < WIDTH; ++col) {
            char c;
            int h = hits[row][col];
            if (h <= 0) {
                c = pal[0];
            } else if (maxhit <= 0 || pal_len <= 1) {
                c = '#';
            } else {
                /* Échelle linéaire des compteurs vers la palette */
                int idx = (h * (pal_len - 1)) / maxhit;
                if (idx < 0) idx = 0;
                if (idx >= pal_len) idx = pal_len - 1;
                c = pal[idx];
            }
            putchar(c);
        }
        putchar('\n');
    }

    return 0;
}
