/*
 * Rendu isometrique simple d'une heightmap 0..1 vers une image PPM.
 * C ANSI C89, aucune dependance externe.
 *
 * Lecture: fichier texte avec width*height doubles (format "plasma --only-values")
 * Projection: tuiles isometriques (losange) + deux faces laterales
 * Occlusion: painter's algorithm par sommes (x+y) croissantes
 *
 * Compilation:
 *   cc -std=c89 -Wall -Wextra -O2 iso.c -o iso
 *
 * Exemple:
 *   ./iso -x 64 -y 48 -i hmap.txt -o iso.ppm -tw 16 -th 8 -zs 80
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- Options et etat ----- */
static int GRID_W = 20;
static int GRID_H = 20;
static const char *IN_PATH  = 0;       /* 0 = stdin */
static const char *OUT_PATH = "iso.ppm";
static int TILE_W = 16;                /* largeur d'une tuile isometrique */
static int TILE_H = 8;                 /* hauteur d'une tuile isometrique */
static int ZS     = 64;                /* echelle verticale */
static int BG_R = 16, BG_G = 16, BG_B = 24; /* couleur de fond sombre */

/* ----- Outils ----- */
static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -x N           largeur de la grille\n"
        "  -y N           hauteur de la grille\n"
        "  -i PATH        fichier d'entree (sinon stdin, utiliser '-' pour stdin)\n"
        "  -o PATH        fichier PPM de sortie (defaut iso.ppm)\n"
        "  -tw N          largeur de tuile isometrique (defaut 16)\n"
        "  -th N          hauteur de tuile isometrique (defaut 8)\n"
        "  -zs N          echelle verticale / hauteur max (defaut 64)\n"
        "  -bg r,g,b      fond (0..255, defaut 16,16,24)\n"
        , prog);
}

/* Parse r,g,b */
static int parse_rgb(const char *s, int *r, int *g, int *b) {
    const char *c1 = strchr(s, ',');
    const char *c2;
    char *e;
    long R, G, B;
    if (!c1) return -1;
    c2 = strchr(c1 + 1, ',');
    if (!c2) return -1;

    R = strtol(s, &e, 10);
    if (e != c1) return -1;

    G = strtol(c1 + 1, &e, 10);
    if (e != c2) return -1;

    B = strtol(c2 + 1, &e, 10);
    if (*e != '\0') return -1;

    if (R < 0) { R = 0; }
    else if (R > 255) { R = 255; }

    if (G < 0) { G = 0; }
    else if (G > 255) { G = 255; }

    if (B < 0) { B = 0; }
    else if (B > 255) { B = 255; }

    *r = (int)R; *g = (int)G; *b = (int)B;
    return 0;
}

/* Clamp entier 0..255 */
static int clamp8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

/* Ecriture PPM binaire P6 */
static int write_ppm(const char *path, const unsigned char *rgb, int W, int H) {
    FILE *f = fopen(path, "wb");
    size_t want, wrote;
    if (!f) {
        fprintf(stderr, "Impossible d'ouvrir '%s' en ecriture.\n", path);
        return -1;
    }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    want = (size_t)(W * H * 3);
    wrote = fwrite(rgb, 1, want, f);
    if (wrote != want) {
        fprintf(stderr, "Erreur d'ecriture PPM.\n");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

/* Framebuffer: acces pixel avec clipping */
static void put_px(unsigned char *fb, int W, int H, int x, int y, int r, int g, int b) {
    int off;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    off = (y * W + x) * 3;
    fb[off + 0] = (unsigned char)clamp8(r);
    fb[off + 1] = (unsigned char)clamp8(g);
    fb[off + 2] = (unsigned char)clamp8(b);
}

/* Remplissage triangle plein (ints), test "meme signe" */
static void fill_tri(unsigned char *fb, int W, int H,
                     int x0, int y0, int x1, int y1, int x2, int y2,
                     int r, int g, int b)
{
    int minx, maxx, miny, maxy, x, y;
    long A01, B01, A12, B12, A20, B20;
    long w0, w1, w2;

    /* Boite englobante */
    minx = x0; if (x1 < minx) minx = x1; if (x2 < minx) minx = x2;
    maxx = x0; if (x1 > maxx) maxx = x1; if (x2 > maxx) maxx = x2;
    miny = y0; if (y1 < miny) miny = y1; if (y2 < miny) miny = y2;
    maxy = y0; if (y1 > maxy) maxy = y1; if (y2 > maxy) maxy = y2;

    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= W) maxx = W - 1;
    if (maxy >= H) maxy = H - 1;

    /* Coeffs des fonctions de bord */
    A01 = (long)(y0 - y1); B01 = (long)(x1 - x0);
    A12 = (long)(y1 - y2); B12 = (long)(x2 - x1);
    A20 = (long)(y2 - y0); B20 = (long)(x0 - x2);

    for (y = miny; y <= maxy; ++y) {
        for (x = minx; x <= maxx; ++x) {
            w0 = (long)(x - x1) * A12 + (long)(y - y1) * B12;
            w1 = (long)(x - x2) * A20 + (long)(y - y2) * B20;
            w2 = (long)(x - x0) * A01 + (long)(y - y0) * B01;

            /* Remplir si tous >=0 ou tous <=0 (meme signe) */
            if ( (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                 (w0 <= 0 && w1 <= 0 && w2 <= 0) ) {
                put_px(fb, W, H, x, y, r, g, b);
            }
        }
    }
}

/* Remplit un quadrilatere convexe en 2 triangles */
static void fill_quad(unsigned char *fb, int W, int H,
                      int x0, int y0, int x1, int y1,
                      int x2, int y2, int x3, int y3,
                      int r, int g, int b)
{
    fill_tri(fb, W, H, x0, y0, x1, y1, x2, y2, r, g, b);
    fill_tri(fb, W, H, x0, y0, x2, y2, x3, y3, r, g, b);
}

/* ----- Programme principal ----- */
int main(int argc, char **argv) {
    int i;

    /* Parsing args minimaliste */
    for (i = 1; i < argc; ) {
        const char *a = argv[i];
        if (strcmp(a, "-x") == 0 && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v <= 0) { print_usage(argv[0]); return 1; }
            GRID_W = (int)v; i += 2; continue;
        } else if (strcmp(a, "-y") == 0 && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v <= 0) { print_usage(argv[0]); return 1; }
            GRID_H = (int)v; i += 2; continue;
        } else if (strcmp(a, "-i") == 0 && i + 1 < argc) {
            IN_PATH = argv[i+1]; i += 2; continue;
        } else if (strcmp(a, "-o") == 0 && i + 1 < argc) {
            OUT_PATH = argv[i+1]; i += 2; continue;
        } else if (strcmp(a, "-tw") == 0 && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v <= 0) { print_usage(argv[0]); return 1; }
            TILE_W = (int)v; i += 2; continue;
        } else if (strcmp(a, "-th") == 0 && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v <= 0) { print_usage(argv[0]); return 1; }
            TILE_H = (int)v; i += 2; continue;
        } else if (strcmp(a, "-zs") == 0 && i + 1 < argc) {
            char *e = 0; long v = strtol(argv[i+1], &e, 10);
            if (*e != '\0' || v < 0) { print_usage(argv[0]); return 1; }
            ZS = (int)v; i += 2; continue;
        } else if (strcmp(a, "-bg") == 0 && i + 1 < argc) {
            if (parse_rgb(argv[i+1], &BG_R, &BG_G, &BG_B) != 0) { print_usage(argv[0]); return 1; }
            i += 2; continue;
        } else {
            print_usage(argv[0]); return 1;
        }
    }

    /* Lecture de la heightmap */
    {
        long N = (long)GRID_W * (long)GRID_H;
        double *grid = (double*)malloc((size_t)N * sizeof(double));
        unsigned char *fb;
        int FB_W, FB_H, MARGIN;
        int x, y;
        FILE *f;

        if (!grid) { fprintf(stderr, "Allocation impossible.\n"); return 1; }

        if (IN_PATH && strcmp(IN_PATH, "-") != 0) {
            f = fopen(IN_PATH, "r");
        } else {
            f = stdin;
        }
        if (!f) { fprintf(stderr, "Impossible d'ouvrir '%s'.\n", IN_PATH ? IN_PATH : "(stdin)"); free(grid); return 1; }

        for (y = 0; y < GRID_H; ++y) {
            for (x = 0; x < GRID_W; ++x) {
                double v = 0.0;
                if (fscanf(f, "%lf", &v) != 1) {
                    fprintf(stderr, "Fichier trop court ou invalide a y=%d x=%d.\n", y, x);
                    if (f != stdin) fclose(f);
                    free(grid);
                    return 1;
                }
                if (v < 0.0) v = 0.0;
                if (v > 1.0) v = 1.0;
                grid[y * GRID_W + x] = v;
            }
        }
        if (f != stdin) fclose(f);

        /* Dimensions de l'image isometrique */
        MARGIN = TILE_W; /* marge visuelle */
        FB_W = (GRID_W + GRID_H) * (TILE_W / 2) + MARGIN * 2 + TILE_W;
        FB_H = (GRID_W + GRID_H) * (TILE_H / 2) + ZS + MARGIN * 2 + TILE_H;

        fb = (unsigned char*)malloc((size_t)FB_W * (size_t)FB_H * 3);
        if (!fb) { fprintf(stderr, "Allocation framebuffer impossible.\n"); free(grid); return 1; }

        /* Fond */
        {
            long i2, total = (long)FB_W * (long)FB_H * 3;
            for (i2 = 0; i2 < total; i2 += 3) {
                fb[i2+0] = (unsigned char)BG_R;
                fb[i2+1] = (unsigned char)BG_G;
                fb[i2+2] = (unsigned char)BG_B;
            }
        }

        /* Offsets pour centrer */
        {
            /* On decale de H * (TILE_W/2) a gauche pour bien placer l'origine */
            int off_x = MARGIN + (GRID_H * (TILE_W / 2));
            int off_y = MARGIN + ZS; /* laisser de la place pour l'elevation */

            /* Peinture du fond vers l'avant: s = x + y */
            int s, sx, sy;

            for (s = 0; s <= (GRID_W - 1) + (GRID_H - 1); ++s) {
                for (x = 0; x < GRID_W; ++x) {
                    int gy = s - x;
                    int gx = x;
                    if (gy < 0 || gy >= GRID_H) continue;

                    /* valeur de hauteur 0..1 */
                    {
                        double h = grid[gy * GRID_W + gx];
                        int z = (int)(h * (double)ZS + 0.5);

                        /* Centre iso au niveau du sommet (haut de la colonne) */
                        sx = off_x + (gx - gy) * (TILE_W / 2);
                        sy = off_y + (gx + gy) * (TILE_H / 2);

                        /* Points du losange sommet (au niveau eleve sy - z) */
                        {
                            int cx = sx;
                            int cy = sy - z;

                            int top_x    = cx;
                            int top_y    = cy - (TILE_H / 2);
                            int left_x   = cx - (TILE_W / 2);
                            int left_y   = cy;
                            int right_x  = cx + (TILE_W / 2);
                            int right_y  = cy;
                            int bot_x    = cx;
                            int bot_y    = cy + (TILE_H / 2);

                            /* Points du losange au sol (base) */
                            int base_cx  = sx;
                            int base_cy  = sy;
                            int b_left_x = base_cx - (TILE_W / 2);
                            int b_left_y = base_cy;
                            int b_right_x= base_cx + (TILE_W / 2);
                            int b_right_y= base_cy;
                            int b_bot_x  = base_cx;
                            int b_bot_y  = base_cy + (TILE_H / 2);

                            /* Couleurs en niveaux de gris, faces differenciees */
                            {
                                int g_top   = clamp8((int)(h * 255.0 + 0.5));
                                int g_left  = clamp8((int)(g_top * 80 / 100));
                                int g_right = clamp8((int)(g_top * 60 / 100));

                                /* Faces laterales (gauche et droite) */
                                fill_quad(fb, FB_W, FB_H,
                                          left_x,  left_y,
                                          b_left_x,b_left_y,
                                          b_bot_x, b_bot_y,
                                          bot_x,   bot_y,
                                          g_left, g_left, g_left);

                                fill_quad(fb, FB_W, FB_H,
                                          right_x,  right_y,
                                          bot_x,    bot_y,
                                          b_bot_x,  b_bot_y,
                                          b_right_x,b_right_y,
                                          g_right, g_right, g_right);

                                /* Dessus (losange) en deux triangles */
                                fill_tri(fb, FB_W, FB_H, top_x, top_y, left_x, left_y, right_x, right_y,
                                         g_top, g_top, g_top);
                                fill_tri(fb, FB_W, FB_H, bot_x, bot_y, right_x, right_y, left_x, left_y,
                                         g_top, g_top, g_top);
                            }
                        }
                    }
                }
            }
        }

        /* Ecriture PPM */
        if (write_ppm(OUT_PATH, fb, FB_W, FB_H) != 0) {
            fprintf(stderr, "Echec d'ecriture de %s\n", OUT_PATH);
            free(fb);
            free(grid);
            return 1;
        }

        free(fb);
        free(grid);
    }

    return 0;
}
