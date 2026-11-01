/* kittyout: the shared kitty encoder (pic/kittyout.c) driven into the engine's own kitty parser:
 * what the encoder writes, the engine shows as the input picture (several chunks, RGB and RGBA),
 * and a PNG goes through as f=100. The encoder's chunk framing is the sentinel: the same bytes
 * cut in two chunks by hand give the same picture, a broken encoder (one chunk, no m=) does not. */
#include "harness.h"
#include <stdlib.h>
#include <string.h>
#include "../pic/kittyout.h"
#include "kitty_png_vectors.h"

typedef struct sink {
    char buf[40000];
    long n;
    long chunks;
} sink;

static void sink_put(void *u, const char *s, long n)
{
    sink *k = (sink *)u;
    if (n == 2 && s[0] == '\033' && s[1] == '\\')
        k->chunks++;
    memcpy(k->buf + k->n, s, (size_t)n);
    k->n += n;
    k->buf[k->n] = 0;
}

static vt_u32 pixel(const vt_image_view *v, int x, int y)
{
    return v->pal[v->pix[y * v->w + x]];
}

#define PW 40
#define PH 30
static const unsigned long cols4[4] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00};

static void make_rgb(unsigned char *px, int bpp)
{
    int x, y;
    for (y = 0; y < PH; y++)
        for (x = 0; x < PW; x++) {
            unsigned long c = cols4[(x / 10 + y / 15) & 3];
            unsigned char *p = px + ((long)y * PW + x) * bpp;
            p[0] = (unsigned char)(c >> 16);
            p[1] = (unsigned char)(c >> 8);
            p[2] = (unsigned char)c;
            if (bpp == 4)
                p[3] = (y == 29 && x == 39) ? 0 : 255;
        }
}

static void round_trip(int bpp)
{
    static unsigned char px[PW * PH * 4];
    static sink s;
    vt_term *t = h_new(80, 24, VT_XTERM);
    vt_image_view v;
    int x, y;
    vt_set_cell_pixels(t, 2, 6);
    make_rgb(px, bpp);
    s.n = s.chunks = 0;
    CHECK(kout_pixels(sink_put, &s, px, PW, PH, bpp, 0));
    CHECK(s.chunks >= 2); /* 3600 or 4800 bytes: more than one chunk */
    CHECK(strstr(s.buf, "m=1;") != 0);
    h_put(t, s.buf);
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, PW);
    CHECK_INT(v.h, PH);
    for (y = 0; y < PH; y++)
        for (x = 0; x < PW; x++) {
            if (bpp == 4 && x == 39 && y == 29)
                CHECK_INT(v.pix[y * v.w + x], 0); /* alpha 0: unset */
            else if (pixel(&v, x, y) != cols4[(x / 10 + y / 15) & 3])
                CHECK_INT(pixel(&v, x, y), (vt_u32)cols4[(x / 10 + y / 15) & 3]);
        }
    vt_free(t);
}

static void png_goes_as_f100_and_scales_by_cells(void)
{
    static sink s;
    kout_opts o;
    vt_term *t = h_new(80, 24, VT_XTERM);
    vt_image_view v;
    vt_set_cell_pixels(t, 2, 6);
    o.cols = 10;
    o.rows = 0;
    o.quiet = 2;
    s.n = s.chunks = 0;
    CHECK(kout_png(sink_put, &s, png_rgb8, (long)sizeof(png_rgb8), &o));
    CHECK(strstr(s.buf, "f=100,c=10,q=2") != 0);
    h_put(t, s.buf);
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 20); /* 10 cells of 2 pixels */
    vt_free(t);
}

static void query_and_scale_and_fit(void)
{
    static sink s;
    vt_term *t = h_new(80, 24, VT_XTERM);
    unsigned char src[4 * 2 * 3], dst[2 * 1 * 3];
    int dw, dh, i;
    s.n = s.chunks = 0;
    kout_query(sink_put, &s);
    h_reply_clear();
    h_put(t, s.buf);
    CHECK_STR(h_reply, "\033_Gi=31;OK\033\\");
    CHECK(kout_query_ok(h_reply, (long)strlen(h_reply)));
    CHECK(!kout_query_ok("", 0));
    CHECK_INT(vt_images(t), 0);
    for (i = 0; i < 24; i++)
        src[i] = (unsigned char)i;
    kout_scale(src, 4, 2, 3, dst, 2, 1);
    CHECK_INT(dst[3], 6); /* pixel 2 of row 0 */
    kout_fit(2000, 1000, 1000000L, 0, &dw, &dh);
    CHECK(dw * dh <= 1000000 && dw * 2 >= dh * 3);
    kout_fit(2000, 1000, 1000000L, 640, &dw, &dh);
    CHECK_INT(dw, 640);
    CHECK_INT(dh, 320);
    kout_fit(10, 10, 1000000L, 640, &dw, &dh);
    CHECK_INT(dw, 10);
    vt_free(t);
}

void suite_kittyout(void)
{
    round_trip(3);
    round_trip(4);
    png_goes_as_f100_and_scales_by_cells();
    query_and_scale_and_fit();
}
