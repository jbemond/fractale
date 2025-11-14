/*
 * Plasma fractal ASCII + export des valeurs (Diamond-Square)
 * C ANSI C89 uniquement
 *
 * Options:
 *   -x, --width N          largeur (defaut 20)
 *   -y, --height N         hauteur (defaut 20)
 *   -s, --seed N           graine aleatoire (unsigned long)
 *   -a, --amplitude R      amplitude initiale (double)
 *   -k, --decay R          decroissance par niveau 0..1 (double)
 *   -f, --filter r,p       filtre box-blur rayon r, passes p
 *   -p, --palette CHARS    palette ASCII
 *   -g, --gamma R          correction gamma (double, defaut 1.0)
 *       --values           imprime la grille normalisee apres l'ASCII
 *       --only-values      n'imprime que la grille normalisee
 *   -h, --help             aide
 *
 * Compilation:
 *   cc -std=c89 -Wall -Wextra -O2 plasma.c -o plasma -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Valeurs par defaut */
#define DEFAULT_WIDTH   20
#define DEFAULT_HEIGHT  20
#define DEFAULT_PALETTE " .:-=+*#%@"

/* Limite pour eviter des allocations immenses */
#define MAX_CELLS 2000000L

/* Etat global des options */
static int width  = DEFAULT_WIDTH;
static int height = DEFAULT_HEIGHT;
static unsigned long SEED = 12345UL;
static double AMP   = 1.0;
static double DECAY = 0.6;
static int FILT_RADIUS = 0;
static int FILT_PASSES = 0;
static const char *PALETTE = DEFAULT_PALETTE;
static double GAMMA_CORR = 1.0;
static int PRINT_VALUES = 0;
static int ONLY_VALUES  = 0;

/* Aide */
static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -x, --width N          largeur\n"
        "  -y, --height N         hauteur\n"
        "  -s, --seed N           graine aleatoire\n"
        "  -a, --amplitude R      amplitude initiale (double)\n"
        "  -k, --decay R          decroissance par niveau 0..1 (double)\n"
        "  -f, --filter r,p       filtre box-blur rayon r, passes p\n"
        "  -p, --palette CHARS    palette ASCII\n"
        "  -g, --gamma R          correction gamma (double)\n"
        "      --values           imprimer aussi la grille normalisee\n"
        "      --only-values      imprimer uniquement la grille normalisee\n"
        "  -h, --help             cette aide\n", prog);
}

/* Parsing r,p pour -f */
static int parse_filter(const char *s, int *r, int *p) {
    const char *c = strchr(s, ',');
    char *e = 0;
    long rv, pv;
    if (!c) return -1;
    rv = strtol(s, &e, 10); if (e != c) return -1;
    pv = strtol(c + 1, &e, 10); if (*e != '\0') return -1;
    if (rv < 0) rv = 0;
    if (pv < 0) pv = 0;
    *r = (int)rv; *p = (int)pv;
    return 0;
}

/* Random double dans [0,1) */
static double frand01(void) {
    return (double)rand() / (double)(RAND_MAX);
}

/* Random double dans [-amp, +amp] */
static double frand_symmetric(double amp) {
    return (frand01() * 2.0 - 1.0) * amp;
}

/* Calcule le plus petit m = 2^n + 1 tel que m >= need */
static int pow2plus1_at_least(int need) {
    long side = 1; /* representera (m - 1) */
    long target = (need <= 1) ? 1 : (need - 1);
    while (side < target) side <<= 1;
    return (int)(side + 1);
}

/* Acces securise dans une grille carree n x n */
static double get_src(double *src, int n, int x, int y) {
    if (x < 0) { x = 0; }
    else if (x >= n) { x = n - 1; }

    if (y < 0) { y = 0; }
    else if (y >= n) { y = n - 1; }

    return src[y * n + x];
}

static void set_src(double *src, int n, int x, int y, double v) {
    src[y * n + x] = v;
}

/* Diamond-Square sur une grille n x n, n = 2^k + 1 */
static void diamond_square(double *src, int n, double amp, double decay) {
    int step = n - 1;
    int half;
    double scale = amp;

    /* coins */
    set_src(src, n, 0,     0,     frand_symmetric(scale));
    set_src(src, n, step,  0,     frand_symmetric(scale));
    set_src(src, n, 0,     step,  frand_symmetric(scale));
    set_src(src, n, step,  step,  frand_symmetric(scale));

    while (step > 1) {
        int x, y;
        half = step / 2;

        /* Diamond */
        for (y = half; y < n; y += step) {
            for (x = half; x < n; x += step) {
                double a = get_src(src, n, x - half, y - half);
                double b = get_src(src, n, x + half, y - half);
                double c = get_src(src, n, x - half, y + half);
                double d = get_src(src, n, x + half, y + half);
                double avg = (a + b + c + d) * 0.25;
                double off = frand_symmetric(scale);
                set_src(src, n, x, y, avg + off);
            }
        }

        /* Square */
        for (y = 0; y < n; y += half) {
            int xstart = ((y / half) % 2) ? 0 : half;
            for (x = xstart; x < n; x += step) {
                double sum = 0.0;
                int cnt = 0;
                if (y - half >= 0) { sum += get_src(src, n, x, y - half); cnt++; }
                if (y + half < n)  { sum += get_src(src, n, x, y + half); cnt++; }
                if (x - half >= 0) { sum += get_src(src, n, x - half, y); cnt++; }
                if (x + half < n)  { sum += get_src(src, n, x + half, y); cnt++; }
                if (cnt > 0) {
                    double avg = sum / (double)cnt;
                    double off = frand_symmetric(scale);
                    set_src(src, n, x, y, avg + off);
                } else {
                    set_src(src, n, x, y, frand_symmetric(scale));
                }
            }
        }

        step = half;
        scale *= decay;
    }
}

/* Bilinear sample src[n x n] vers dst[W x H] */
static void resample_bilinear(double *src, int n, double *dst, int W, int H) {
    int y, x;
    double denomX = (W > 1) ? (double)(W - 1) : 1.0;
    double denomY = (H > 1) ? (double)(H - 1) : 1.0;
    for (y = 0; y < H; ++y) {
        double v = ((double)y) * (double)(n - 1) / denomY;
        int v0 = (int)floor(v);
        int v1 = v0 + 1; if (v1 >= n) v1 = n - 1;
        double fy = v - (double)v0;
        for (x = 0; x < W; ++x) {
            double u = ((double)x) * (double)(n - 1) / denomX;
            int u0 = (int)floor(u);
            int u1 = u0 + 1; if (u1 >= n) u1 = n - 1;
            double fx = u - (double)u0;

            double p00 = get_src(src, n, u0, v0);
            double p10 = get_src(src, n, u1, v0);
            double p01 = get_src(src, n, u0, v1);
            double p11 = get_src(src, n, u1, v1);

            double a = p00 * (1.0 - fx) + p10 * fx;
            double b = p01 * (1.0 - fx) + p11 * fx;
            dst[y * W + x] = a * (1.0 - fy) + b * fy;
        }
    }
}

/* Box blur rayon r, p passes, resultat final garanti dans 'grid' */
static void box_blur(double *grid, int W, int H, int r, int p) {
    int pass, y, x, dy, dx;
    if (r <= 0 || p <= 0) return;

    {
        double *tmp = (double*)malloc((size_t)W * (size_t)H * sizeof(double));
        if (!tmp) return;

        for (pass = 0; pass < p; ++pass) {
            double *src = (pass % 2 == 0) ? grid : tmp;
            double *dst = (pass % 2 == 0) ? tmp  : grid;

            for (y = 0; y < H; ++y) {
                for (x = 0; x < W; ++x) {
                    double sum = 0.0;
                    int cnt = 0;
                    for (dy = -r; dy <= r; ++dy) {
                        int yy = y + dy; if (yy < 0) yy = 0; if (yy >= H) yy = H - 1;
                        for (dx = -r; dx <= r; ++dx) {
                            int xx = x + dx; if (xx < 0) xx = 0; if (xx >= W) xx = W - 1;
                            sum += src[yy * W + xx];
                            cnt++;
                        }
                    }
                    dst[y * W + x] = sum / (double)cnt;
                }
            }
        }

        /* Si p est impair, le dernier ecrit a ete fait dans tmp -> recopier vers grid */
        if ((p % 2) == 1) {
            int i2, N = W * H;
            for (i2 = 0; i2 < N; ++i2) grid[i2] = tmp[i2];
        }

        free(tmp);
    }
}

/* Normalisation vers [0,1] */
static void normalize01(double *grid, int N) {
    int i;
    double mn = grid[0], mx = grid[0];
    for (i = 1; i < N; ++i) {
        if (grid[i] < mn) mn = grid[i];
        if (grid[i] > mx) mx = grid[i];
    }
    if (mx - mn <= 1e-12) {
        for (i = 0; i < N; ++i) grid[i] = 0.5;
        return;
    }
    for (i = 0; i < N; ++i) grid[i] = (grid[i] - mn) / (mx - mn);
}

/* Applique gamma > 0 */
static void apply_gamma(double *grid, int N, double gamma) {
    int i;
    if (gamma <= 0.0 || fabs(gamma - 1.0) < 1e-12) return;
    for (i = 0; i < N; ++i) {
        double v = grid[i];
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        grid[i] = pow(v, 1.0 / gamma);
    }
}

/* Impression ASCII selon palette */
static void print_ascii(double *grid, int W, int H, const char *palette) {
    int y, x, plen = 0;
    while (palette[plen] != '\0') plen++;
    if (plen < 1) { palette = DEFAULT_PALETTE; plen = 10; }

    for (y = 0; y < H; ++y) {
        for (x = 0; x < W; ++x) {
            double v = grid[y * W + x];
            int idx = (int)(v * (double)(plen - 1) + 0.5);
            if (idx < 0) idx = 0;
            if (idx >= plen) idx = plen - 1;
            putchar(palette[idx]);
        }
        putchar('\n');
    }
}

/* Impression des valeurs normalisees 0..1 */
static void print_values(double *grid, int W, int H) {
    int y, x;
    for (y = 0; y < H; ++y) {
        for (x = 0; x < W; ++x) {
            double v = grid[y * W + x];
            printf("%.6f", v);
            if (x + 1 < W) putchar(' ');
        }
        putchar('\n');
    }
}

int main(int argc, char **argv) {
    int i;

    /* Parsing simple */
    for (i = 1; i < argc; ) {
        const char *a = argv[i];

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]); return 0;

        } else if ((strcmp(a, "-x") == 0 || strcmp(a, "--width") == 0) && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v <= 0) { print_usage(argv[0]); return 1; }
            width = (int)v; i += 2; continue;

        } else if ((strcmp(a, "-y") == 0 || strcmp(a, "--height") == 0) && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v <= 0) { print_usage(argv[0]); return 1; }
            height = (int)v; i += 2; continue;

        } else if ((strcmp(a, "-s") == 0 || strcmp(a, "--seed") == 0) && i + 1 < argc) {
            char *e = 0; unsigned long v = strtoul(argv[i+1], &e, 10);
            if (*e != '\0') { print_usage(argv[0]); return 1; }
            SEED = v; i += 2; continue;

        } else if ((strcmp(a, "-a") == 0 || strcmp(a, "--amplitude") == 0) && i + 1 < argc) {
            char *e = 0; double v = strtod(argv[i+1], &e);
            if (*e != '\0') { print_usage(argv[0]); return 1; }
            AMP = v; i += 2; continue;

        } else if ((strcmp(a, "-k") == 0 || strcmp(a, "--decay") == 0) && i + 1 < argc) {
            char *e = 0; double v = strtod(argv[i+1], &e);
            if (*e != '\0' || v < 0.0) { print_usage(argv[0]); return 1; }
            DECAY = v; i += 2; continue;

        } else if ((strcmp(a, "-f") == 0 || strcmp(a, "--filter") == 0) && i + 1 < argc) {
            if (parse_filter(argv[i+1], &FILT_RADIUS, &FILT_PASSES) != 0) {
                fprintf(stderr, "Filtre invalide, utiliser r,p par exemple -f 1,2\n");
                return 1;
            }
            i += 2; continue;

        } else if ((strcmp(a, "-p") == 0 || strcmp(a, "--palette") == 0) && i + 1 < argc) {
            PALETTE = argv[i+1]; i += 2; continue;

        } else if ((strcmp(a, "-g") == 0 || strcmp(a, "--gamma") == 0) && i + 1 < argc) {
            char *e = 0; double v = strtod(argv[i+1], &e);
            if (*e != '\0' || v <= 0.0) { print_usage(argv[0]); return 1; }
            GAMMA_CORR = v; i += 2; continue;

        } else if (strcmp(a, "--values") == 0) {
            PRINT_VALUES = 1; i += 1; continue;

        } else if (strcmp(a, "--only-values") == 0) {
            ONLY_VALUES = 1; i += 1; continue;

        } else {
            print_usage(argv[0]); return 1;
        }
    }

    /* Checks de taille et allocations */
    {
        long total = (long)width * (long)height;
        if (total <= 0 || total > MAX_CELLS) {
            fprintf(stderr, "Taille invalide ou trop grande.\n");
            return 1;
        }
    }

    srand((unsigned)SEED);

    /* Grille source carree n x n pour DS */
    {
        int need = (width > height) ? width : height;
        int n = pow2plus1_at_least(need);
        double *src = (double*)malloc((size_t)n * (size_t)n * sizeof(double));
        double *dst = (double*)malloc((size_t)width * (size_t)height * sizeof(double));
        if (!src || !dst) {
            fprintf(stderr, "Allocation memoire impossible.\n");
            if (src) free(src);
            if (dst) free(dst);
            return 1;
        }

        diamond_square(src, n, AMP, DECAY);
        resample_bilinear(src, n, dst, width, height);

        if (FILT_RADIUS > 0 && FILT_PASSES > 0) {
            box_blur(dst, width, height, FILT_RADIUS, FILT_PASSES);
        }

        normalize01(dst, width * height);
        apply_gamma(dst, width * height, GAMMA_CORR);

        if (!ONLY_VALUES) {
            print_ascii(dst, width, height, PALETTE);
        }
        if (PRINT_VALUES || ONLY_VALUES) {
            if (!ONLY_VALUES) putchar('\n');
            print_values(dst, width, height);
        }

        free(src);
        free(dst);
    }

    return 0;
}
