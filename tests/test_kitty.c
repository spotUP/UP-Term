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
    CHECK_INT(pixel(&v, 0, 0), 0x333333); /* 0x112233 on the cube */
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

/* z >= 0 keeps the model of D12: the picture takes the cells */
static void k30_z_0_and_up_still_takes_the_cells(void)
{
    vt_term *t = kitty_term(20, 8);
    h_put(t, "\033[1;1HXY");
    put_at(t, 1, 1, "i=1,p=1,z=5");
    CHECK(is_image(t, 0, 0) && !is_under(t, 0, 0));
    CHECK(h_cell(t, 0, 0)->ch == ' ');
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

void suite_kitty(void)
{
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
    k30_z_0_and_up_still_takes_the_cells();
    k31_placeholder_cells_resolve_to_tiles();
    k31_third_mark_is_the_ids_high_byte();
    k34_frames_build_on_each_other();
    k34_frames_stop_at_the_animation_memory();
}
