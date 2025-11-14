/*
 * Fractale ASCII 20x20 (triangle de Sierpinski via "chaos game")
 * C ANSI (C89) uniquement : stdio.h, stdlib.h, string.h
 *
 * Paramètres via la ligne de commande :
 *   -s, --seed N          : graine (entier non signé)
 *   -n, --iter N          : itérations (entier)
 *   -r, --ratio a/b       : ratio de rapprochement (entiers a et b, b>0)
 *   -w, --weights a,b,c   : poids des sommets (entiers >=0)
 *   -u, --warmup N        : itérations ignorées au début
 *   -p, --palette "chars" : jeu de caractères pour la densité
 *   -h, --help            : afficher l'aide
 *
 * Compilation :
 *   cc -std=c89 -Wall -Wextra -O2 fractale.c -o fractale
 * Exemples :
 *   ./fractale -s 42 -n 8000 -r 1/2 -w 3,1,1 -u 20 -p " .:-=+*#%@"
 *   ./fractale --seed 2025 --iter 6000 --ratio 2/3 --weights 1,1,5
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Taille fixe demandée : 20x20 */
#define WIDTH  20
#define HEIGHT 20

/* Valeurs par défaut */
static unsigned long SEED       = 12345UL;
static long          ITERATIONS = 5000L;
static int           RATIO_NUM  = 1;
static int           RATIO_DEN  = 2;
static int           W0 = 1, W1 = 1, W2 = 1;
static int           WARMUP     = 10;
static const char   *PALETTE    = " .:-=+*#%@";

/* Affiche l'aide et quitte. */
static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  -s, --seed N          graine aleatoire (unsigned long)\n"
        "  -n, --iter N          nombre d'iterations (long)\n"
        "  -r, --ratio a/b       ratio de rapprochement vers le sommet\n"
        "  -w, --weights a,b,c   poids des 3 sommets\n"
        "  -u, --warmup N        iterations ignorees au debut\n"
        "  -p, --palette CHARS   jeu de caracteres pour la densite\n"
        "  -h, --help            affiche cette aide\n",
        prog
    );
}

/* Parse a/b dans *num et *den. Retourne 0 si OK, -1 sinon. */
static int parse_ratio(const char *s, int *num, int *den) {
    const char *slash = strchr(s, '/');
    char *endptr;
    long a, b;

    if (!slash) return -1;
    a = strtol(s, &endptr, 10);
    if (endptr != slash) return -1;
    b = strtol(slash + 1, &endptr, 10);
    if (*endptr != '\0') return -1;
    if (b == 0) return -1;

    *num = (int)a;
    *den = (int)b;
    return 0;
}

/* Parse a,b,c dans *w0,*w1,*w2. Retourne 0 si OK, -1 sinon. */
static int parse_weights(const char *s, int *w0, int *w1, int *w2) {
    const char *c1 = strchr(s, ',');
    const char *c2;
    char *endptr;
    long a, b, c;

    if (!c1) return -1;
    c2 = strchr(c1 + 1, ',');
    if (!c2) return -1;

    a = strtol(s, &endptr, 10);
    if (endptr != c1) return -1;

    b = strtol(c1 + 1, &endptr, 10);
    if (endptr != c2) return -1;

    c = strtol(c2 + 1, &endptr, 10);
    if (*endptr != '\0') return -1;

    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (c < 0) c = 0;

    *w0 = (int)a; *w1 = (int)b; *w2 = (int)c;
    return 0;
}

/* Sélectionne un sommet 0,1,2 selon des poids entiers positifs. */
static int choose_vertex(int w0, int w1, int w2) {
    int sum = w0 + w1 + w2;
    int r, t;

    if (sum <= 0) sum = 1;
    r = rand() % sum;
    t = w0;
    if (r < t) return 0;
    t += w1;
    if (r < t) return 1;
    return 2;
}

int main(int argc, char **argv) {
    /* Grille de compteurs pour mapping densite -> palette */
    int hits[HEIGHT][WIDTH];
    int x, y, vx[3], vy[3];
    long i;
    int row, col;
    int maxhit = 0;
    const char *pal = PALETTE;
    int pal_len = 0;

    /* Parsing simple de argv pour rester ANSI */
    {
        int idx = 1;
        while (idx < argc) {
            const char *a = argv[idx];

            if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
                print_usage(argv[0]);
                return 0;
            } else if ((strcmp(a, "-s") == 0 || strcmp(a, "--seed") == 0) && (idx + 1 < argc)) {
                char *e = 0;
                unsigned long v = strtoul(argv[idx + 1], &e, 10);
                if (*e != '\0') { print_usage(argv[0]); return 1; }
                SEED = v; idx += 2; continue;
            } else if ((strcmp(a, "-n") == 0 || strcmp(a, "--iter") == 0) && (idx + 1 < argc)) {
                char *e = 0; long v = strtol(argv[idx + 1], &e, 10);
                if (*e != '\0' || v < 0) { print_usage(argv[0]); return 1; }
                ITERATIONS = v; idx += 2; continue;
            } else if ((strcmp(a, "-r") == 0 || strcmp(a, "--ratio") == 0) && (idx + 1 < argc)) {
                if (parse_ratio(argv[idx + 1], &RATIO_NUM, &RATIO_DEN) != 0 || RATIO_DEN == 0) {
                    fprintf(stderr, "Ratio invalide. Utiliser a/b avec b>0.\n");
                    return 1;
                }
                idx += 2; continue;
            } else if ((strcmp(a, "-w") == 0 || strcmp(a, "--weights") == 0) && (idx + 1 < argc)) {
                if (parse_weights(argv[idx + 1], &W0, &W1, &W2) != 0) {
                    fprintf(stderr, "Weights invalides. Utiliser a,b,c.\n");
                    return 1;
                }
                idx += 2; continue;
            } else if ((strcmp(a, "-u") == 0 || strcmp(a, "--warmup") == 0) && (idx + 1 < argc)) {
                char *e = 0; long v = strtol(argv[idx + 1], &e, 10);
                if (*e != '\0' || v < 0) { print_usage(argv[0]); return 1; }
                WARMUP = (int)v; idx += 2; continue;
            } else if ((strcmp(a, "-p") == 0 || strcmp(a, "--palette") == 0) && (idx + 1 < argc)) {
                pal = argv[idx + 1];
                idx += 2; continue;
            } else {
                /* Argument inconnu ou valeur manquante */
                print_usage(argv[0]);
                return 1;
            }
        }
    }

    /* Longueur de la palette */
    while (pal[pal_len] != '\0') pal_len++;
    if (pal_len <= 0) {
        pal = " .:-=+*#%@";
        pal_len = 10;
    }

    /* Reset des compteurs */
    for (row = 0; row < HEIGHT; ++row)
        for (col = 0; col < WIDTH; ++col)
            hits[row][col] = 0;

    /* Sommets du triangle dans la grille 20x20 */
    vx[0] = 0;           vy[0] = HEIGHT - 1;   /* bas-gauche  */
    vx[1] = WIDTH - 1;   vy[1] = HEIGHT - 1;   /* bas-droit   */
    vx[2] = WIDTH / 2;   vy[2] = 0;            /* haut-centre */

    /* Graine contrôlée */
    srand((unsigned)SEED);

    /* Point de départ */
    x = WIDTH / 2;
    y = HEIGHT / 2;

    /* Boucle principale */
    for (i = 0; i < ITERATIONS; ++i) {
        int v = choose_vertex(W0, W1, W2);

        /* Rapprochement vers le sommet choisi, en entier */
        x = x + ((vx[v] - x) * RATIO_NUM) / RATIO_DEN;
        y = y + ((vy[v] - y) * RATIO_NUM) / RATIO_DEN;

        if (i >= WARMUP) {
            if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
                hits[y][x]++;
                if (hits[y][x] > maxhit) maxhit = hits[y][x];
            }
        }
    }

    /* Rendu ASCII selon densité */
    for (row = 0; row < HEIGHT; ++row) {
        for (col = 0; col < WIDTH; ++col) {
            char c;
            int h = hits[row][col];
            if (h <= 0) {
                c = pal[0];
            } else if (maxhit <= 0 || pal_len <= 1) {
                c = '#';
            } else {
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
