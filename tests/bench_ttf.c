/* bench_ttf: render every glyph of a real TrueType font through render/ttf.c
 * at the 8x16 and 16x32 cells (plan 2026-10-22-handler-rasterizer.md, R2.4).
 * Fails on any error return; prints the glyph count, blank count and time.
 * Not part of `make test` (the font is not in the repository): run
 * `make bench-ttf` after a change to the rasterizer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../render/ttf.h"

typedef struct {
    FILE *fp;
} file_ctx;

static long f_read(void *ctx, unsigned long off, void *buf, long len)
{
    file_ctx *c = (file_ctx *)ctx;
    if (fseek(c->fp, (long)off, SEEK_SET) != 0)
        return -1;
    return (long)fread(buf, 1, (size_t)len, c->fp);
}

static void *f_alloc(void *ctx, unsigned long size)
{
    (void)ctx;
    return calloc(1, size);
}

static void f_free(void *ctx, void *p)
{
    (void)ctx;
    free(p);
}

static unsigned char cov[32 * 32];
static unsigned char mask[4 * 32];

int main(int argc, char **argv)
{
    static const int cells[2][3] = { { 8, 16, 12 }, { 16, 32, 25 } };
    file_ctx c;
    ttf_io io;
    ttf_font *f;
    int r, s, n, gid, errors = 0;
    long blank = 0, drawn = 0;
    clock_t t0;
    if (argc != 2) {
        fprintf(stderr, "usage: bench_ttf FONT.ttf\n");
        return 2;
    }
    c.fp = fopen(argv[1], "rb");
    if (!c.fp) {
        fprintf(stderr, "[ERROR] cannot open %s\n", argv[1]);
        return 2;
    }
    io.read = f_read;
    io.alloc = f_alloc;
    io.free = f_free;
    io.ctx = &c;
    r = ttf_open(&io, &f);
    if (r != TTF_OK) {
        fprintf(stderr, "[ERROR] ttf_open: %d\n", r);
        return 1;
    }
    n = ttf_num_glyphs(f);
    t0 = clock();
    for (s = 0; s < 2; s++) {
        int w = cells[s][0], h = cells[s][1];
        long xs = (long)w * 65536L / ttf_units_per_em(f);
        long ys = (long)h * 65536L / ttf_units_per_em(f);
        for (gid = 0; gid < n; gid++) {
            memset(cov, 0, sizeof cov);
            r = ttf_render(f, (unsigned)gid, xs, ys, 0, cells[s][2], cov, w, h);
            if (r < 0) {
                if (errors < 20)
                    fprintf(stderr, "[ERROR] glyph %d at %dx%d: %d\n", gid, w, h, r);
                errors++;
                continue;
            }
            if (r == 0)
                blank++;
            else
                drawn++;
            ttf_threshold(cov, w, h, mask, (w + 7) / 8);
        }
    }
    printf("[INFO] %d glyphs x 2 cells: %ld drawn, %ld blank, %d errors, %.2f s\n", n, drawn,
           blank, errors, (double)(clock() - t0) / CLOCKS_PER_SEC);
    ttf_close(f);
    fclose(c.fp);
    return errors ? 1 : 0;
}
