/* Kitty graphics (APC G): the keys and the base64 data become the same
 * indexed images sixel makes, replies go back to the program, and the
 * pictures obey the shared image memory. Cells are 2 x 6 pixels. The
 * PNG decoder here is a stand-in that hands back fixed pixels. */
#include "harness.h"

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

static void files_are_refused(void)
{
    vt_term *t = kitty_term(10, 5);
    h_reply_clear();
    h_put(t, "\033_Ga=T,t=f,i=3,f=24,s=1,v=1;AAAA\033\\");
    CHECK_STR(h_reply, "\033_Gi=3;ENOTSUP:only direct data (t=d)\033\\");
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

void suite_kitty(void)
{
    raw_rgb_becomes_an_indexed_image();
    rgba_clear_pixels_stay_unset();
    chunks_join_into_one_picture();
    zlib_data_is_inflated();
    png_goes_through_the_host_decoder();
    query_answers_without_showing();
    quiet_keys_hide_the_replies();
    delete_by_id_removes_the_image();
    putting_an_unknown_id_says_enoent();
    a_picture_over_the_memory_limit_says_enomem();
    stored_pictures_give_way_to_new_ones();
    files_are_refused();
    c_equals_1_keeps_the_cursor();
    columns_and_rows_scale_the_picture();
    apc_g_is_not_reported_unhandled();
}
