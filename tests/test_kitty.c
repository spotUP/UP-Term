/* Kitty graphics (APC G): the keys and the base64 data become the same
 * indexed images sixel makes, replies go back to the program, and the
 * pictures obey the shared image memory. Cells are 2 x 6 pixels. The
 * host PNG decoder here is a stand-in that hands back fixed pixels. */
#include "harness.h"
#include <stdlib.h>
#include <string.h>
/* The engine's own PNG decoder, compiled in here to assert its words
 * exactly; the engine has its own copy behind vt_term. */
#define VT_MALLOC(n) malloc(n)
#define VT_FREE(p) free(p)
#define KIT_RAW_MAX (4L * 1024L * 1024L)
#include "../engine/inflate.c"
#include "../engine/png.c"
#include "kitty_png_vectors.h"

static vt_term *kitty_term(int cols, int rows)
{
    vt_term *t = h_new(cols, rows, VT_XTERM);
    vt_set_cell_pixels(t, 2, 6);
    return t;
}

static vt_u32 pixel(const vt_image_view *v, int x, int y)
{
    return v->pal[v->pix[y * v->w + x]];
}

/* 2 x 2 RGB: red green / blue white */
static const char rgb2x2[] = "/wAAAP8AAAD/////";
/* the same 12 bytes as 2 x 2 RGBA with the last pixel clear */
static const char rgba2x2[] = "/wAA/wD/AP8AAP//////AA==";
/* zlib of the 12 bytes of the RGB picture */
static const char rgb2x2_z[] = "eJz7z8DA8B+EgQAAHe4F+w==";
/* zlib of 3,000,000 zero bytes: a 1000 x 1000 black RGB picture, in
 * pieces (a C90 string literal is short) */
static const char *const black1000_z[] = {
    "eJztwQEBAAAAgiD/r25IQAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAC8GyWMAAQ==",
    0
};

static int decode_calls;
static int stand_in_decode(const vt_u8 *data, long n, vt_u32 **argb, int *w, int *h)
{
    static vt_u32 px[2] = { 0xFF112233u, 0x00000000u };
    (void)data;
    decode_calls++;
    if (n < 4 || data[0] != 0x89)
        return 0;
    *argb = px;
    *w = 2;
    *h = 1;
    return 1;
}
static void stand_in_free(vt_u32 *p) { (void)p; }

static void raw_rgb_becomes_an_indexed_image(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    h_put(t, "\033_Ga=T,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 2);
    CHECK_INT(v.h, 2);
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    CHECK_INT(pixel(&v, 1, 0), 0x00FF00);
    CHECK_INT(pixel(&v, 0, 1), 0x0000FF);
    CHECK_INT(pixel(&v, 1, 1), 0xFFFFFF);
    CHECK_INT(h_cell(t, 0, 0)->pad & VT_CELL_IMAGE, VT_CELL_IMAGE);
    vt_free(t);
}

static void rgba_clear_pixels_stay_unset(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    h_put(t, "\033_Ga=T,f=32,s=2,v=2;");
    h_put(t, rgba2x2);
    h_put(t, "\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK(v.pix[0] != 0);
    CHECK_INT(v.pix[3], 0); /* alpha 0: index 0, the background shows */
    vt_free(t);
}

static void chunks_join_into_one_picture(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    h_put(t, "\033_Ga=T,f=24,s=2,v=2,m=1;/wAAAP8A\033\\");
    CHECK_INT(vt_images(t), 0); /* not complete yet */
    h_put(t, "\033_Gm=1;AAD/\033\\");
    h_put(t, "\033_Gm=0;////\033\\");
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(pixel(&v, 0, 1), 0x0000FF);
    CHECK_INT(pixel(&v, 1, 1), 0xFFFFFF);
    vt_free(t);
}

static void zlib_data_is_inflated(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    h_put(t, "\033_Ga=T,f=24,s=2,v=2,o=z;");
    h_put(t, rgb2x2_z);
    h_put(t, "\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    CHECK_INT(pixel(&v, 1, 1), 0xFFFFFF);
    h_reply_clear();
    h_put(t, "\033_Ga=T,f=24,s=2,v=2,o=z,i=4;AAAAAAAA\033\\");
    CHECK_STR(h_reply, "\033_Gi=4;EINVAL:broken zlib data\033\\");
    vt_free(t);
}

/* png_decode on a vector: 1 and exactly the expected words. */
static void png_gives(const unsigned char *d, long n, int w, int h, const unsigned long *px)
{
    vt_u32 *out = 0;
    int ow = 0, oh = 0, i;
    CHECK_INT(png_decode(d, n, &out, &ow, &oh), 1);
    CHECK_INT(ow, w);
    CHECK_INT(oh, h);
    if (out && ow == w && oh == h)
        for (i = 0; i < w * h; i++)
            if ((unsigned long)out[i] != px[i]) {
                CHECK_INT((long)out[i], (long)px[i]); /* names the pixel's value */
                break;
            }
    free(out);
}

#define PNG_GIVES(name, w, h) png_gives(png_##name, sizeof(png_##name), w, h, png_##name##_px)

static void own_decoder_reads_every_colour_type_depth_and_filter(void)
{
    PNG_GIVES(grey8, 3, 5);        /* type 0, depth 8, filters 0-4 */
    PNG_GIVES(grey1, 10, 2);       /* depth 1, a row over two bytes, filters 2 3 */
    PNG_GIVES(grey16, 2, 1);       /* depth 16 keeps the high byte; tRNS key clear */
    PNG_GIVES(rgb8, 3, 5);         /* type 2, filters 0-4, tRNS colour key */
    PNG_GIVES(rgb16, 2, 2);        /* type 2, depth 16, Paeth */
    PNG_GIVES(pal2, 5, 2);         /* type 3, depth 2, PLTE + tRNS, IDAT in two chunks */
    PNG_GIVES(grey_alpha8, 3, 2);  /* type 4 */
    PNG_GIVES(rgba8, 3, 5);        /* type 6, filters 0-4, IDAT in two chunks */
}

static void own_decoder_answers_0_broken_minus_1_unsupported_minus_2_too_big(void)
{
    vt_u32 *out = 0;
    int w, h;
    unsigned char cut[sizeof(png_rgba8)];
    CHECK_INT(png_decode(png_interlaced, sizeof(png_interlaced), &out, &w, &h), -1);
    CHECK_INT(png_decode((const vt_u8 *)"GIF89a......", 12, &out, &w, &h), -1); /* not a PNG */
    CHECK_INT(png_decode(png_broken, sizeof(png_broken), &out, &w, &h), 0);
    memcpy(cut, png_rgba8, sizeof(cut));
    CHECK_INT(png_decode(cut, 40, &out, &w, &h), 0);                          /* cut inside a chunk */
    cut[16] = 0xFF;                                                           /* a width of 2^24 */
    CHECK_INT(png_decode(cut, sizeof(cut), &out, &w, &h), -2);                /* over the limit */
    CHECK(!out);
}

/* The whole path: APC G, the engine's decoder, an image on the screen; the
 * host's decoder is not asked for a PNG it can read or one it cannot. */
static void png_is_decoded_by_the_engine_and_never_reaches_the_host(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    char buf[2048];
    decode_calls = 0;
    vt_set_image_decoder(t, stand_in_decode, stand_in_free);
    h_reply_clear();
    snprintf(buf, sizeof(buf), "\033_Ga=T,f=100,i=21;%s\033\\", png_rgb_cube_b64);
    h_put(t, buf);
    CHECK_STR(h_reply, "\033_Gi=21;OK\033\\");
    CHECK_INT(vt_kitty_png_own(t), 1);
    CHECK_INT(decode_calls, 0);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 2);
    CHECK_INT(v.h, 2);
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    CHECK_INT(pixel(&v, 1, 0), 0x00FF00);
    CHECK_INT(pixel(&v, 0, 1), 0x0000FF);
    CHECK_INT(pixel(&v, 1, 1), 0xFFFFFF);
    /* broken: EBADPNG, and the host decoder is not called */
    h_reply_clear();
    snprintf(buf, sizeof(buf), "\033_Ga=T,f=100,i=22;%s\033\\", png_broken_b64);
    h_put(t, buf);
    CHECK_STR(h_reply, "\033_Gi=22;EBADPNG:not a picture\033\\");
    CHECK_INT(decode_calls, 0);
    CHECK_INT(vt_kitty_png_own(t), 1);
    /* interlaced: the engine passes, the host decoder is called once */
    h_reply_clear();
    snprintf(buf, sizeof(buf), "\033_Ga=T,f=100,i=23;%s\033\\", png_interlaced_b64);
    h_put(t, buf);
    CHECK_INT(decode_calls, 1);
    CHECK_STR(h_reply, "\033_Gi=23;OK\033\\");
    CHECK_INT(vt_kitty_png_own(t), 1);
    /* too big: ENOMEM at once, the host decoder is not called (it would
     * unpack the whole picture only to be refused) */
    {
        char big[sizeof(png_rgb_cube_b64)];
        memcpy(big, png_rgb_cube_b64, sizeof(big));
        memcpy(big + 20, "Uv8A", 4); /* byte 16, the width's top byte, 0xFF */
        h_reply_clear();
        snprintf(buf, sizeof(buf), "\033_Ga=T,f=100,i=24;%s\033\\", big);
        h_put(t, buf);
        CHECK_STR(h_reply, "\033_Gi=24;ENOMEM:too big\033\\");
        CHECK_INT(decode_calls, 1);
    }
    vt_free(t);
}

/* "iVBORw0K" is 89 50 4E 47 0D 0A: no whole PNG signature, so the engine
 * answers -1 (not mine) and the picture goes to the host decoder; a file
 * of another format the datatypes know takes the same way. */
static void png_goes_through_the_host_decoder(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    decode_calls = 0;
    h_reply_clear();
    h_put(t, "\033_Ga=T,f=100,i=8;iVBORw0K\033\\"); /* no decoder yet */
    CHECK_STR(h_reply, "\033_Gi=8;EINVAL:no PNG decoder\033\\");
    CHECK_INT(vt_images(t), 0);
    vt_set_image_decoder(t, stand_in_decode, stand_in_free);
    h_reply_clear();
    h_put(t, "\033_Ga=T,f=100,i=8;iVBORw0K\033\\");
    CHECK_INT(decode_calls, 1);
    CHECK_STR(h_reply, "\033_Gi=8;OK\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 2);
    CHECK_INT(v.h, 1);
    CHECK_INT(pixel(&v, 0, 0), 0x112233); /* two colours: the exact palette (was 0x333333, the cube) */
    CHECK_INT(v.pix[1], 0);               /* the clear pixel */
    vt_free(t);
}

static void query_answers_without_showing(void)
{
    vt_term *t = kitty_term(10, 5);
    h_reply_clear();
    h_put(t, "\033_Ga=q,i=31,f=24,s=1,v=1;AAAA\033\\");
    CHECK_STR(h_reply, "\033_Gi=31;OK\033\\");
    CHECK_INT(vt_images(t), 0);
    vt_free(t);
}

static void quiet_keys_hide_the_replies(void)
{
    vt_term *t = kitty_term(10, 5);
    h_reply_clear();
    h_put(t, "\033_Ga=q,i=1,q=1,f=24,s=1,v=1;AAAA\033\\");
    CHECK_STR(h_reply, "");
    h_put(t, "\033_Ga=p,i=77,q=1\033\\");
    CHECK_STR(h_reply, "\033_Gi=77;ENOENT:no such image\033\\"); /* q=1 hides OK only */
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=77,q=2\033\\");
    CHECK_STR(h_reply, "");
    vt_free(t);
}

static void delete_by_id_removes_the_image(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    h_put(t, "\033_Ga=T,i=5,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK(vt_row_image(t, 0, 0, &v)); /* shown (and kept: 2 alive) */
    h_put(t, "\033_Ga=d,d=i,i=5\033\\");
    CHECK(!vt_row_image(t, 0, 0, &v));
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=5\033\\"); /* the data stays stored: lower case keeps it */
    CHECK_STR(h_reply, "\033_Gi=5;OK\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    h_put(t, "\033_Ga=d,d=I,i=5\033\\");
    CHECK_INT(vt_images(t), 0);
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=5\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;ENOENT:no such image\033\\");
    vt_free(t);
}

/* R3C-UD3-PLACEMENT-STALE: a=p of a placement that is on the screen moves
 * it, a=d removes it; the renderer must be told to draw the old cells again
 * or the old picture stays on the screen (rig 96, UPDemo UD3). */
static void moved_and_deleted_placements_damage_their_old_cells(void)
{
    vt_term *t = kitty_term(20, 10);
    vt_image_view v;
    h_put(t, "\033_Ga=t,i=7,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    h_put(t, "\033[2;3H\033_Ga=p,i=7,p=2,c=3,r=2,C=1,q=2\033\\"); /* rows 1-2, columns 2-4 */
    CHECK(vt_row_image(t, 1, 0, &v));
    h_put(t, "\033[6;3H");
    h_damage_clear();
    h_put(t, "\033_Ga=p,i=7,p=2,c=3,r=2,C=1,q=2\033\\"); /* the same placement, rows 5-6 */
    CHECK(!vt_row_image(t, 1, 0, &v));
    CHECK(vt_row_image(t, 5, 0, &v));
    CHECK(h_damaged(2, 1, 5)); /* the old cells: drawn again, the picture gone */
    CHECK(h_damaged(2, 2, 5));
    CHECK(h_damaged(2, 5, 5)); /* the new ones */
    h_damage_clear();
    h_put(t, "\033_Ga=d,d=i,i=7,p=2\033\\");
    CHECK(!vt_row_image(t, 5, 0, &v));
    CHECK(h_damaged(2, 5, 5));
    CHECK(h_damaged(2, 6, 5));
    vt_free(t);
}

/* C=1 at the bottom: the cursor does not move, so nothing scrolls; the
 * picture is cut at the screen's last row (kitty). The AmiExpress LiveChat
 * door places half-scrolled pictures 64 rows tall with C=1 and the whole
 * door scrolled up with each one. */
static void c_equals_1_past_the_bottom_cuts_and_never_scrolls(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    int x, y;
    h_put(t, "TOP");
    h_put(t, "\033_Ga=t,i=7,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    h_put(t, "\033[4;2H\033_Ga=p,i=7,c=2,r=4,C=1,q=2\033\\"); /* rows 3-6 of 0-4 */
    CHECK(h_cell(t, 0, 0)->ch == 'T'); /* not scrolled */
    CHECK(vt_row_image(t, 3, 0, &v));
    CHECK(vt_row_image(t, 4, 0, &v));
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 1);
    CHECK_INT(y, 3);
    vt_free(t);
}

static void putting_an_unknown_id_says_enoent(void)
{
    vt_term *t = kitty_term(10, 5);
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=99\033\\");
    CHECK_STR(h_reply, "\033_Gi=99;ENOENT:no such image\033\\");
    vt_free(t);
}

static void a_picture_over_the_memory_limit_says_enomem(void)
{
    vt_term *t = kitty_term(10, 5);
    h_reply_clear();
    h_put(t, "\033_Ga=T,i=2,f=32,s=1025,v=1025;AAAA\033\\");
    CHECK_STR(h_reply, "\033_Gi=2;ENOMEM:too big\033\\");
    CHECK_INT(vt_images(t), 0);
    vt_free(t);
}

static void stored_pictures_give_way_to_new_ones(void)
{
    vt_term *t = kitty_term(10, 5);
    char put[] = "\033_Ga=t,i=0,f=24,s=1000,v=1000,o=z;", ok[] = "\033_Gi=0;OK\033\\";
    int i, j;
    for (i = 1; i <= 3; i++) {
        h_reply_clear();
        put[9] = (char)('0' + i);
        h_put(t, put);
        for (j = 0; black1000_z[j]; j++)
            h_put(t, black1000_z[j]);
        h_put(t, "\033\\");
        ok[5] = (char)('0' + i);
        CHECK_STR(h_reply, ok);
    }
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=1\033\\"); /* 3 x 1 MB does not fit in 2 MB: the oldest went */
    CHECK_STR(h_reply, "\033_Gi=1;ENOENT:no such image\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=3\033\\");
    CHECK_STR(h_reply, "\033_Gi=3;OK\033\\");
    vt_free(t);
}

/* K25-K28: a stand-in file reader that counts its calls and records its arguments. */
static int rd_calls, ul_calls, rd_fail, rel_calls;
static long rd_off, rd_size;
static char rd_path[256], ul_path[256];
static vt_u8 rd_bytes[12] = { 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255 }; /* 2x2 RGB */
static int stand_in_read(const char *path, long off, long size, vt_u8 **buf, long *n)
{
    rd_calls++;
    rd_off = off;
    rd_size = size;
    snprintf(rd_path, sizeof(rd_path), "%s", path);
    if (rd_fail)
        return 0;
    *buf = rd_bytes;
    *n = sizeof(rd_bytes);
    return 1;
}
static int stand_in_unlink(const char *path)
{
    ul_calls++;
    snprintf(ul_path, sizeof(ul_path), "%s", path);
    return 1;
}
static void counting_free(vt_u32 *p) { (void)p; rel_calls++; }

/* base64 of a path, as the client sends it */
static void kf_send(vt_term *t, const char *keys, const char *path)
{
    static const char tb[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char buf[512], *o = buf;
    int n = (int)strlen(path), i;
    o += snprintf(buf, sizeof(buf), "\033_G%s;", keys);
    for (i = 0; i < n; i += 3) {
        unsigned v = ((unsigned char)path[i] << 16) | (i + 1 < n ? (unsigned char)path[i + 1] << 8 : 0) |
                     (i + 2 < n ? (unsigned char)path[i + 2] : 0);
        *o++ = tb[(v >> 18) & 63];
        *o++ = tb[(v >> 12) & 63];
        *o++ = i + 1 < n ? tb[(v >> 6) & 63] : '=';
        *o++ = i + 2 < n ? tb[v & 63] : '=';
    }
    *o = 0;
    h_put(t, buf);
    h_put(t, "\033\\");
}

static vt_term *kf_term(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_set_image_decoder(t, stand_in_decode, counting_free);
    vt_set_file_reader(t, stand_in_read, stand_in_unlink);
    rd_calls = ul_calls = rd_fail = rel_calls = 0;
    rd_off = rd_size = -1;
    h_reply_clear();
    return t;
}

static void k25_t_f_reads_through_the_host_reader(void)
{
    vt_term *t = kf_term();
    vt_image_view v;
    kf_send(t, "a=T,t=f,i=3,f=24,s=2,v=2,O=5,S=7", "Work:pics/x.rgb");
    CHECK_INT(rd_calls, 1);
    CHECK_STR(rd_path, "Work:pics/x.rgb");
    CHECK_INT(rd_off, 5);
    CHECK_INT(rd_size, 7);
    CHECK_STR(h_reply, "\033_Gi=3;OK\033\\");
    CHECK_INT(vt_images(t), 2); /* the stored picture and its placement */
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    CHECK_INT(pixel(&v, 1, 1), 0xFFFFFF);
    CHECK_INT(rel_calls, 1); /* the buffer went back through the release hook */
    /* no S=: to the end */
    kf_send(t, "a=T,t=f,i=4,f=24,s=2,v=2", "RAM:y");
    CHECK_INT(rd_size, 4L * 1024L * 1024L); /* no S=: the engine asks for the most it takes (KIT_RAW_MAX) */
    CHECK_INT(rd_off, 0);
    /* a reader failure: exactly the one message */
    h_reply_clear();
    rd_fail = 1;
    kf_send(t, "a=T,t=f,i=5,f=24,s=2,v=2", "RAM:gone");
    CHECK_STR(h_reply, "\033_Gi=5;EBADF:Failed to read image file\033\\");
    /* no reader set: the same */
    vt_set_file_reader(t, 0, 0);
    h_reply_clear();
    kf_send(t, "a=T,t=f,i=6,f=24,s=2,v=2", "RAM:y");
    CHECK_STR(h_reply, "\033_Gi=6;EBADF:Failed to read image file\033\\");
    CHECK_INT(vt_images(t), 4);
    vt_free(t);
}

static void k26_t_t_unlinks_only_kitty_named_files_after_a_good_read(void)
{
    vt_term *t = kf_term();
    kf_send(t, "a=T,t=t,i=1,f=24,s=2,v=2", "T:plain.rgb");
    CHECK_INT(rd_calls, 1);
    CHECK_INT(ul_calls, 0); /* no marker in the name */
    kf_send(t, "a=T,t=t,i=2,f=24,s=2,v=2", "T:tty-graphics-protocol-1.rgb");
    CHECK_INT(ul_calls, 1);
    CHECK_STR(ul_path, "T:tty-graphics-protocol-1.rgb");
    kf_send(t, "a=T,t=f,i=3,f=24,s=2,v=2", "T:tty-graphics-protocol-2.rgb");
    CHECK_INT(rd_calls, 3);
    CHECK_INT(ul_calls, 1); /* t=f never deletes */
    rd_fail = 1;
    kf_send(t, "a=T,t=t,i=4,f=24,s=2,v=2", "T:tty-graphics-protocol-3.rgb");
    CHECK_INT(ul_calls, 1); /* a failed read never deletes */
    vt_free(t);
}

static void k28_t_s_shared_memory_says_ebadf(void)
{
    vt_term *t = kf_term();
    kf_send(t, "a=T,t=s,i=3,f=24,s=1,v=1", "/vtcon-shm");
    CHECK_STR(h_reply, "\033_Gi=3;EBADF:Failed to read image file\033\\");
    CHECK_INT(rd_calls, 0);
    CHECK_INT(vt_images(t), 0);
    vt_free(t);
}

static void c_equals_1_keeps_the_cursor(void)
{
    vt_term *t = kitty_term(10, 5);
    int x, y;
    h_put(t, "ab\033_Ga=T,C=1,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK_INT(vt_images(t), 1);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(y, 0);
    vt_free(t);
}

static void columns_and_rows_scale_the_picture(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    h_put(t, "\033_Ga=T,f=24,s=2,v=2,c=3,r=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 6);  /* 3 cells of 2 pixels */
    CHECK_INT(v.h, 12); /* 2 rows of 6 */
    vt_free(t);
}

static void apc_g_is_not_reported_unhandled(void)
{
    vt_term *t = kitty_term(10, 5);
    const char *kinds[4];
    long counts[4];
    h_put(t, "\033_Ga=T,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK_INT(vt_unhandled(t, kinds, counts, 4), 0);
    vt_free(t);
}

/* ---- K19-K24: protocol completion (s48) ---- */

static int shown(vt_term *t)
{
    vt_image_view v;
    int r, i, n = 0;
    for (r = 0; r < 8; r++)
        for (i = 0; vt_row_image(t, r, i, &v); i++)
            n++;
    return n;
}

/* a 2 x 2 picture (one cell) at cell (col, row), 1-based, with the keys */
static void put_at(vt_term *t, int col, int row, const char *keys)
{
    char b[96] = "\033[r;cH\033_Ga=T,C=1,f=24,s=2,v=2,"; /* single digits */
    b[2] = (char)('0' + row);
    b[4] = (char)('0' + col);
    strcat(b, keys);
    strcat(b, ";");
    h_put(t, b);
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
}

static void k19_signed_keys_keep_their_sign(void)
{
    vt_term *t = kitty_term(10, 5);
    put_at(t, 1, 1, "i=1,z=-1");
    CHECK_INT((int)vt_kitty_last_z(t), -1);
    put_at(t, 1, 2, "i=2,z=7");
    CHECK_INT((int)vt_kitty_last_z(t), 7);
    h_reply_clear();
    h_put(t, "\033_Ga=q,i=4294967295,f=24,s=1,v=1;AAAA\033\\"); /* the ids stay unsigned */
    CHECK_STR(h_reply, "\033_Gi=4294967295;OK\033\\");
    vt_free(t);
}

static void k20_bad_key_combinations_are_einval(void)
{
    vt_term *t = kitty_term(10, 5);
    unsigned long placed;
    h_reply_clear();
    h_put(t, "\033_Ga=T,i=3,I=4,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK_STR(h_reply, "\033_Gi=3,I=4;EINVAL:i and I together\033\\");
    CHECK_INT(vt_images(t), 0);
    h_reply_clear();
    h_put(t, "\033_Ga=p\033\\");
    CHECK_STR(h_reply, "\033_G;EINVAL:no image id\033\\");
    /* a delete ends an open upload: the closing chunk finds none */
    placed = vt_kitty_placed(t);
    h_put(t, "\033_Ga=T,i=8,f=24,s=2,v=2,m=1;/wAAAP8A\033\\");
    h_put(t, "\033_Ga=d\033\\");
    h_reply_clear();
    h_put(t, "\033_Gm=0;AAD/////\033\\");
    CHECK_INT(vt_images(t), 0);
    CHECK_INT((int)(vt_kitty_placed(t) - placed), 0);
    vt_free(t);
}

static void k21_retransmitting_an_id_removes_its_placements(void)
{
    vt_term *t = kitty_term(10, 5);
    vt_image_view v;
    put_at(t, 1, 1, "i=5");
    CHECK_INT(shown(t), 1);
    h_put(t, "\033[2;1H\033_Ga=T,i=5,f=32,s=2,v=2;");
    h_put(t, rgba2x2);
    h_put(t, "\033\\");
    CHECK_INT(shown(t), 1);
    CHECK(!vt_row_image(t, 0, 0, &v));
    vt_free(t);
}

static void k22_transient_pictures_are_evicted_first(void)
{
    vt_term *t = kitty_term(10, 5);
    char put[] = "\033_Ga=t,i=0,f=24,s=1000,v=1000,o=z;", putn[] = "\033_Ga=t,i=0,N=1,f=24,s=1000,v=1000,o=z;";
    int i, j;
    for (i = 1; i <= 3; i++) {
        char *c = i == 2 ? putn : put;
        c[9] = (char)('0' + i);
        h_put(t, c);
        for (j = 0; black1000_z[j]; j++)
            h_put(t, black1000_z[j]);
        h_put(t, "\033\\");
    }
    CHECK_INT(vt_images(t), 2);
    h_put(t, "\033_Ga=d,d=I,i=2\033\\"); /* the transient one went: nothing to free */
    CHECK_INT(vt_images(t), 2);
    h_put(t, "\033_Ga=d,d=I,i=1\033\\"); /* the older plain one stayed */
    CHECK_INT(vt_images(t), 1);
    vt_free(t);
}

static void k23_pixel_offset_inside_the_first_cell(void)
{
    vt_term *t = h_new(10, 5, VT_XTERM);
    vt_image_view v;
    int n = 0, x;
    vt_set_cell_pixels(t, 8, 16);
    /* 14 x 1 black, X=3 Y=5: 3 + 14 = 17 pixels -> 3 cells (2 without the offset) */
    h_put(t, "\033_Ga=T,f=24,s=14,v=1,X=3,Y=5;AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.px0, 3);
    CHECK_INT(v.py0, 5);
    CHECK_INT(v.py, -5);
    for (x = 0; x < 10; x++)
        n += (h_cell(t, x, 0)->pad & VT_CELL_IMAGE) ? 1 : 0;
    CHECK_INT(n, 3);
    CHECK_INT(h_cell(t, 3, 0)->pad & VT_CELL_IMAGE, 0);
    h_put(t, "\033[2J\033[H\033_Ga=T,f=24,s=14,v=1,X=99,Y=99;AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.px0, 7); /* clamped to cell - 1 */
    CHECK_INT(v.py0, 15);
    vt_free(t);
}

/* three one-cell pictures: id 1 at (1,1) z 0, id 2 at (3,2) z 5, id 3 at (5,3) z -2 */
typedef struct {
    const char *cmd;
    int shown, total; /* after: on the screen, alive (stored ones count) */
} kdel_row;

static void k24_the_missing_deletes(void)
{
    static const kdel_row rows[] = {
        { "d=p,x=3,y=2", 2, 5 },   { "d=P,x=3,y=2", 2, 4 },        { "d=q,x=3,y=2,z=5", 2, 5 },
        { "d=q,x=3,y=2,z=4", 3, 6 }, { "d=Q,x=3,y=2,z=5", 2, 4 }, { "d=r,x=1,y=2", 1, 4 },
        { "d=R,x=1,y=2", 1, 2 },   { "d=x,x=5", 2, 5 },             { "d=X,x=5", 2, 4 },
        { "d=y,y=1", 2, 5 },       { "d=Y,y=1", 2, 4 },             { "d=z,z=-2", 2, 5 },
        { "d=Z,z=-2", 2, 4 },
    };
    size_t n;
    for (n = 0; n < sizeof(rows) / sizeof(rows[0]); n++) {
        vt_term *t = kitty_term(10, 5);
        char b[64];
        put_at(t, 1, 1, "i=1,z=0");
        put_at(t, 3, 2, "i=2,z=5");
        put_at(t, 5, 3, "i=3,z=-2");
        CHECK_INT(shown(t), 3);
        CHECK_INT(vt_images(t), 6);
        strcpy(b, "\033_Ga=d,");
        strcat(b, rows[n].cmd);
        strcat(b, "\033\\");
        h_put(t, b);
        if (shown(t) != rows[n].shown || vt_images(t) != rows[n].total)
            printf("  row %s: shown %d (want %d) total %d (want %d)\n", rows[n].cmd, shown(t),
                   rows[n].shown, vt_images(t), rows[n].total);
        CHECK_INT(shown(t), rows[n].shown);
        CHECK_INT(vt_images(t), rows[n].total);
        vt_free(t);
    }
}

/* ---- K29: relative placements (P= Q= H= V=) ---- */

static int is_image(vt_term *t, int x, int y)
{
    return (h_cell(t, x, y)->pad & VT_CELL_IMAGE) != 0;
}

/* a one-cell picture with the keys, at the cursor unless they name a parent */
static void put_rel(vt_term *t, const char *keys)
{
    h_put(t, "\033_Ga=T,f=24,s=2,v=2,");
    h_put(t, keys);
    h_put(t, ";");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
}

/* The reachability test: escape bytes in, the child's cells out. The
 * cell (parent + H, V) can only come from the relative path: before K29
 * P/Q/H/V were ignored and the child landed at the cursor. */
static void k29_child_sits_at_the_parent_plus_h_v(void)
{
    vt_term *t = kitty_term(20, 8);
    int x, y;
    put_at(t, 3, 2, "i=1,p=1"); /* parent at column 2, row 1 (0-based) */
    CHECK(is_image(t, 2, 1));
    h_put(t, "\033[7;9H"); /* the cursor far away: column 8, row 6 */
    h_reply_clear();
    put_rel(t, "i=2,p=1,P=1,Q=1,H=2,V=1");
    CHECK_STR(h_reply, "\033_Gi=2,p=1;OK\033\\");
    CHECK(is_image(t, 4, 2));  /* 2 + 2, 1 + 1 */
    CHECK(!is_image(t, 8, 6)); /* not at the cursor */
    vt_cursor(t, &x, &y);      /* C=0, and still the cursor stays */
    CHECK_INT(x, 8);
    CHECK_INT(y, 6);
    put_rel(t, "i=3,p=1,P=2,Q=1,H=-3,V=-2"); /* signed, from the child's cell */
    CHECK(is_image(t, 1, 0));
    CHECK_INT(shown(t), 3);
    vt_free(t);
}

static void k29_deleting_a_parent_deletes_its_children(void)
{
    vt_term *t = kitty_term(20, 8);
    put_at(t, 1, 1, "i=1,p=1");
    put_rel(t, "i=2,p=1,P=1,Q=1,H=4");
    put_rel(t, "i=3,p=1,P=2,Q=1,V=2"); /* a grandchild */
    put_at(t, 1, 5, "i=4,p=1");          /* unrelated */
    CHECK_INT(shown(t), 4);
    CHECK(is_image(t, 4, 2));
    h_put(t, "\033_Ga=d,d=i,i=1\033\\");
    CHECK_INT(shown(t), 1);
    CHECK(is_image(t, 0, 4));
    vt_free(t);
}

static void k29_the_error_replies(void)
{
    vt_term *t = kitty_term(20, 8);
    int n;
    char b[64];
    h_reply_clear();
    put_rel(t, "i=2,p=1,P=9,Q=1");
    CHECK_STR(h_reply, "\033_Gi=2,p=1;ENOPARENT:no such parent placement\033\\");
    CHECK_INT(shown(t), 0);
    put_at(t, 1, 1, "i=1,p=1");
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=1,p=1,P=1,Q=1\033\\"); /* itself */
    CHECK_STR(h_reply, "\033_Gi=1,p=1;ECYCLE:the parent chain loops\033\\");
    put_rel(t, "i=2,p=1,P=1,Q=1,H=1");
    h_reply_clear();
    h_put(t, "\033_Ga=p,i=1,p=1,P=2,Q=1\033\\"); /* its parent's parent is the one being placed */
    CHECK_STR(h_reply, "\033_Gi=1,p=1;ECYCLE:the parent chain loops\033\\");
    CHECK_INT(shown(t), 2);
    /* the chain 1 <- 2 <- ... <- 9 is 8 deep; the 10th is one too many */
    for (n = 3; n <= 10; n++) {
        static const char *const id[] = { "", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
        strcpy(b, "i=");
        strcat(b, id[n]);
        strcat(b, ",p=1,P=");
        strcat(b, id[n - 1]);
        strcat(b, ",Q=1,H=1");
        h_reply_clear();
        put_rel(t, b);
    }
    CHECK_STR(h_reply, "\033_Gi=10,p=1;ETOODEEP:more than 8 parents deep\033\\");
    CHECK_INT(shown(t), 9);
    vt_free(t);
}

static void k29_virtual_placements_and_parents(void)
{
    vt_term *t = kitty_term(20, 8);
    put_rel(t, "i=5,p=1,U=1"); /* a virtual parent: none until K31 */
    h_reply_clear();
    put_rel(t, "i=6,p=1,P=5,Q=1");
    CHECK_STR(h_reply, "\033_Gi=6,p=1;ENOPARENT:no such parent placement\033\\");
    put_at(t, 1, 1, "i=1,p=1");
    h_reply_clear();
    put_rel(t, "i=7,p=1,U=1,P=1,Q=1");
    CHECK_STR(h_reply, "\033_Gi=7,p=1;EINVAL:a virtual placement has no parent\033\\");
    vt_free(t);
}


/* K31: a=T,U=1 stores a virtual placement and puts nothing on the screen;
 * cells of U+10EEEE with the row and column marks (U+0305 = 0, U+030D = 1),
 * fg 38;5;7 = image id 7, each resolve to one tile of the 2 x 2 cell
 * picture. Before K31 U=1 answered EINVAL:no unicode placeholders. */
#define PH "\xF4\x8E\xBB\xAE"
#define M0 "\xCC\x85"
#define M1 "\xCC\x8D"
static int ph_tile(vt_term *t, int col, int row, int *tc, int *tr, vt_image_view *v)
{
    return vt_cell_placeholder(t, row, col, v, tc, tr);
}

static void k31_placeholder_cells_resolve_to_tiles(void)
{
    vt_term *t = kitty_term(20, 8);
    vt_image_view v, w;
    int tc = -1, tr = -1, cx, cy;
    unsigned long n0;
    h_reply_clear();
    h_put(t, "\033_Ga=T,U=1,i=7,f=24,s=2,v=2,c=2,r=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK_STR(h_reply, "\033_Gi=7;OK\033\\");
    vt_cursor(t, &cx, &cy);
    CHECK_INT(cx, 0);
    CHECK_INT(cy, 0);
    CHECK(!vt_row_image(t, 0, 0, &v)); /* nothing on the cells */
    h_put(t, "\033[2;1H\033[38;5;7m" PH M0 M0 PH M0 M1 "\033[3;1H" PH M1 M0 PH M1 M1);
    n0 = vt_kitty_placeholders(t);
    CHECK(ph_tile(t, 0, 1, &tc, &tr, &v));
    CHECK_INT(tc, 0);
    CHECK_INT(tr, 0);
    CHECK_INT(v.w, 4); /* scaled to 2 x 2 cells of 2 x 6 pixels */
    CHECK_INT(v.h, 12);
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000); /* red quarter */
    CHECK(ph_tile(t, 1, 1, &tc, &tr, &w));
    CHECK_INT(tc, 1);
    CHECK_INT(tr, 0);
    CHECK_INT(pixel(&w, tc * 2, tr * 6), 0x00FF00);
    CHECK(ph_tile(t, 0, 2, &tc, &tr, &w));
    CHECK_INT(pixel(&w, tc * 2, tr * 6), 0x0000FF);
    CHECK(ph_tile(t, 1, 2, &tc, &tr, &w));
    CHECK_INT(tc, 1);
    CHECK_INT(tr, 1);
    CHECK_INT(pixel(&w, tc * 2, tr * 6), 0xFFFFFF);
    CHECK_INT((long)(vt_kitty_placeholders(t) - n0), 4); /* the sentinel: four cells resolved */
    /* marks only on the first cell: the next two are column 1, then past the picture */
    h_put(t, "\033[5;1H" PH M0 M0 PH PH);
    CHECK(ph_tile(t, 1, 4, &tc, &tr, &w));
    CHECK_INT(tc, 1);
    CHECK_INT(tr, 0);
    CHECK(!ph_tile(t, 2, 4, &tc, &tr, &w));
    /* another colour is another image: nothing known as 8 */
    h_put(t, "\033[7;1H\033[38;5;8m" PH M0 M0);
    CHECK(!ph_tile(t, 0, 6, &tc, &tr, &w));
    /* d=a leaves the virtual placement; d=i,i=7 takes it */
    h_put(t, "\033_Ga=d,d=a\033\\");
    CHECK(ph_tile(t, 0, 1, &tc, &tr, &w));
    h_put(t, "\033_Ga=d,d=i,i=7\033\\");
    CHECK(!ph_tile(t, 0, 1, &tc, &tr, &w));
    vt_free(t);
}

/* the third mark is the id's high byte: fg RGB 0,0,7 plus U+030D (1) is id 0x01000007 */
static void k31_third_mark_is_the_ids_high_byte(void)
{
    vt_term *t = kitty_term(20, 8);
    vt_image_view v;
    int tc, tr;
    h_put(t, "\033_Ga=T,U=1,i=16777223,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    h_put(t, "\033[2;1H\033[38;2;0;0;7m" PH M0 M0 M1);
    CHECK(vt_cell_placeholder(t, 1, 0, &v, &tc, &tr));
    h_put(t, "\033[3;1H" PH M0 M0); /* without it: id 7, unknown */
    CHECK(!vt_cell_placeholder(t, 2, 0, &v, &tc, &tr));
    vt_free(t);
}

/* K30: the images of a row come out by z, then i=, whatever their age.
 * Before K30 they came in age order (5, 2, 9, 7 here) and had no z. */
static void k30_row_images_go_by_z_then_id(void)
{
    vt_term *t = kitty_term(20, 8);
    vt_image_view v[4];
    int i;
    /* four cells of one row: a picture written over a picture's cell would replace it */
    put_at(t, 1, 1, "i=5,p=1,z=3");
    put_at(t, 2, 1, "i=2,p=1,z=3");
    put_at(t, 3, 1, "i=9,p=1,z=-1");
    put_at(t, 4, 1, "i=7,p=1");
    for (i = 0; i < 4; i++)
        CHECK(vt_row_image(t, 0, i, &v[i]));
    CHECK_INT((int)v[0].z, -1);
    CHECK_INT((int)v[1].z, 0);
    CHECK_INT((int)v[2].z, 3);
    CHECK_INT((int)v[3].z, 3);
    CHECK(v[2].serial > v[3].serial); /* i=2 is the newer, yet goes first */
    vt_free(t);
}

static int is_under(vt_term *t, int x, int y)
{
    return (h_cell(t, x, y)->pad & VT_CELL_UNDER) != 0;
}

/* K30: a z<0 picture leaves the text in its cells, and text written or
 * erased over it later keeps the picture under it. Before K30 the
 * picture blanked the cells and the text took its place. */
static void k30_text_over_a_picture_below_keeps_it(void)
{
    vt_term *t = kitty_term(20, 8);
    vt_image_view v;
    h_put(t, "\033[1;1HXY");
    put_at(t, 1, 1, "i=1,p=1,z=-1,c=6");
    CHECK(h_cell(t, 0, 0)->ch == 'X'); /* the text stays */
    CHECK(h_cell(t, 1, 0)->ch == 'Y');
    CHECK(is_under(t, 0, 0) && is_under(t, 5, 0) && !is_under(t, 6, 0));
    CHECK(!is_image(t, 0, 0));
    h_put(t, "\033[1;3Ha\033[31mb\033[0mcdef"); /* one cell, a run, the colour path */
    CHECK(h_cell(t, 2, 0)->ch == 'a');
    CHECK(h_cell(t, 3, 0)->ch == 'b');
    CHECK(is_under(t, 2, 0) && is_under(t, 3, 0) && is_under(t, 5, 0));
    h_put(t, "\033[1;5H\033[K"); /* erase to the end: the picture stays */
    CHECK(h_cell(t, 4, 0)->ch == ' ');
    CHECK(is_under(t, 4, 0) && is_under(t, 5, 0));
    h_put(t, "\033[1;1H\033[2X"); /* erase characters */
    CHECK(is_under(t, 0, 0));
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT((int)v.z, -1);
    h_put(t, "\033[2K"); /* the whole line: the picture goes, as for z>=0 */
    CHECK(!vt_row_image(t, 0, 0, &v));
    vt_free(t);
}

/* K40d (O6): a kitty picture with z >= 0 lies over the text as in kitty:
 * its cells keep their glyph and colours (its translucent pixels show
 * them) and gain VT_CELL_IMAGE. Text written there later keeps the picture
 * over it, as in kitty (D12). A sixel over the same text still blanks its cells. Before
 * K40d the kitty picture blanked the cells to the default colours too. */
static void k40d_kitty_front_picture_keeps_the_cells(void)
{
    vt_term *t = kitty_term(20, 8);
    vt_image_view v;
    h_put(t, "\033[1;1H\033[31;44mXY\033[0m");
    put_at(t, 1, 1, "i=1,p=1,z=5,c=4");
    CHECK(is_image(t, 0, 0) && is_image(t, 3, 0) && !is_image(t, 4, 0) && !is_under(t, 0, 0));
    CHECK(h_cell(t, 0, 0)->ch == 'X'); /* the glyph stays */
    CHECK(h_cell(t, 1, 0)->ch == 'Y');
    CHECK(h_cell(t, 0, 0)->fg == 1); /* and its colours */
    CHECK(h_cell(t, 0, 0)->bg == 4);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT((int)v.z, 5);
    h_put(t, "\033[1;2Hq"); /* D12: text written over the picture keeps the picture over it */
    CHECK(h_cell(t, 1, 0)->ch == 'q' && is_image(t, 1, 0));
    CHECK(is_image(t, 0, 0) && is_image(t, 2, 0));
    vt_free(t);
    /* text written into a z=1 picture's cells later: the cells keep VT_CELL_IMAGE and store the glyphs
     * (the one-character, run and put_char paths all keep it) */
    t = kitty_term(20, 8);
    put_at(t, 1, 1, "i=1,p=1,z=1,c=6,r=2");
    CHECK(is_image(t, 0, 0) && is_image(t, 5, 1));
    h_put(t, "\033[1;1Hab");
    h_put(t, "\033[2;1H\033[31mc\033[0m\033[2;3H\xe2\x82\xac");
    CHECK(h_cell(t, 0, 0)->ch == 'a' && is_image(t, 0, 0));
    CHECK(h_cell(t, 1, 0)->ch == 'b' && is_image(t, 1, 0));
    CHECK(h_cell(t, 0, 1)->ch == 'c' && is_image(t, 0, 1));
    CHECK(vt_row_image(t, 0, 0, &v) && v.z == 1);
    h_put(t, "\033_Ga=d,d=a,q=2\033\\"); /* a=d takes the picture away */
    CHECK(!vt_row_image(t, 0, 0, &v));
    CHECK(h_cell(t, 0, 0)->ch == 'a');
    vt_free(t);
    /* a sixel over the same text: the cells are blanks in the default colours */
    t = kitty_term(20, 8);
    h_put(t, "\033[1;1H\033[31;44mXY\033[0m\033[1;1H\033Pq#0;2;100;0;0#0~~~~~~~~\033\\");
    CHECK(is_image(t, 0, 0) && is_image(t, 1, 0));
    CHECK(h_cell(t, 0, 0)->ch == ' ' && h_cell(t, 1, 0)->ch == ' ');
    CHECK(h_cell(t, 0, 0)->bg == VT_COLOR_DEFAULT);
    vt_free(t);
}

/* ---- K34: animation frames (a=f) ---- */

/* cube indices of the 2 x 2 picture's colours */
#define K34_RED 181
#define K34_GREEN 31
#define K34_BLUE 6
#define K34_WHITE 216

static void k34_frames_build_on_each_other(void)
{
    vt_term *t = kitty_term(10, 5);
    unsigned long n0 = vt_kitty_frames(t);
    h_reply_clear();
    h_put(t, "\033_Ga=t,i=5,f=24,s=2,v=2;/wAAAP8AAAD/////\033\\");
    CHECK_INT((long)(vt_kitty_frames(t) - n0), 0); /* a plain picture has no frame list */
    /* frame 2: the root plus a blue pixel at (0,0) */
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=5,c=1,f=24,s=1,v=1,x=0,y=0,z=100;AAD/\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;OK\033\\");
    /* frame 3 on frame 2: a red pixel at (1,1) */
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=5,c=2,f=24,s=1,v=1,x=1,y=1;/wAA\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;OK\033\\");
    CHECK_INT((long)(vt_kitty_frames(t) - n0), 3); /* the sentinel: root and two frames */
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 0, 0), K34_RED);
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 1, 1), K34_WHITE);
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 0, 0), K34_BLUE);
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 1, 0), K34_GREEN);
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 1, 1), K34_WHITE);
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 0, 0), K34_BLUE);
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 1, 0), K34_GREEN);
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 0, 1), K34_BLUE);
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 1, 1), K34_RED); /* frame 2 plus the rectangle */
    CHECK_INT(vt_kitty_frame_px(t, 5, 4, 0, 0), -1);
    /* a half-clear rectangle pixel blends (keeps the base), X=1 overwrites with clear */
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=5,r=3,f=32,s=1,v=1,x=0,y=0;/wAA/w==\033\\");
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 0, 0), K34_RED); /* alpha 255 overwrites */
    h_put(t, "\033_Ga=f,i=5,r=3,f=32,s=1,v=1,x=1,y=0;/wAAAA==\033\\");
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 1, 0), K34_GREEN); /* alpha 0 keeps the base */
    h_put(t, "\033_Ga=f,i=5,r=3,f=32,s=1,v=1,x=1,y=0,X=1;/wAAAA==\033\\");
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 1, 0), 0); /* X=1 writes the clear pixel */
    /* errors */
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=9,f=24,s=1,v=1;AAD/\033\\");
    CHECK_STR(h_reply, "\033_Gi=9;ENOENT:no such image\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=5,f=24,s=2,v=1,x=1,y=0;AAD/AAD/\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;EINVAL:frame outside the picture\033\\");
    /* d=f drops the frames and keeps the picture; d=F also frees it */
    h_put(t, "\033_Ga=d,d=f,i=5\033\\");
    CHECK_INT((long)(vt_kitty_frames(t) - n0), 0);
    CHECK_INT(vt_images(t), 1);
    h_put(t, "\033_Ga=f,i=5,f=24,s=1,v=1;AAD/\033\\");
    CHECK_INT((long)(vt_kitty_frames(t) - n0), 2);
    h_put(t, "\033_Ga=d,d=F,i=5\033\\");
    CHECK_INT((long)(vt_kitty_frames(t) - n0), 0);
    CHECK_INT(vt_images(t), 0);
    vt_free(t);
}

static void k34_frames_stop_at_the_animation_memory(void)
{
    vt_term *t = kitty_term(10, 5);
    int j;
    h_put(t, "\033_Ga=t,i=7,f=24,s=1000,v=1000,o=z;");
    for (j = 0; black1000_z[j]; j++)
        h_put(t, black1000_z[j]);
    h_put(t, "\033\\");
    h_reply_clear();
    /* the root's canvas and one frame are 2,000,000 of VT_ANIM_MEMORY (2 MB) */
    h_put(t, "\033_Ga=f,i=7,f=24,s=1,v=1;AAD/\033\\");
    CHECK_STR(h_reply, "\033_Gi=7;OK\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=7,f=24,s=1,v=1;AAD/\033\\");
    CHECK_STR(h_reply, "\033_Gi=7;ENOSPC:no room for frames\033\\");
    CHECK_INT((long)vt_kitty_frames(t), 2);
    h_put(t, "\033_Ga=d,d=f,i=7\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=f,i=7,f=24,s=1,v=1;AAD/\033\\"); /* the budget came back */
    CHECK_STR(h_reply, "\033_Gi=7;OK\033\\");
    vt_free(t);
}


/* ---- K38: the RGBA store (vt_set_image_rgb) ---- */

static void k38_b64(const unsigned char *d, long n, char *o)
{
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    long i;
    for (i = 0; i < n; i += 3) {
        unsigned long v = (unsigned long)d[i] << 16 | (i + 1 < n ? (unsigned long)d[i + 1] << 8 : 0) |
                          (i + 2 < n ? d[i + 2] : 0);
        *o++ = a[v >> 18 & 63];
        *o++ = a[v >> 12 & 63];
        *o++ = i + 1 < n ? a[v >> 6 & 63] : '=';
        *o++ = i + 2 < n ? a[v & 63] : '=';
    }
    *o = 0;
}

/* 64 x 64 RGBA: column 0 clear (one pixel at alpha 127), the others opaque, R from x, G from y, B fixed:
 * columns 1-63 are 4032 distinct colours */
static void k38_send_gradient(vt_term *t, const char *keys)
{
    static unsigned char px[64 * 64 * 4];
    static char b64[64 * 64 * 4 / 3 * 4 + 8];
    char hd[96];
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            unsigned char *q = px + (y * 64 + x) * 4;
            q[0] = (unsigned char)(x * 4);
            q[1] = (unsigned char)(y * 4);
            q[2] = 0x80;
            q[3] = x ? 255 : (y == 63 ? 127 : 0);
        }
    k38_b64(px, sizeof(px), b64);
    snprintf(hd, sizeof(hd), "\033_G%s;", keys);
    h_put(t, hd);
    h_put(t, b64);
    h_put(t, "\033\\");
}

static vt_term *k38_term(int rgb)
{
    vt_term *t = h_new(20, 10, VT_XTERM);
    vt_set_cell_pixels(t, 8, 16);
    vt_set_image_rgb(t, rgb);
    return t;
}

static int k38_distinct(const vt_image_view *v, int x0, int x1)
{
    static unsigned long seen[4096];
    int x, y, n = 0, i;
    for (y = 0; y < v->h; y++)
        for (x = x0; x < x1; x++) {
            const unsigned char *q = v->pix + (y * v->w + x) * 4;
            unsigned long c = (unsigned long)q[0] << 16 | q[1] << 8 | q[2];
            for (i = 0; i < n && seen[i] != c; i++)
                ;
            if (i == n)
                seen[n++] = c;
        }
    return n;
}

static void k38_rgb_store_keeps_every_colour_and_the_alpha_byte(void)
{
    vt_term *t = k38_term(1);
    vt_image_view v;
    int x, y, bad = 0;
    k38_send_gradient(t, "a=T,i=5,f=32,s=64,v=64,C=1");
    CHECK_INT((long)vt_kitty_rgb_made(t), 2); /* the stored root and the placement */
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.fmt, 1);
    CHECK_INT(v.opaque, 0);
    CHECK_INT(v.w, 64);
    for (y = 0; y < 64; y++)
        for (x = 1; x < 64; x++) {
            const unsigned char *q = v.pix + (y * 64 + x) * 4;
            if (q[0] != x * 4 || q[1] != y * 4 || q[2] != 0x80 || q[3] != 255)
                bad++;
        }
    CHECK_INT(bad, 0);
    CHECK_INT(k38_distinct(&v, 1, 64), 4032);
    CHECK_INT(v.pix[3], 0);                    /* column 0 clear */
    CHECK_INT(v.pix[(63 * 64) * 4 + 3], 127);  /* a half-clear pixel keeps its byte (owner O2) */
    CHECK_INT(v.pix[4 + 3], 255);              /* column 1 opaque */
    CHECK_INT(vt_image_memory(t), 2 * 16384);
    vt_free(t);
}

static void k38_index_mode_is_unchanged(void)
{
    vt_term *t = k38_term(0);
    vt_image_view v;
    k38_send_gradient(t, "a=T,i=5,f=32,s=64,v=64,C=1");
    CHECK_INT((long)vt_kitty_rgb_made(t), 0);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.fmt, 0);
    CHECK(v.npal <= 256);
    CHECK_INT(vt_image_memory(t), 2 * 4096);
    vt_free(t);
}

static void k38_an_opaque_picture_says_so_and_a_placement_scales_the_rgba(void)
{
    vt_term *t = k38_term(1);
    vt_image_view v;
    h_put(t, "\033_Ga=T,f=24,s=2,v=2;");
    h_put(t, rgb2x2);
    h_put(t, "\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.fmt, 1);
    CHECK_INT(v.opaque, 1);
    vt_free(t);
    /* a=p c=4 (cell 8 wide): 32 x 32, every other source pixel, columns 1-31 distinct */
    t = k38_term(1);
    k38_send_gradient(t, "a=t,i=5,f=32,s=64,v=64");
    h_put(t, "\033[5;1H\033_Ga=p,i=5,c=4,C=1\033\\");
    CHECK_INT((long)vt_kitty_rgb_made(t), 2);
    CHECK(vt_row_image(t, 4, 0, &v));
    CHECK_INT(v.w, 32);
    CHECK_INT(v.h, 32);
    CHECK_INT(k38_distinct(&v, 1, 32), 992);
    vt_free(t);
}

static void k38_budget_is_x4_with_the_same_pixel_capacity(void)
{
    static const char *const mode[2] = { "index", "rgb" };
    int m;
    for (m = 0; m < 2; m++) {
        long n = 1024L * 1025L * 3L;
        unsigned char *raw = (unsigned char *)calloc(n, 1);
        char *b = (char *)malloc(n / 3 * 4 + 8);
        char *msg = (char *)malloc(n / 3 * 4 + 128);
        vt_term *t = k38_term(m);
        k38_b64(raw, 1024L * 1024L * 3L, b);
        snprintf(msg, n / 3 * 4 + 128, "\033_Ga=t,i=9,f=24,s=1024,v=1024;%s\033\\", b);
        h_reply_clear();
        h_put(t, msg);
        if (strcmp(h_reply, "\033_Gi=9;OK\033\\"))
            h_failures++, printf("[FAIL] %s mode: 1024x1024 reply '%s'\n", mode[m], h_reply);
        h_checks++;
        CHECK_INT(vt_image_memory(t), 1024L * 1024L * (m ? 4 : 1));
        k38_b64(raw, n, b);
        snprintf(msg, n / 3 * 4 + 128, "\033_Ga=t,i=10,f=24,s=1024,v=1025;%s\033\\", b);
        h_reply_clear();
        h_put(t, msg);
        if (strncmp(h_reply, "\033_Gi=10;ENOMEM", 14))
            h_failures++, printf("[FAIL] %s mode: 1024x1025 reply '%s'\n", mode[m], h_reply);
        h_checks++;
        vt_free(t);
        free(raw);
        free(b);
        free(msg);
    }
}

/* A 16 x 32 one-pixel black and white checker shrunk into one 8 x 16
 * cell: every screen pixel covers two black and two white pixels, so it
 * is grey. Taking one point per pixel made it solid black (aliasing),
 * the way photos shrunk into a few cells came out jagged and noisy. */
static void k38_send_checker(vt_term *t)
{
    static unsigned char px[16 * 32 * 3];
    static char b64[16 * 32 * 3 / 3 * 4 + 8];
    int x, y;
    for (y = 0; y < 32; y++)
        for (x = 0; x < 16; x++)
            memset(px + (y * 16 + x) * 3, (x + y) & 1 ? 255 : 0, 3);
    k38_b64(px, sizeof(px), b64);
    h_put(t, "\033_Ga=T,f=24,s=16,v=32,c=1,r=1,C=1;");
    h_put(t, b64);
    h_put(t, "\033\\");
}

static void a_shrunk_picture_averages_instead_of_aliasing(void)
{
    int m;
    for (m = 0; m <= 1; m++) {
        vt_term *t = k38_term(m);
        vt_image_view v;
        int x, y, bad = 0;
        k38_send_checker(t);
        CHECK(vt_row_image(t, 0, 0, &v));
        CHECK_INT(v.w, 8);
        CHECK_INT(v.h, 16);
        for (y = 0; y < v.h; y++)
            for (x = 0; x < v.w; x++) {
                vt_u32 c;
                int g;
                if (m) {
                    const unsigned char *q = v.pix + (y * v.w + x) * 4;
                    c = (vt_u32)q[0] << 16 | (vt_u32)q[1] << 8 | q[2];
                } else
                    c = pixel(&v, x, y);
                g = (int)(c >> 8) & 255;
                if (g < 0x60 || g > 0xA0)
                    bad++;
            }
        CHECK_INT(bad, 0);
        vt_free(t);
    }
}

/* A flat white and blue picture (the LiveChat door's logo): two colours, the blue off the 6x6x6 cube. Cut to the
 * cube with the ordered dither it showed a pattern on every screen; 255 colours or fewer keep their own, exactly.
 * With the copy on (a screen short of pens) the RGB copy is made too, for the renderer's own-colour dither. */
static void a_flat_two_colour_picture_shows_exact_colours_undithered(void)
{
    static unsigned char px[16 * 16 * 3];
    static char b64[16 * 16 * 3 / 3 * 4 + 8];
    int copy;
    for (copy = 0; copy <= 1; copy++) {
        vt_term *t = k38_term(0);
        vt_image_view v;
        int x, y, bad = 0;
        vt_set_image_rgb_copy(t, copy);
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++) {
                unsigned char *q = px + (y * 16 + x) * 3;
                q[0] = y < 8 ? 0xFF : 0x20;
                q[1] = y < 8 ? 0xFF : 0x50;
                q[2] = y < 8 ? 0xFF : 0xC8;
            }
        k38_b64(px, sizeof(px), b64);
        h_put(t, "\033_Ga=T,f=24,s=16,v=16,C=1;");
        h_put(t, b64);
        h_put(t, "\033\\");
        CHECK(vt_row_image(t, 0, 0, &v));
        CHECK_INT(v.fmt, 0);
        CHECK_INT(v.exact, 1);
        CHECK_INT(v.npal, 3); /* clear, white, blue: nothing else */
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++)
                if (pixel(&v, x, y) != (y < 8 ? 0xFFFFFFu : 0x2050C8u))
                    bad++;
        CHECK_INT(bad, 0);
        CHECK(copy ? v.rgb != 0 : v.rgb == 0);
        vt_free(t);
    }
}

/* A picture of more colours than a palette holds is still dithered to the cube (once); with the copy on, its own
 * colours beside the indices are kit_sample's average like the indices, not one point a pixel: a 64 x 64 green
 * checker over a red/blue gradient shown 64 x 32 (rows halved) averages to mid-green, a point sample is 0 or 255. */
static void a_shrunk_many_colour_picture_copies_its_averaged_colours(void)
{
    static unsigned char px[64 * 64 * 3];
    static char b64[64 * 64 * 3 / 3 * 4 + 8];
    vt_term *t = k38_term(0);
    vt_image_view v;
    int x, y, bad = 0;
    vt_set_image_rgb_copy(t, 1);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            unsigned char *q = px + (y * 64 + x) * 3;
            q[0] = (unsigned char)(x * 4);
            q[1] = (x + y) & 1 ? 255 : 0;
            q[2] = (unsigned char)(y * 4);
        }
    k38_b64(px, sizeof(px), b64);
    h_put(t, "\033_Ga=T,f=24,s=64,v=64,c=8,r=2,C=1;"); /* 64 x 32: rows halved, 2048 colours */
    h_put(t, b64);
    h_put(t, "\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.exact, 0); /* 4096 colours: the cube */
    CHECK(v.rgb != 0);
    if (v.rgb)
        for (y = 0; y < v.h; y++)
            for (x = 0; x < v.w; x++) {
                int g = v.rgb[(y * v.w + x) * 3 + 1];
                if (g < 0x60 || g > 0xA0)
                    bad++;
            }
    CHECK_INT(bad, 0);
    vt_free(t);
}

/* A frame over a root of its own colours: the frames are cube canvases, and a root colour off the cube must land on
 * the cube there, not turn clear (index 0) because no cube entry equals it. */
/* 4 x 24 RGB, 4 cell rows of 6: pixel (x, y) is R y*10, G 200-y*5, B x*60
 * (96 colours: the exact-colour path) */
static const char tall4x24[] =
    "AMgAAMg8AMh4AMi0CsMACsM8CsN4CsO0FL4AFL48FL54FL60HrkAHrk8Hrl4Hrm0KLQAKLQ8KLR4KLS0Mq8AMq88Mq94Mq+0"
    "PKoAPKo8PKp4PKq0RqUARqU8RqV4RqW0UKAAUKA8UKB4UKC0WpsAWps8Wpt4Wpu0ZJYAZJY8ZJZ4ZJa0bpEAbpE8bpF4bpG0"
    "eIwAeIw8eIx4eIy0gocAgoc8god4goe0jIIAjII8jIJ4jIK0ln0Aln08ln14ln20oHgAoHg8oHh4oHi0qnMAqnM8qnN4qnO0"
    "tG4AtG48tG54tG60vmkAvmk8vml4vmm0yGQAyGQ8yGR4yGS00l8A0l880l940l+03FoA3Fo83Fp43Fq05lUA5lU85lV45lW0";

static vt_u32 tall_px(int x, int y)
{
    return ((vt_u32)(y * 10) << 16) | ((vt_u32)(200 - y * 5) << 8) | (vt_u32)(x * 60);
}

static void store_tall(vt_term *t)
{
    h_put(t, "\033_Ga=t,i=7,f=24,s=4,v=24,q=2;");
    h_put(t, tall4x24);
    h_put(t, "\033\\");
}

/* screen row `row` shows the picture's source rows from y0 (1:1) */
static int row_shows(const vt_term *t, int row, int y0)
{
    vt_image_view v;
    int x, y;
    if (!vt_row_image(t, row, 0, &v))
        return 0;
    for (y = 0; y < 6; y++)
        for (x = 0; x < 4; x++)
            if (pixel(&v, x, v.py + y) != tall_px(x, y0 + y))
                return 0;
    return 1;
}

/* The LiveChat door scrolls a stored picture by deleting its placement
 * (a=d,d=i: the data stays) and placing the next crop of whole rows, with
 * +-1 px jitter. Each step was a new picture: scaled, quantised and
 * dithered again under a new serial (seconds on a 68020, the renderer's
 * caches missed). A band of whole rows is now rows of one scaled copy. */
static void scrolling_a_cropped_picture_does_not_rebuild_it(void)
{
    vt_term *t = kitty_term(20, 10);
    vt_image_view v;
    unsigned long made;
    long serial;
    store_tall(t);
    made = vt_kitty_pictures_made(t);
    h_put(t, "\033[1;1H\033_Ga=p,i=7,p=1,x=0,y=0,w=4,h=12,c=2,r=2,C=1,q=2\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    serial = v.serial;
    CHECK(v.h == 24 && v.py == 0);
    CHECK(row_shows(t, 0, 0) && row_shows(t, 1, 6));
    h_put(t, "\033_Ga=d,d=i,i=7\033\\");
    CHECK(!vt_row_image(t, 0, 0, &v));
    h_put(t, "\033_Ga=p,i=7,p=1,x=0,y=6,w=4,h=12,c=2,r=2,C=1,q=2\033\\");
    CHECK(vt_row_image(t, 0, 0, &v) && v.py == 6 && v.serial == serial);
    CHECK(row_shows(t, 0, 6) && row_shows(t, 1, 12));
    h_put(t, "\033_Ga=d,d=i,i=7\033\\");
    h_put(t, "\033_Ga=p,i=7,p=1,x=0,y=11,w=4,h=12,c=2,r=2,C=1,q=2\033\\"); /* -1 px: the nearest row */
    CHECK(vt_row_image(t, 0, 0, &v) && v.py == 12 && v.serial == serial);
    CHECK(row_shows(t, 0, 12) && row_shows(t, 1, 18));
    CHECK(!vt_row_image(t, 2, 0, &v));
    CHECK(vt_kitty_pictures_made(t) == made + 1);
    /* the same placement id again, no delete: replaced in place, still the one copy */
    h_put(t, "\033_Ga=p,i=7,p=1,x=0,y=0,w=4,h=12,c=2,r=2,C=1,q=2\033\\");
    CHECK(row_shows(t, 0, 0) && row_shows(t, 1, 6));
    CHECK(vt_kitty_pictures_made(t) == made + 1);
    /* deleting the data frees the copy with it */
    h_put(t, "\033_Ga=d,d=I,i=7\033\\");
    CHECK(!vt_row_image(t, 0, 0, &v));
    store_tall(t);
    made = vt_kitty_pictures_made(t);
    h_put(t, "\033_Ga=p,i=7,p=1,x=0,y=6,w=4,h=12,c=2,r=2,C=1,q=2\033\\");
    CHECK(row_shows(t, 0, 6) && vt_kitty_pictures_made(t) == made + 1);
    vt_free(t);
}

/* A crop that is not whole rows of the whole width keeps its own picture. */
static void a_crop_off_the_rows_is_still_its_own_picture(void)
{
    vt_term *t = kitty_term(20, 10);
    vt_image_view v;
    unsigned long made;
    store_tall(t);
    made = vt_kitty_pictures_made(t);
    h_put(t, "\033[1;1H\033_Ga=p,i=7,p=1,x=2,y=0,w=2,h=12,c=1,r=2,C=1,q=2\033\\"); /* the right half */
    CHECK(vt_row_image(t, 0, 0, &v) && v.w == 2 && v.h == 12);
    CHECK(pixel(&v, 0, 0) == tall_px(2, 0) && pixel(&v, 1, 7) == tall_px(3, 7));
    h_put(t, "\033_Ga=d,d=i,i=7\033\\");
    h_put(t, "\033_Ga=p,i=7,p=1,x=0,y=3,w=4,h=12,c=2,r=2,C=1,q=2\033\\"); /* half a row down */
    CHECK(vt_row_image(t, 0, 0, &v) && v.w == 4 && v.h == 12);
    CHECK(pixel(&v, 0, 0) == tall_px(0, 3) && pixel(&v, 3, 11) == tall_px(3, 14));
    CHECK(vt_kitty_pictures_made(t) == made + 2);
    vt_free(t);
}

static void a_frame_over_an_exact_colour_root_keeps_the_root_pixels(void)
{
    vt_term *t = kitty_term(10, 5);
    h_put(t, "\033_Ga=t,i=5,f=24,s=2,v=2;IFDIIFDIIFDIIFDI\033\\"); /* 2 x 2 of 0x2050C8 */
    h_put(t, "\033_Ga=f,i=5,c=1,f=24,s=1,v=1,x=0,y=0;/wAA\033\\"); /* frame 2: a red pixel at (0,0) */
    CHECK(vt_kitty_frame_px(t, 5, 1, 1, 1) > 0);
    CHECK(vt_kitty_frame_px(t, 5, 2, 1, 1) > 0);
    CHECK(vt_kitty_frame_px(t, 5, 2, 1, 0) > 0);
    vt_free(t);
}

static void k38_switching_off_converts_and_on_leaves_indices(void)
{
    vt_term *t = k38_term(1);
    vt_image_view v;
    k38_send_gradient(t, "a=T,i=5,f=32,s=64,v=64,C=1");
    CHECK_INT(vt_image_memory(t), 2 * 16384);
    vt_set_image_rgb(t, 0);
    CHECK_INT(vt_image_memory(t), 2 * 4096);
    CHECK_INT(vt_images(t), 2);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.fmt, 0);
    CHECK(v.npal <= 256);
    CHECK_INT(v.w, 64);
    vt_set_image_rgb(t, 1); /* off -> on: the old picture stays indexed */
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.fmt, 0);
    CHECK_INT(vt_image_memory(t), 2 * 4096);
    vt_free(t);
}

/* ---- K35: animation control (a=a), compose (a=c), the clock ---- */

/* a 2 x 2 picture (red green / blue white) with frame 2 = root plus a blue pixel at (0,0) and frame 3 on
 * frame 2 with a red pixel at (1,1); placed on the screen */
static vt_term *k35_term(void)
{
    vt_term *t = kitty_term(10, 5);
    h_put(t, "\033_Ga=t,i=5,f=24,s=2,v=2;/wAAAP8AAAD/////\033\\");
    h_put(t, "\033_Ga=f,i=5,c=1,f=24,s=1,v=1,x=0,y=0,z=40;AAD/\033\\");
    return t;
}

/* K36: kitty scan_active_animations switches once per wake and sets shown_at = now: a late wake drops the lateness */
static void k36_a_late_tick_switches_once_and_never_catches_up(void)
{
    vt_term *t = k35_term();
    unsigned long sw0 = vt_kitty_frame_switches(t);
    h_put(t, "\033_Ga=a,i=5,s=3\033\\");
    CHECK_INT(vt_anim_tick(t, 0), 40000);
    CHECK_INT(vt_anim_tick(t, 100000), 40000); /* 100 ms late: one switch, the next frame a whole gap away */
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    CHECK_INT(vt_anim_tick(t, 0), 40000); /* the lateness is gone, not carried */
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    vt_free(t);
}

static void k35_a_a_keys_move_the_state(void)
{
    vt_term *t = k35_term();
    unsigned long sw0 = vt_kitty_frame_switches(t);
    h_reply_clear();
    h_put(t, "\033_Ga=a,i=5,s=3\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;OK\033\\");
    CHECK_INT(vt_anim_tick(t, 0), 40000);
    CHECK_INT(vt_anim_tick(t, 40000), 40000); /* frame 2 now, the next due in 40 ms */
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    CHECK_INT(vt_anim_tick(t, 40000), 40000); /* loops back to frame 1 */
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 2);
    /* s=1 stops: nothing runs */
    h_put(t, "\033_Ga=a,i=5,s=1\033\\");
    CHECK_INT(vt_anim_tick(t, 40000), -1);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 2);
    /* r= with z= sets a frame's gap, the root's included */
    h_put(t, "\033_Ga=a,i=5,s=3,r=1,z=10\033\\");
    CHECK_INT(vt_anim_tick(t, 0), 10000);
    CHECK_INT(vt_anim_tick(t, 10000), 40000); /* on frame 2, gap 40 */
    h_put(t, "\033_Ga=a,i=5,s=1\033\\");
    /* c= shows a frame at once, no tick, and counts as a switch */
    sw0 = vt_kitty_frame_switches(t);
    h_put(t, "\033_Ga=a,i=5,c=1\033\\");
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1); /* the clock left it on frame 2 */
    h_put(t, "\033_Ga=a,i=5,c=1\033\\");
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1); /* already frame 1: no switch */
    vt_free(t);
}

static void k35_loops_wait_and_errors(void)
{
    vt_term *t = k35_term();
    unsigned long sw0 = vt_kitty_frame_switches(t);
    /* v=2: one loop, then it stops on the last frame */
    h_put(t, "\033_Ga=a,i=5,s=3,v=2\033\\");
    vt_anim_tick(t, 40000);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    CHECK_INT(vt_anim_tick(t, 40000), -1); /* stopped at the end of its one loop */
    CHECK_INT(vt_anim_tick(t, 400000), -1);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    /* s=2 waits at the last frame and goes on when a frame arrives */
    h_put(t, "\033_Ga=a,i=5,s=2,c=1\033\\");
    sw0 = vt_kitty_frame_switches(t);
    vt_anim_tick(t, 40000);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    CHECK_INT(vt_anim_tick(t, 400000), -1); /* at the end: waits */
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    h_put(t, "\033_Ga=f,i=5,c=2,f=24,s=1,v=1,x=1,y=1;/wAA\033\\");
    CHECK_INT(vt_anim_tick(t, 1000), -1); /* frame 3 is the last again: waits */
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 2); /* frame 3 shown */
    /* gapless frames are never shown */
    h_put(t, "\033_Ga=a,i=5,s=3,c=1,r=2,z=-1\033\\");
    sw0 = vt_kitty_frame_switches(t);
    vt_anim_tick(t, 40000);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1); /* 1 -> 3 */
    /* errors */
    h_reply_clear();
    h_put(t, "\033_Ga=a,i=9,s=3\033\\");
    CHECK_STR(h_reply, "\033_Gi=9;ENOENT:no such image\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=a,i=5,c=9\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;ENOENT:no such frame\033\\");
    h_put(t, "\033_Ga=t,i=6,f=24,s=1,v=1;AAD/\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=a,i=6,s=3\033\\");
    CHECK_STR(h_reply, "\033_Gi=6;ENOENT:the picture has no frames\033\\");
    vt_free(t);
}

static void k35_a_c_composes_a_rectangle(void)
{
    vt_term *t = k35_term();
    h_put(t, "\033_Ga=f,i=5,c=2,f=24,s=1,v=1,x=1,y=1;/wAA\033\\"); /* frame 3 */
    h_reply_clear();
    /* frame 3's pixel (1,1) red onto frame 1 at (0,0) */
    h_put(t, "\033_Ga=c,i=5,r=3,c=1,X=1,Y=1,x=0,y=0,w=1,h=1\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;OK\033\\");
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 0, 0), K34_RED);
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 1, 0), K34_GREEN); /* the rest stays */
    /* frame 2's blue (0,0) onto frame 1's (1,1) */
    h_put(t, "\033_Ga=c,i=5,r=2,c=1,X=0,Y=0,x=1,y=1,w=1,h=1\033\\");
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 1, 1), K34_BLUE);
    /* errors */
    h_reply_clear();
    h_put(t, "\033_Ga=c,i=5,r=2,c=7,w=1,h=1\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;ENOENT:no such frame\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=c,i=9,r=2,c=1\033\\");
    CHECK_STR(h_reply, "\033_Gi=9;ENOENT:no such image\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=c,i=5,r=2,c=1,x=1,y=1,w=2,h=2\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;EINVAL:rectangle outside the picture\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=c,i=5,r=1,c=1,X=0,Y=0,x=1,y=0,w=2,h=1\033\\");
    CHECK_STR(h_reply, "\033_Gi=5;EINVAL:rectangle outside the picture\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=c,i=5,r=1,c=1,X=0,Y=0,x=1,y=1,w=2,h=2\033\\"); /* overlaps itself (and is out) */
    CHECK_STR(h_reply, "\033_Gi=5;EINVAL:rectangle outside the picture\033\\");
    h_reply_clear();
    h_put(t, "\033_Ga=c,i=5,r=1,c=1,X=0,Y=0,x=0,y=0,w=1,h=2\033\\"); /* the same rectangle: overlap */
    CHECK_STR(h_reply, "\033_Gi=5;EINVAL:rectangles overlap\033\\");
    vt_free(t);
}

/* THE reachability test: the byte stream of a program puts a picture, animates it, and the clock changes
 * the pixels on the screen and damages its rows */
static void k35_the_clock_changes_the_placement(void)
{
    vt_term *t = k35_term();
    vt_image_view v;
    unsigned long sw0 = vt_kitty_frame_switches(t);
    int d0;
    h_put(t, "\033_Ga=p,i=5\033\\"); /* placed: root frame, 2 x 2 */
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    h_put(t, "\033_Ga=a,i=5,s=3\033\\");
    vt_flush(t);
    d0 = h_damage_calls;
    vt_anim_tick(t, 40000);
    vt_flush(t);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1); /* sentinel: the clock ran */
    CHECK(h_damage_calls > d0);                              /* the rows of the placement were marked */
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(pixel(&v, 0, 0), 0x0000FF); /* frame 2: blue at (0,0) */
    CHECK_INT(pixel(&v, 1, 1), 0xFFFFFF);
    /* editing the root (r=1) while it is shown reaches the placement without a tick */
    h_put(t, "\033_Ga=a,i=5,s=1,c=1\033\\");
    h_put(t, "\033_Ga=f,i=5,r=1,f=24,s=1,v=1,x=1,y=0;/wAA\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(pixel(&v, 1, 0), 0xFF0000);
    vt_free(t);
}

/* K36: the profile key image-animation = off reaches the engine as vt_set_animation(t, 0): the clock
 * switches nothing and says nothing runs; a client's a=a c= still switches. */
static void k36_image_animation_off_stops_the_clock(void)
{
    vt_term *t = k35_term();
    unsigned long sw0 = vt_kitty_frame_switches(t);
    h_put(t, "\033_Ga=a,i=5,s=3\033\\");
    vt_set_animation(t, 0);
    CHECK_INT(vt_anim_tick(t, 40000), -1);
    CHECK_INT(vt_anim_tick(t, 400000), -1);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 0); /* no frame advanced */
    h_put(t, "\033_Ga=a,i=5,c=2\033\\");
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1); /* the client still switches */
    vt_set_animation(t, 1);
    CHECK_INT(vt_anim_tick(t, 0), 40000);
    CHECK_INT(vt_anim_tick(t, 40000), 40000);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 2); /* on again: the sentinel moves */
    vt_free(t);
}

/* ---- K39: frames of an RGB picture are RGBA canvases ---- */

/* 200 x 200 canvases: 160,000 B each in RGB mode, 40,000 in index mode, against 8 MB / 2 MB:
 * 52 canvases (the root's and 51 frames) fit in both modes, the 53rd answers ENOSPC */
static void k39_frame_canvases_stop_at_the_x4_budget_in_both_modes(void)
{
    static const char *const mode[2] = { "index", "rgb" };
    int m;
    for (m = 0; m < 2; m++) {
        unsigned char *raw = (unsigned char *)calloc(200L * 200L * 4L, 1);
        char *b = (char *)malloc(200L * 200L * 4L / 3 * 4 + 8);
        char *msg = (char *)malloc(200L * 200L * 4L / 3 * 4 + 128);
        vt_term *t = k38_term(m);
        int j, bad = 0;
        k38_b64(raw, 200L * 200L * 4L, b);
        snprintf(msg, 200L * 200L * 4L / 3 * 4 + 128, "\033_Ga=t,i=7,f=32,s=200,v=200;%s\033\\", b);
        h_put(t, msg);
        for (j = 0; j < 51; j++) { /* frames 2..52 */
            h_reply_clear();
            h_put(t, "\033_Ga=f,i=7,f=32,s=1,v=1;EjRW/w==\033\\");
            if (strcmp(h_reply, "\033_Gi=7;OK\033\\"))
                bad++;
        }
        h_checks++;
        if (bad)
            h_failures++, printf("[FAIL] %s mode: %d of 51 frames refused\n", mode[m], bad);
        CHECK_INT((long)vt_kitty_frames(t), 52);
        CHECK_INT((long)vt_kitty_rgba_frames(t), m ? 52 : 0); /* sentinel: the root's canvas and 51 frames */
        h_reply_clear();
        h_put(t, "\033_Ga=f,i=7,f=32,s=1,v=1;EjRW/w==\033\\"); /* the 53rd canvas */
        CHECK_STR(h_reply, "\033_Gi=7;ENOSPC:no room for frames\033\\");
        CHECK_INT((long)vt_kitty_frames(t), 52);
        CHECK_INT((long)vt_kitty_rgba_frames(t), m ? 52 : 0);
        vt_free(t);
        free(raw);
        free(b);
        free(msg);
    }
}

/* frame 2 = root + a 1-px rectangle in exact RGB with alpha, frame 3 = frame 2 + another (a=f c=2), a=c copies
 * a rectangle with its alpha; a pixel under half alpha keeps the base; the clock shows frame 2 in the placement */
static void k39_rgba_frames_keep_exact_bytes_and_alpha(void)
{
    vt_term *t = k38_term(1);
    vt_image_view v;
    unsigned long sw0, n0;
    /* root 2x2: 0x112233 FF, 0x445566 80, 0x778899 FF, 0xAABBCC 10 */
    h_put(t, "\033_Ga=T,i=5,f=32,s=2,v=2,C=1;ESIz/0RVZoB3iJn/qrvMEA==\033\\");
    n0 = vt_kitty_rgba_frames(t);
    h_put(t, "\033_Ga=f,i=5,c=1,f=32,s=1,v=1,x=1,y=1,z=40;EjRW/w==\033\\"); /* frame 2: (1,1) = 0x123456 FF */
    h_put(t, "\033_Ga=f,i=5,c=2,f=32,s=1,v=1,x=0,y=0;AQID7w==\033\\"); /* frame 3 on 2: alpha 0xEF at (0,0) */
    h_put(t, "\033_Ga=f,i=5,c=2,f=32,s=1,v=1,x=1,y=0;/wAAIA==\033\\"); /* frame 4 on 2: alpha 0x20, base kept */
    CHECK_INT((long)(vt_kitty_rgba_frames(t) - n0), 4); /* root canvas + frames 2, 3, 4 */
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 0, 0), 0xFF112233L);
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 1, 1), 0x10AABBCCL);
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 1, 1), 0xFF123456L);
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 0, 0), 0xFF112233L);
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 1, 0), 0x80445566L); /* the base alpha stays exact */
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 0, 0), 0xEF010203L);
    CHECK_INT(vt_kitty_frame_px(t, 5, 3, 1, 1), 0xFF123456L);
    CHECK_INT(vt_kitty_frame_px(t, 5, 4, 1, 0), 0x80445566L); /* alpha 0x20 under half: the base stays */
    /* a=c: frame 3's (1,1) onto frame 1's (0,1) */
    h_put(t, "\033_Ga=c,i=5,r=3,c=1,X=1,Y=1,x=0,y=1,w=1,h=1\033\\");
    CHECK_INT(vt_kitty_frame_px(t, 5, 1, 0, 1), 0xFF123456L);
    sw0 = vt_kitty_frame_switches(t);
    h_put(t, "\033_Ga=a,i=5,s=3\033\\");
    vt_anim_tick(t, 40000);
    CHECK_INT((long)(vt_kitty_frame_switches(t) - sw0), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.fmt, 1);
    CHECK_INT(v.w, 2);
    CHECK_INT(v.pix[3 * 4], 0x12); /* frame 2, pixel (1,1): R G B A */
    CHECK_INT(v.pix[3 * 4 + 1], 0x34);
    CHECK_INT(v.pix[3 * 4 + 2], 0x56);
    CHECK_INT(v.pix[3 * 4 + 3], 0xFF);
    CHECK_INT(v.pix[4 + 3], 0x80); /* (1,0) keeps its own alpha */
    CHECK_INT(v.opaque, 0);
    vt_set_image_rgb(t, 0); /* off: the canvases are cube indices again, a quarter of the bytes */
    CHECK_INT(vt_kitty_frame_px(t, 5, 2, 1, 1) >= 0 && vt_kitty_frame_px(t, 5, 2, 1, 1) < 256, 1);
    vt_free(t);
}

/* IMAGE-ASPECT-HIRES: on 640x256 hires the pixel is twice as tall as wide (rx:ry = 1:2); a picture placed
 * with no cell counts keeps its look, so its height halves (rounded to nearest, half up, at least 1).
 * Square pixels (1:1) and an unset aspect leave the size exactly as sent. */
static int asp_height(int rx, int ry)
{
    vt_term *t = h_new(20, 10, VT_XTERM);
    vt_image_view v;
    int h = -1, i;
    char b[160];
    vt_set_cell_pixels(t, 8, 16);
    if (rx)
        vt_set_pixel_aspect(t, rx, ry);
    for (i = 0; i < 112; i++)
        b[i] = 'A'; /* 84 zero bytes: a 4 x 7 RGB picture */
    b[i] = 0;
    h_put(t, "\033_Ga=T,f=24,s=4,v=7,q=2;");
    h_put(t, b);
    h_put(t, "\033\\");
    if (vt_row_image(t, 0, 0, &v))
        h = v.h;
    return h;
}

static void image_aspect_hires_halves_the_height_on_tall_pixels(void)
{
    CHECK(vt_aspect_rows(64, 1, 2) == 32);
    CHECK(vt_aspect_rows(7, 1, 2) == 4); /* 3.5 rounds up */
    CHECK(vt_aspect_rows(1, 1, 2) == 1); /* never below one row */
    CHECK(vt_aspect_rows(64, 1, 1) == 64);
    CHECK(vt_aspect_rows(64, 0, 0) == 64);
    CHECK(asp_height(1, 2) == 4); /* through the kitty placement */
    CHECK(asp_height(1, 1) == 7);
    CHECK(asp_height(0, 0) == 7);
}

void suite_kitty(void)
{
    image_aspect_hires_halves_the_height_on_tall_pixels();
    raw_rgb_becomes_an_indexed_image();
    rgba_clear_pixels_stay_unset();
    chunks_join_into_one_picture();
    zlib_data_is_inflated();
    png_goes_through_the_host_decoder();
    own_decoder_reads_every_colour_type_depth_and_filter();
    own_decoder_answers_0_broken_minus_1_unsupported_minus_2_too_big();
    png_is_decoded_by_the_engine_and_never_reaches_the_host();
    query_answers_without_showing();
    quiet_keys_hide_the_replies();
    delete_by_id_removes_the_image();
    moved_and_deleted_placements_damage_their_old_cells();
    c_equals_1_past_the_bottom_cuts_and_never_scrolls();
    putting_an_unknown_id_says_enoent();
    a_picture_over_the_memory_limit_says_enomem();
    stored_pictures_give_way_to_new_ones();
    k25_t_f_reads_through_the_host_reader();
    k26_t_t_unlinks_only_kitty_named_files_after_a_good_read();
    k28_t_s_shared_memory_says_ebadf();
    c_equals_1_keeps_the_cursor();
    columns_and_rows_scale_the_picture();
    apc_g_is_not_reported_unhandled();
    k19_signed_keys_keep_their_sign();
    k20_bad_key_combinations_are_einval();
    k21_retransmitting_an_id_removes_its_placements();
    k22_transient_pictures_are_evicted_first();
    k23_pixel_offset_inside_the_first_cell();
    k24_the_missing_deletes();
    k29_child_sits_at_the_parent_plus_h_v();
    k29_deleting_a_parent_deletes_its_children();
    k29_the_error_replies();
    k29_virtual_placements_and_parents();
    k30_row_images_go_by_z_then_id();
    k30_text_over_a_picture_below_keeps_it();
    k40d_kitty_front_picture_keeps_the_cells();
    k31_placeholder_cells_resolve_to_tiles();
    k31_third_mark_is_the_ids_high_byte();
    k34_frames_build_on_each_other();
    k34_frames_stop_at_the_animation_memory();
    k35_a_a_keys_move_the_state();
    k36_a_late_tick_switches_once_and_never_catches_up();
    k35_loops_wait_and_errors();
    k35_a_c_composes_a_rectangle();
    k35_the_clock_changes_the_placement();
    k36_image_animation_off_stops_the_clock();
    k38_rgb_store_keeps_every_colour_and_the_alpha_byte();
    k38_index_mode_is_unchanged();
    k38_an_opaque_picture_says_so_and_a_placement_scales_the_rgba();
    k38_budget_is_x4_with_the_same_pixel_capacity();
    k38_switching_off_converts_and_on_leaves_indices();
    a_shrunk_picture_averages_instead_of_aliasing();
    a_flat_two_colour_picture_shows_exact_colours_undithered();
    a_shrunk_many_colour_picture_copies_its_averaged_colours();
    a_frame_over_an_exact_colour_root_keeps_the_root_pixels();
    scrolling_a_cropped_picture_does_not_rebuild_it();
    a_crop_off_the_rows_is_still_its_own_picture();
    k39_frame_canvases_stop_at_the_x4_budget_in_both_modes();
    k39_rgba_frames_keep_exact_bytes_and_alpha();
}
