/*
 * geo.c — Generateur de relief (type "plasma"/diamond-square) avec simulation de niveau d'eau.
 * C ANSI C89, aucune dependance externe.
 *
 * Fonctionnalites :
 *  - Generation d'une heightmap 0..1 (diamond-square) resamplee en WxH arbitraire
 *  - Adoucissement optionnel (flou boite 3x3, passes multiples)
 *  - Simulation d'eau : seuil "sea level" et remplissage par inondation
 *       * --from-edge : l'eau envahit depuis les bords, uniquement les cellules <= niveau
 *       * --fill-all  : toute cellule <= niveau devient eau (sans contrainte de connectivite)
 *       * --seed x,y  : point de depart optionnel pour l'inondation (ajoute aux bords si --from-edge)
 *  - Sortie : PPM couleur (--out map.ppm) et/ou valeurs texte (--only-values)
 *  - Option --values-with-water : imprime la heightmap "remplie" (h' = max(h, sea_level) pour les cellules eau)
 *
 * Compilation :
 *   cc -std=c89 -Wall -Wextra -O2 geo.c -o geo -lm
 *
 * Exemples :
 *   # Carte couleur avec ocean depuis les bords au niveau 0.45
 *   ./geo -x 256 -y 192 -s 42 -a 1 -k 0.65 -f 2 --sea 0.45 --from-edge -o map.ppm
 *
 *   # Heightmap pour iso.c, avec eau transformee en plateau a niveau constant
 *   ./geo -x 128 -y 96 -s 42 -a 1 -k 0.65 -f 1 --sea 0.5 --from-edge --values-with-water > hmap.txt
 *
 *   # Remplir toutes les depressions <= niveau (lacs internes compris), sans contrainte de connexion
 *   ./geo -x 200 -y 150 -s 123 --sea 0.40 --fill-all -o map.ppm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* --------- Parametres ---------- */
static int OUT_VALUES = 1;             /* imprimer les valeurs par defaut */
static int OUT_PPM = 0;
static const char *PPM_PATH = "map.ppm";

static int GRID_W = 64;
static int GRID_H = 48;

static unsigned long SEED = 1;
static double AMP0 = 1.0;              /* amplitude initiale diamond-square */
static double ROUGH = 0.65;            /* facteur de rugosite (0..1) */
static int SMOOTH_PASSES = 0;          /* adoucissements 3x3 */

static int WATER_ENABLE = 0;
static double WATER_LEVEL = 0.5;
static int WATER_FROM_EDGE = 1;        /* 1: inonder depuis les bords; 0: remplir tout <= niveau */
static int WATER_SEED_SET = 0;
static int WATER_SEED_X = 0;
static int WATER_SEED_Y = 0;
static int VALUES_WITH_WATER = 0;

/* --------- RNG simple (LCG) ---------- */
static unsigned long rng_state = 1;
static void rng_srand(unsigned long s) { if (s == 0) s = 1; rng_state = s; }
static unsigned long rng_nextu(void) { rng_state = rng_state * 1664525UL + 1013904223UL; return rng_state; }
/* [0,1) */
static double rng_rand01(void) {
    unsigned long u = rng_nextu();
    return (double)(u & 0xFFFFFF) / (double)0x1000000; /* 24 bits */
}
/* [-1,1] */
static double rng_randm1p1(void) { return rng_rand01() * 2.0 - 1.0; }

/* --------- Utils ---------- */
static double clamp01d(double v) { if (v < 0.0) return 0.0; if (v > 1.0) return 1.0; return v; }

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -x N            largeur sortie\n"
        "  -y N            hauteur sortie\n"
        "  -s N            seed RNG (entier)\n"
        "  -a R            amplitude initiale diamond-square (defaut 1.0)\n"
        "  -k R            rugosite (0..1, defaut 0.65)\n"
        "  -f N            passes d'adoucissement 3x3 (defaut 0)\n"
        "  --sea R         activer eau au niveau R (0..1)\n"
        "  --from-edge     inonde depuis les bords (par defaut si --sea)\n"
        "  --fill-all      marque eau toutes cellules <= niveau (ignore connectivite)\n"
        "  --seed x,y      point de depart supplementaire pour l'inondation\n"
        "  --values-with-water  imprimer h'=max(h, niveau) sur cellules eau\n"
        "  -o PATH         ecrire une carte couleur PPM\n"
        "  --no-values     ne pas imprimer les valeurs texte\n"
        , prog);
}

/* --------- Diamond-Square de taille P=2^n + 1 ---------- */
static void ds_generate(double *buf, int P) {
    int step;
    /* Initial corners */
    buf[0] = rng_rand01();
    buf[(P-1)] = rng_rand01();
    buf[(P-1)*P] = rng_rand01();
    buf[(P-1)*P + (P-1)] = rng_rand01();

    for (step = P - 1; step > 1; step /= 2) {
        int half = step / 2;
        int y, x;
        double scale = AMP0 * pow(ROUGH, (double)((int)(log((double)(P-1))/log(2.0)) - (int)(log((double)step)/log(2.0))));

        /* Diamond */
        for (y = half; y < P; y += step) {
            for (x = half; x < P; x += step) {
                double a = buf[(y - half) * P + (x - half)];
                double b = buf[(y - half) * P + (x + half - step + step - half)]; /* (x + half) */
                double c = buf[(y + half - step + step - half) * P + (x - half)]; /* (y + half) */
                double d = buf[(y + half - step + step - half) * P + (x + half - step + step - half)]; /* (y+half,x+half) */
                double avg = (a + b + c + d) * 0.25;
                double off = rng_randm1p1() * scale;
                buf[y * P + x] = clamp01d(avg + off);
            }
        }
        /* Square */
        for (y = 0; y < P; y += half) {
            int xstart = (y/half) % 2 == 0 ? half : 0;
            for (x = xstart; x < P; x += step) {
                double sum = 0.0; int cnt = 0;
                if (x - half >= 0) { sum += buf[y * P + (x - half)]; cnt++; }
                if (x + half < P)  { sum += buf[y * P + (x + half)]; cnt++; }
                if (y - half >= 0) { sum += buf[(y - half) * P + x]; cnt++; }
                if (y + half < P)  { sum += buf[(y + half) * P + x]; cnt++; }
                if (cnt > 0) {
                    double avg = sum / (double)cnt;
                    double off = rng_randm1p1() * scale;
                    buf[y * P + x] = clamp01d(avg + off);
                }
            }
        }
    }
}

/* Bilinear sampling from ds grid (P x P) into out (W x H) */
static void resample_bilinear(const double *src, int P, double *out, int W, int H) {
    int y, x;
    for (y = 0; y < H; ++y) {
        double v = ((double)y) * (double)(P - 1) / (double)(H - 1);
        int y0 = (int)floor(v);
        int y1 = y0 + 1; if (y1 >= P) y1 = P - 1;
        double fy = v - (double)y0;
        for (x = 0; x < W; ++x) {
            double u = ((double)x) * (double)(P - 1) / (double)(W - 1);
            int x0 = (int)floor(u);
            int x1 = x0 + 1; if (x1 >= P) x1 = P - 1;
            double fx = u - (double)x0;
            {
                double a = src[y0 * P + x0];
                double b = src[y0 * P + x1];
                double c = src[y1 * P + x0];
                double d = src[y1 * P + x1];
                double v0 = a * (1.0 - fx) + b * fx;
                double v1 = c * (1.0 - fx) + d * fx;
                out[y * W + x] = v0 * (1.0 - fy) + v1 * fy;
            }
        }
    }
}

/* Box blur 3x3 integer passes */
static void smooth_box(double *buf, int W, int H, int passes) {
    int p;
    if (passes <= 0) return;
    for (p = 0; p < passes; ++p) {
        int y, x;
        double *tmp = (double*)malloc((size_t)W * (size_t)H * sizeof(double));
        if (!tmp) return;
        for (y = 0; y < H; ++y) {
            for (x = 0; x < W; ++x) {
                int yy, xx;
                double sum = 0.0; int cnt = 0;
                for (yy = y - 1; yy <= y + 1; ++yy) {
                    for (xx = x - 1; xx <= x + 1; ++xx) {
                        int cx = xx; int cy = yy;
                        if (cx < 0) cx = 0; if (cx >= W) cx = W - 1;
                        if (cy < 0) cy = 0; if (cy >= H) cy = H - 1;
                        sum += buf[cy * W + cx];
                        cnt++;
                    }
                }
                tmp[y * W + x] = sum / (double)cnt;
            }
        }
        for (y = 0; y < H; ++y) {
            for (x = 0; x < W; ++x) buf[y * W + x] = tmp[y * W + x];
        }
        free(tmp);
    }
}

/* Flood water mask: outmask[y*W+x]=1 si eau, 0 sinon */
static void flood_from_edges_or_seed(const double *h, int W, int H,
                                     double level, int from_edge,
                                     int seed_set, int sx, int sy,
                                     unsigned char *mask)
{
    int i;
    int *queue_x, *queue_y;
    int qh = 0, qt = 0, qcap;
    /* init mask=0 */
    for (i = 0; i < W*H; ++i) mask[i] = 0;

    qcap = W*H;
    queue_x = (int*)malloc((size_t)qcap * sizeof(int));
    queue_y = (int*)malloc((size_t)qcap * sizeof(int));
    if (!queue_x || !queue_y) { free(queue_x); free(queue_y); return; }

    /* seed edges if requested */
    if (from_edge) {
        int x, y;
        for (x = 0; x < W; ++x) {
            if (h[0 * W + x] <= level) { mask[0 * W + x] = 1; queue_x[qt] = x; queue_y[qt] = 0; qt++; }
            if (h[(H-1) * W + x] <= level) { mask[(H-1) * W + x] = 1; queue_x[qt] = x; queue_y[qt] = H-1; qt++; }
        }
        for (y = 0; y < H; ++y) {
            if (h[y * W + 0] <= level) { mask[y * W + 0] = 1; queue_x[qt] = 0; queue_y[qt] = y; qt++; }
            if (h[y * W + (W-1)] <= level) { mask[y * W + (W-1)] = 1; queue_x[qt] = W-1; queue_y[qt] = y; qt++; }
        }
    }
    /* optional single seed */
    if (seed_set) {
        if (sx < 0) sx = 0; if (sx >= W) sx = W-1;
        if (sy < 0) sy = 0; if (sy >= H) sy = H-1;
        if (h[sy * W + sx] <= level && !mask[sy * W + sx]) {
            mask[sy * W + sx] = 1;
            queue_x[qt] = sx; queue_y[qt] = sy; qt++;
        }
    }

    /* BFS 4-connexity */
    while (qh < qt) {
        int cx = queue_x[qh];
        int cy = queue_y[qh];
        qh++;
        /* neighbors */
        {
            int nx, ny, k;
            static const int dx[4] = {1,-1,0,0};
            static const int dy[4] = {0,0,1,-1};
            for (k = 0; k < 4; ++k) {
                nx = cx + dx[k];
                ny = cy + dy[k];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (mask[ny * W + nx]) continue;
                if (h[ny * W + nx] <= level) {
                    mask[ny * W + nx] = 1;
                    queue_x[qt] = nx; queue_y[qt] = ny; qt++;
                }
            }
        }
    }

    free(queue_x); free(queue_y);
}

/* Remplissage sans connectivite : tout <= niveau est eau */
static void mark_all_below(const double *h, int W, int H, double level, unsigned char *mask) {
    int i;
    for (i = 0; i < W*H; ++i) mask[i] = (unsigned char)(h[i] <= level ? 1 : 0);
}

/* Palette geographique simple */
static void color_for(double v, int water, double level, int *R, int *G, int *B) {
    if (water) {
        /* profondeur: bleu plus sombre si profond */
        double d = level - v; if (d < 0.0) d = 0.0; if (d > 1.0) d = 1.0;
        int r = (int)(10 + 30 * (1.0 - d));
        int g = (int)(40 + 60 * (1.0 - d));
        int b = (int)(120 + 120 * (1.0 - d));
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        *R = r; *G = g; *B = b;
        return;
    }
    /* terre : sable -> vert -> roche -> neige */
    if (v < 0.05) { *R=194; *G=178; *B=128; return; }    /* plage */
    if (v < 0.30) { *R= 80; *G=160; *B= 60; return; }    /* plaine/foret */
    if (v < 0.60) { *R=120; *G=120; *B=120; return; }    /* roches */
    { *R=240; *G=240; *B=240; }                          /* neige */
}

/* Ecriture PPM */
static int write_ppm(const char *path, const int *rgb, int W, int H) {
    FILE *f = fopen(path, "wb");
    size_t want, wrote;
    int y, x;
    if (!f) { fprintf(stderr, "Impossible d'ouvrir '%s' en ecriture.\n", path); return -1; }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    want = (size_t)W * (size_t)H * 3;
    for (y = 0; y < H; ++y) {
        for (x = 0; x < W; ++x) {
            int off = (y * W + x) * 3;
            unsigned char px[3];
            int r = rgb[off+0], g = rgb[off+1], b = rgb[off+2];
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            px[0] = (unsigned char)r; px[1] = (unsigned char)g; px[2] = (unsigned char)b;
            wrote = fwrite(px, 1, 3, f);
            if (wrote != 3) { fclose(f); return -1; }
        }
    }
    fclose(f);
    (void)want; /* silence unused warning if any */
    return 0;
}

/* --------- main ---------- */
int main(int argc, char **argv) {
    int i;
    /* parse args */
    for (i = 1; i < argc; ) {
        const char *a = argv[i];
        if (strcmp(a, "-x") == 0 && i + 1 < argc) {
            char *e=0; long v = strtol(argv[i+1], &e, 10);
            if (*e!='\0' || v<=1) { usage(argv[0]); return 1; }
            GRID_W = (int)v; i+=2; continue;
        } else if (strcmp(a, "-y") == 0 && i + 1 < argc) {
            char *e=0; long v = strtol(argv[i+1], &e, 10);
            if (*e!='\0' || v<=1) { usage(argv[0]); return 1; }
            GRID_H = (int)v; i+=2; continue;
        } else if (strcmp(a, "-s") == 0 && i + 1 < argc) {
            char *e=0; unsigned long v = (unsigned long)strtoul(argv[i+1], &e, 10);
            if (*e!='\0') { usage(argv[0]); return 1; }
            SEED = v; i+=2; continue;
        } else if (strcmp(a, "-a") == 0 && i + 1 < argc) {
            char *e=0; double v = strtod(argv[i+1], &e);
            if (*e!='\0') { usage(argv[0]); return 1; }
            AMP0 = v; i+=2; continue;
        } else if (strcmp(a, "-k") == 0 && i + 1 < argc) {
            char *e=0; double v = strtod(argv[i+1], &e);
            if (*e!='\0' || v<=0.0) { usage(argv[0]); return 1; }
            ROUGH = v; i+=2; continue;
        } else if (strcmp(a, "-f") == 0 && i + 1 < argc) {
            char *e=0; long v = strtol(argv[i+1], &e, 10);
            if (*e!='\0' || v<0) { usage(argv[0]); return 1; }
            SMOOTH_PASSES = (int)v; i+=2; continue;
        } else if (strcmp(a, "--sea") == 0 && i + 1 < argc) {
            char *e=0; double v = strtod(argv[i+1], &e);
            if (*e!='\0' || v<0.0 || v>1.0) { usage(argv[0]); return 1; }
            WATER_ENABLE = 1; WATER_LEVEL = v; i+=2; continue;
        } else if (strcmp(a, "--from-edge") == 0) {
            WATER_FROM_EDGE = 1; i+=1; continue;
        } else if (strcmp(a, "--fill-all") == 0) {
            WATER_FROM_EDGE = 0; i+=1; continue;
        } else if (strcmp(a, "--seed") == 0 && i + 1 < argc) {
            int x=0,y=0; const char *p = argv[i+1]; const char *c = strchr(p, ',');
            if (!c) { usage(argv[0]); return 1; }
            x = (int)strtol(p, 0, 10);
            y = (int)strtol(c+1, 0, 10);
            WATER_SEED_SET = 1; WATER_SEED_X = x; WATER_SEED_Y = y; i+=2; continue;
        } else if (strcmp(a, "--values-with-water") == 0) {
            VALUES_WITH_WATER = 1; i+=1; continue;
        } else if (strcmp(a, "-o") == 0 && i + 1 < argc) {
            OUT_PPM = 1; PPM_PATH = argv[i+1]; i+=2; continue;
        } else if (strcmp(a, "--no-values") == 0) {
            OUT_VALUES = 0; i+=1; continue;
        } else {
            usage(argv[0]); return 1;
        }
    }

    /* Generation via diamond-square a taille P=2^n+1, puis resample en WxH */
    {
        int maxdim = (GRID_W > GRID_H) ? GRID_W : GRID_H;
        int n = 1; while (((1<<n) + 1) < maxdim) n++;
        /* Taille DS : P = 2^n + 1 */
        {
            int P = (1<<n) + 1;
            double *ds = (double*)malloc((size_t)P * (size_t)P * sizeof(double));
            double *map = (double*)malloc((size_t)GRID_W * (size_t)GRID_H * sizeof(double));
            unsigned char *water = 0;
            int *rgb = 0;
            int y, x;

            if (!ds || !map) { fprintf(stderr, "Alloc DS/map impossible.\n"); free(ds); free(map); return 1; }

            rng_srand(SEED);
            ds_generate(ds, P);
            resample_bilinear(ds, P, map, GRID_W, GRID_H);
            if (SMOOTH_PASSES > 0) smooth_box(map, GRID_W, GRID_H, SMOOTH_PASSES);

            /* Eau */
            if (WATER_ENABLE) {
                water = (unsigned char*)malloc((size_t)GRID_W * (size_t)GRID_H);
                if (!water) { fprintf(stderr, "Alloc eau impossible.\n"); free(ds); free(map); return 1; }
                if (WATER_FROM_EDGE) {
                    flood_from_edges_or_seed(map, GRID_W, GRID_H, WATER_LEVEL,
                                             1, WATER_SEED_SET, WATER_SEED_X, WATER_SEED_Y, water);
                } else {
                    mark_all_below(map, GRID_W, GRID_H, WATER_LEVEL, water);
                }
            }

            /* Sortie valeurs */
            if (OUT_VALUES) {
                for (y = 0; y < GRID_H; ++y) {
                    for (x = 0; x < GRID_W; ++x) {
                        double v = map[y * GRID_W + x];
                        if (WATER_ENABLE && VALUES_WITH_WATER) {
                            if (water && water[y * GRID_W + x]) {
                                v = WATER_LEVEL; /* remplir au niveau constant */
                            }
                        }
                        printf("%.6f%s", v, (x == GRID_W - 1) ? "\n" : " ");
                    }
                }
            }

            /* Sortie PPM */
            if (OUT_PPM) {
                rgb = (int*)malloc((size_t)GRID_W * (size_t)GRID_H * 3 * sizeof(int));
                if (!rgb) { fprintf(stderr, "Alloc RGB impossible.\n"); free(ds); free(map); free(water); return 1; }
                for (y = 0; y < GRID_H; ++y) {
                    for (x = 0; x < GRID_W; ++x) {
                        double v = map[y * GRID_W + x];
                        int w = (WATER_ENABLE && water) ? water[y * GRID_W + x] : 0;
                        int r,g,b;
                        color_for(v, w, WATER_LEVEL, &r, &g, &b);
                        /* renforcement du rivage: foncer la frontiere eau/terre */
                        if (WATER_ENABLE && water) {
                            int nx, ny, k;
                            static const int dx[4] = {1,-1,0,0};
                            static const int dy[4] = {0,0,1,-1};
                            for (k=0;k<4;++k){
                                nx=x+dx[k]; ny=y+dy[k];
                                if (nx>=0&&ny>=0&&nx<GRID_W&&ny<GRID_H){
                                    int w2 = water[ny*GRID_W+nx];
                                    if (w2 != w) { r = (r*7)/10; g=(g*7)/10; b=(b*7)/10; break; }
                                }
                            }
                        }
                        rgb[(y*GRID_W + x)*3 + 0] = r;
                        rgb[(y*GRID_W + x)*3 + 1] = g;
                        rgb[(y*GRID_W + x)*3 + 2] = b;
                    }
                }
                if (write_ppm(PPM_PATH, rgb, GRID_W, GRID_H) != 0) {
                    fprintf(stderr, "Echec ecriture %s\n", PPM_PATH);
                    free(rgb); free(water); free(ds); free(map); return 1;
                }
            }

            free(rgb);
            free(water);
            free(ds);
            free(map);
        }
    }
    return 0;
}
