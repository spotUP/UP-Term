/* aadither_expect -- R6.A3: the expected pen of every antialiased edge pixel of a glyph over a dithered tile.
 *
 * Host helper of tools/rig/aadither_rig.py. It runs the handler's own code: render/ttf.c for the glyph's coverage
 * (placed as render/outline.c's render() does for TTF_SHAPE), ce_mix for the colour, render/aa_ink_rule.h for the
 * shade ramp (target, level, threshold, accept) and render/pen_match.h pm_dist for the nearest pen. The pass order
 * is aa_ink.c tile_passes: per background pen a set of shades, a pass per used step from the lightest, each
 * drawing where coverage >= aa_ink_threshold(lo, i + 1, n), through the tile.
 *
 *   aadither_expect <font.ttf> <hex code point> <cw> <ch> <base> <rx> <ry> <nshades>  < spec
 * spec (text): "pal <n> <rrggbb>*n", "fg <rrggbb>", then ch rows of cw hex digits: the tile's pen under each pixel.
 * Output, per pixel with 0 < coverage < 255: "x y cov tilepen expectedpen idealpen"; ideal = the palette pen
 * nearest to ce_mix(fg, tile pen colour, cov) with no ramp (the plan's wording).
 */
#include "../../render/ttf.h"
#include "../../render/aa_ink_rule.h"
#include "../../render/pen_match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long rd(void *ctx, unsigned long off, void *buf, long len)
{
    FILE *f = (FILE *)ctx;
    if (fseek(f, (long)off, SEEK_SET))
        return -1;
    return (long)fread(buf, 1, (size_t)len, f);
}
static void *al(void *ctx, unsigned long n) { (void)ctx; return calloc(1, n); }
static void fr(void *ctx, void *p) { (void)ctx; free(p); }

static unsigned long pal[32];
static int npal;

static int nearest(unsigned long want)
{
    pm_target t;
    int i, b = 0;
    long bd = -1;
    pm_aim(want, &t);
    for (i = 0; i < npal; i++) {
        long d = pm_dist(&t, pal[i]);
        if (bd < 0 || d < bd)
            bd = d, b = i;
    }
    return b;
}

int main(int argc, char **argv)
{
    FILE *f;
    ttf_io io;
    ttf_font *tt;
    ttf_place_out pl;
    unsigned gid;
    unsigned long cp, fg = 0;
    int cw, ch, base, rx, ry, n, x, y, i, j;
    unsigned char *cov;
    char tok[64];
    int tile[64 * 64];
    int shade[32][AA_INK_MAX_SHADES], use[32][AA_INK_MAX_SHADES];
    if (argc != 9)
        return 2;
    f = fopen(argv[1], "rb");
    if (!f)
        return 3;
    cp = strtoul(argv[2], 0, 16);
    cw = atoi(argv[3]); ch = atoi(argv[4]); base = atoi(argv[5]); rx = atoi(argv[6]); ry = atoi(argv[7]);
    n = atoi(argv[8]);
    io.read = rd; io.alloc = al; io.free = fr; io.ctx = f;
    if (ttf_open(&io, &tt) != TTF_OK)
        return 4;
    gid = ttf_glyph_index(tt, cp);
    if (!gid)
        return 5;
    ttf_place(tt, gid, TTF_SHAPE, cw, ch, base, rx, ry, &pl);
    cov = (unsigned char *)calloc(1, (size_t)cw * ch);
    if (ttf_render(tt, gid, pl.xscale, pl.yscale, pl.ox, pl.baseline, cov, cw, ch) < 0)
        return 6;
    while (scanf("%63s", tok) == 1) {
        if (!strcmp(tok, "pal")) {
            scanf("%d", &npal);
            for (i = 0; i < npal; i++)
                scanf("%lx", &pal[i]);
        } else if (!strcmp(tok, "fg")) {
            scanf("%lx", &fg);
        } else {
            /* first token of the tile rows */
            for (y = 0; y < ch; y++) {
                if (y)
                    scanf("%63s", tok);
                for (x = 0; x < cw; x++)
                    tile[y * cw + x] = tok[x] >= 'a' ? tok[x] - 'a' + 10 : tok[x] - '0';
            }
            break;
        }
    }
    /* a shade set per background pen, as aa_ink.c colours(): nearest pen, used when nearer than fg and bg */
    for (j = 0; j < npal; j++) {
        aa_ink_rgb tab[256];
        aa_ink_build_tab(tab, fg, pal[j]);
        for (i = 0; i < n; i++) {
            unsigned long want = aa_ink_shade_target(tab, i, n);
            int p = nearest(want), k;
            pm_target t;
            pm_aim(want, &t);
            shade[j][i] = p;
            use[j][i] = aa_ink_shade_accept(pm_dist(&t, pal[p]), pm_dist(&t, fg), pm_dist(&t, pal[j]));
            for (k = 0; k < i && use[j][i]; k++)
                if (use[j][k] && shade[j][k] == p)
                    use[j][i] = 0;
        }
    }
    for (y = 0; y < ch; y++)
        for (x = 0; x < cw; x++) {
            int c = cov[y * cw + x], t = tile[y * cw + x], pen = t, lo = 0, ideal, fgp = nearest(fg);
            if (c == 0 || c == 255)
                continue;
            for (i = 0; i <= n; i++) {
                if (i < n && !use[t][i])
                    continue;
                if (c >= aa_ink_threshold(lo, i + 1, n))
                    pen = i < n ? shade[t][i] : fgp;
                lo = i + 1;
            }
            ideal = nearest(ce_mix(fg, pal[t], c));
            printf("%d %d %d %d %d %d\n", x, y, c, t, pen, ideal);
        }
    return 0;
}
