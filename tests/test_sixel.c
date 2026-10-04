/* DEC sixel graphics (gaps #20): the decoder on hand-made streams whose
 * pixels are known, the image store moving with the text, and the
 * replies (DA1, XTSMGRAPHICS, DECRQM). Cells are 2 x 6 pixels here, so a
 * sixel band is one text row. */
#include "harness.h"

static vt_term *sixel_term(int cols, int rows)
{
    vt_term *t = h_new(cols, rows, VT_XTERM);
    vt_set_cell_pixels(t, 2, 6);
    return t;
}

static vt_u32 pixel(const vt_image_view *v, int x, int y)
{
    return v->pal[v->pix[y * v->w + x]];
}

static int image_cell(vt_term *t, int x, int y)
{
    return (h_cell(t, x, y)->pad & VT_CELL_IMAGE) != 0;
}

/* 4 x 12: two columns red and two green over the first band, all red
 * below. RGB in percent; 1:1 pixels from the raster attributes. */
static const char two_colours[] =
    "\033Pq\"1;1;4;12#1;2;100;0;0#2;2;0;100;0#1~~#2~~-#1!4~\033\\";

static void decodes_a_known_image(void)
{
    vt_term *t = sixel_term(10, 5);
    vt_image_view v;
    int x, y, i;
    long n;
    const char *kinds[4];
    long counts[4];
    h_put(t, two_colours);
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 4);
    CHECK_INT(v.h, 12);
    CHECK_INT(v.col0, 0);
    CHECK_INT(v.py, 0);
    CHECK_INT(v.cw, 2);
    CHECK_INT(v.ch, 6);
    for (y = 0; y < 6; y++)
        for (x = 0; x < 4; x++)
            CHECK_INT(pixel(&v, x, y), x < 2 ? 0xFF0000 : 0x00FF00);
    for (y = 6; y < 12; y++)
        for (x = 0; x < 4; x++)
            CHECK_INT(pixel(&v, x, y), 0xFF0000);
    CHECK_INT(v.npal, 3);              /* the unset entry and the two used */
    CHECK(vt_row_image(t, 1, 0, &v));
    CHECK_INT(v.py, 6);
    CHECK(!vt_row_image(t, 0, 1, &v));
    CHECK(!vt_row_image(t, 2, 0, &v));
    for (i = 0; i < 2; i++) {
        CHECK(image_cell(t, 0, i));
        CHECK(image_cell(t, 1, i));
        CHECK(!image_cell(t, 2, i));
        CHECK_INT(h_cell(t, 0, i)->ch, ' ');
    }
    vt_cursor(t, &x, &y);              /* on the image's last row, its column */
    CHECK_INT(x, 0);
    CHECK_INT(y, 1);
    n = vt_unhandled(t, kinds, counts, 4);
    CHECK_INT(n, 0);
    vt_free(t);
}

/* DEC HLS: hue 0 is blue, 120 red, 240 green; RGB percentages round. */
static void hls_and_rgb_registers(void)
{
    vt_term *t = sixel_term(10, 5);
    vt_image_view v;
    h_put(t, "\033P;;q\"1;1"
             "#1;1;120;50;100@#2;1;240;50;100@#3;1;0;50;100@#4;2;50;50;50@#5;1;0;100;0@\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 5);
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    CHECK_INT(pixel(&v, 1, 0), 0x00FF00);
    CHECK_INT(pixel(&v, 2, 0), 0x0000FF);
    CHECK_INT(pixel(&v, 3, 0), 0x808080);
    CHECK_INT(pixel(&v, 4, 0), 0xFFFFFF);
    vt_free(t);
}

/* '!' repeats, '$' goes back to the band's start and draws over, a
 * register defined again takes a new entry (pixels drawn keep theirs). */
static void repeat_overprint_and_redefinition(void)
{
    vt_term *t = sixel_term(10, 5);
    vt_image_view v;
    h_put(t, "\033Pq\"1;1#1;2;100;0;0!3~$#2;2;0;0;100?@$#1;2;0;100;0???~\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 4);
    CHECK_INT(pixel(&v, 0, 0), 0xFF0000);
    CHECK_INT(pixel(&v, 1, 0), 0x0000FF); /* '@' is bit 0: the top pixel only */
    CHECK_INT(pixel(&v, 1, 1), 0xFF0000);
    CHECK_INT(pixel(&v, 2, 5), 0xFF0000);
    CHECK_INT(pixel(&v, 3, 0), 0x00FF00); /* register 1 again, another colour */
    vt_free(t);
}

/* P1 sets the pixel aspect ratio when no raster attributes do: 0 (the
 * default) is 2:1, a sixel bit two pixels tall; 7-9 are 1:1. */
static void aspect_ratio_from_p1(void)
{
    vt_term *t = sixel_term(10, 5);
    vt_image_view v;
    h_put(t, "\033Pq#1~\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 1);
    CHECK_INT(v.h, 12);
    vt_free(t);
    t = sixel_term(10, 5);
    h_put(t, "\033P9q#1~\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.h, 6);
    vt_free(t);
}

/* Any number of writes, of any size: no 256-byte string buffer. */
static void streams_across_writes_and_past_256_bytes(void)
{
    vt_term *t = sixel_term(80, 5);
    vt_image_view v;
    static char big[4200];
    int i, n = 0;
    const char *s = two_colours;
    for (; *s; s++)
        vt_write(t, (const vt_u8 *)s, 1); /* a byte a write */
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 4);
    CHECK_INT(pixel(&v, 3, 0), 0x00FF00);
    vt_free(t);

    t = sixel_term(80, 5);
    vt_set_cell_pixels(t, 16, 6);
    strcpy(big, "\033P7q#1;2;0;0;100");
    n = (int)strlen(big);
    for (i = 0; i < 1000; i++)
        big[n++] = (char)(i % 2 ? '~' : '@');
    big[n++] = 0x1B;
    big[n++] = '\\';
    big[n] = 0;
    h_put(t, big);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 1000);
    CHECK_INT(v.h, 6);
    CHECK_INT(pixel(&v, 999, 5), 0x0000FF);
    CHECK_INT(v.pix[998 + 5 * v.w], 0); /* '@' left the rest unset */
    vt_free(t);
}

/* The size has a limit: what lies past it is cut, nothing breaks. */
static void oversized_images_are_cut(void)
{
    vt_term *t = sixel_term(80, 5);
    vt_image_view v;
    vt_set_cell_pixels(t, 16, 6);
    h_put(t, "\033P7q\"1;1;5000;6#1!3000~\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, VT_SIXEL_MAX_W);
    CHECK_INT(v.h, 6);
    vt_free(t);
}

/* An image runs past the bottom: the screen scrolls like text; the image
 * goes on into the scrollback with its rows, and from there out. */
static void scrolls_with_the_text_into_the_scrollback(void)
{
    vt_term *t = sixel_term(10, 4);
    vt_image_view v;
    int x, y;
    h_put(t, "a\r\nb\r\nc\r\nd");
    h_put(t, "\033Pq\"1;1;2;12#1;2;100;0;0~~-~~\033\\");
    CHECK_STR(h_screen(t), "b|c|d");    /* d's row holds the image's top too */
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 3);
    CHECK_INT(x, 1);
    CHECK(vt_row_image(t, 2, 0, &v));
    CHECK_INT(v.col0, 1);
    CHECK_INT(v.py, 0);
    CHECK(vt_row_image(t, 3, 0, &v));
    CHECK_INT(v.py, 6);
    h_put(t, "\r\n1\r\n2\r\n3\r\n4");
    CHECK(vt_row_image(t, -2, 0, &v)); /* in the scrollback now */
    CHECK_INT(v.py, 0);
    CHECK(vt_row_image(t, -1, 0, &v));
    CHECK_INT(v.py, 6);
    CHECK_INT(vt_images(t), 1);
    vt_clear_scrollback(t);
    CHECK_INT(vt_images(t), 0);         /* its last placement went: freed */
    vt_free(t);

    t = sixel_term(10, 3);
    CHECK(vt_set_scrollback(t, 2));
    h_put(t, two_colours);
    h_put(t, "\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6");
    CHECK_INT(vt_images(t), 0);         /* out of a full scrollback: freed */
    vt_free(t);
}

/* Text written over an image takes those cells; an erase of the row lets
 * the image go. */
static void text_and_erase_replace_the_image(void)
{
    vt_term *t = sixel_term(10, 4);
    vt_image_view v;
    h_put(t, two_colours);
    h_put(t, "\033[1;2HX");
    CHECK(image_cell(t, 0, 0));
    CHECK(!image_cell(t, 1, 0));
    CHECK_INT(h_cell(t, 1, 0)->ch, 'X');
    h_put(t, "\033[2;1H\xc3\xa9"); /* not ASCII: put_char's way */
    CHECK(!image_cell(t, 0, 1));
    CHECK(image_cell(t, 1, 1));
    h_put(t, "\033[2K");
    CHECK(!vt_row_image(t, 1, 0, &v));
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(vt_images(t), 1);
    h_put(t, "\033[2J");
    CHECK_INT(vt_images(t), 0);
    vt_free(t);
}

/* A program drawing frame after frame in one place keeps one image. */
static void frames_in_one_place_keep_one_image(void)
{
    vt_term *t = sixel_term(10, 4);
    vt_image_view v;
    int i;
    for (i = 0; i < 5; i++) {
        h_put(t, "\033[H");
        h_put(t, two_colours);
    }
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK(!vt_row_image(t, 0, 1, &v));
    vt_free(t);
}

/* ?80 (DECSDM): the image at the top left, the cursor stays. */
static void decsdm_places_at_the_top_left(void)
{
    vt_term *t = sixel_term(10, 4);
    vt_image_view v;
    int x, y;
    h_put(t, "\033[3;5H\033[?80h");
    h_put(t, two_colours);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 4);
    CHECK_INT(y, 2);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.col0, 0);
    CHECK(image_cell(t, 0, 0));
    CHECK(!image_cell(t, 4, 2));
    h_reply_clear();
    h_put(t, "\033[?80$p");
    CHECK_STR(h_reply, "\033[?80;1$y");
    h_put(t, "\033c");                  /* RIS: sixel scrolling again */
    h_reply_clear();
    h_put(t, "\033[?80$p");
    CHECK_STR(h_reply, "\033[?80;2$y");
    vt_free(t);
}

/* ?1070: each image its own registers (default) or one shared set. */
static void private_and_shared_registers(void)
{
    vt_term *t = sixel_term(10, 6);
    vt_image_view v;
    h_reply_clear();
    h_put(t, "\033[?1070$p");
    CHECK_STR(h_reply, "\033[?1070;1$y");
    h_put(t, "\033P7q#5;2;100;100;0~\033\\\r\n");
    h_put(t, "\033P7q#5~\033\\\r\n");
    CHECK(vt_row_image(t, 1, 0, &v));
    CHECK_INT(pixel(&v, 0, 0), 0x33CCCC); /* the VT340's register 5 */
    h_put(t, "\033[?1070l");
    h_put(t, "\033P7q#5;2;100;100;0~\033\\\r\n");
    h_put(t, "\033P7q#5~\033\\\r\n");
    CHECK(vt_row_image(t, 3, 0, &v));
    CHECK_INT(pixel(&v, 0, 0), 0xFFFF00); /* the first image's definition */
    vt_free(t);
}

static void replies_tell_programs_about_sixel(void)
{
    vt_term *t = sixel_term(10, 4);
    h_reply_clear();
    h_put(t, "\033[c");
    CHECK_STR(h_reply, "\033[?62;4;22c");
    h_reply_clear();
    h_put(t, "\033[?1;1;0S");
    CHECK_STR(h_reply, "\033[?1;0;256S");
    h_reply_clear();
    h_put(t, "\033[?2;1;0S");
    CHECK_STR(h_reply, "\033[?2;0;20;24S"); /* the window's pixels */
    h_reply_clear();
    h_put(t, "\033[?2;4;0S");
    CHECK_STR(h_reply, "\033[?2;0;1024;1024S");
    h_reply_clear();
    h_put(t, "\033[?3;1;0S");
    CHECK_STR(h_reply, "\033[?3;1;0S");     /* no ReGIS */
    h_reply_clear();
    h_put(t, "\033[?1;9;0S");
    CHECK_STR(h_reply, "\033[?1;2;0S");
    vt_free(t);
}

/* CAN cancels: no image, and the text after it is text. */
static void cancel_drops_the_image(void)
{
    vt_term *t = sixel_term(10, 4);
    h_put(t, "\033Pq#1~~~\x18ok");
    CHECK_INT(vt_images(t), 0);
    CHECK_STR(h_row(t, 0), "ok");
    h_put(t, "\033P7q#1~~~\033[31mred"); /* ESC other than ST ends it too */
    CHECK_INT(vt_images(t), 1);
    CHECK_STR(h_row(t, 0), "okred");
    vt_free(t);
}

/* A reflow carries the tiles: an image split over two rows shows the rest
 * of its columns on the second, and joins again when widened. */
static void reflow_carries_the_image(void)
{
    vt_term *t = sixel_term(10, 4);
    vt_image_view v;
    vt_set_reflow(t, 1);
    h_put(t, "abcdef\033Pq\"1;1;8;6#1!8~\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.col0, 6);
    vt_resize(t, 8, 4);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.col0, 6);
    CHECK(image_cell(t, 7, 0));
    CHECK(vt_row_image(t, 1, 0, &v));
    CHECK_INT(v.col0, -2);              /* cells 0-1 show image columns 2-3 */
    CHECK(image_cell(t, 1, 1));
    CHECK(!image_cell(t, 2, 1));
    vt_resize(t, 10, 4);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.col0, 6);
    CHECK(!vt_row_image(t, 0, 1, &v));  /* one placement again, not two */
    CHECK(!vt_row_image(t, 1, 0, &v));
    CHECK_INT(vt_images(t), 1);
    vt_free(t);
}

/* All images share VT_IMAGE_MEMORY: the oldest gives way, its cells then
 * draw nothing. */
static void the_oldest_image_gives_way(void)
{
    vt_term *t = sixel_term(80, 30);
    vt_image_view v;
    static const char mega[] = "\033P7q\"1;1;1024;1024#1~\033\\\033[H";
    vt_set_cell_pixels(t, 64, 64);
    h_put(t, mega);
    CHECK_INT(vt_images(t), 1);
    h_put(t, "\033[1;30H");
    h_put(t, mega);
    CHECK_INT(vt_images(t), 2);
    h_put(t, "\033[1;60H");
    h_put(t, mega);
    CHECK_INT(vt_images(t), 2);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.col0, 29);              /* the first went */
    CHECK(vt_row_image(t, 0, 1, &v));
    CHECK_INT(v.col0, 59);
    CHECK(!vt_row_image(t, 0, 2, &v));
    vt_free(t);
}

/* An image over half of a wide glyph blanks its other half. */
static void image_over_half_a_wide_glyph(void)
{
    vt_term *t = sixel_term(10, 4);
    h_put(t, "ab\xe6\x97\xa5" "c\033[1;4H\033P7q#1~\033\\");
    CHECK_INT(h_cell(t, 2, 0)->ch, ' ');
    CHECK_INT(h_cell(t, 2, 0)->width, 1);
    CHECK(image_cell(t, 3, 0));
    CHECK_INT(h_cell(t, 4, 0)->ch, 'c');
    vt_free(t);
}

/* Streams from a real encoder (ImageMagick 7.1.2 `magick in.ppm sixel:out`,
 * tests/sixel/): a 20 x 13 picture of four coloured quarters, exact; and a
 * 64 x 48 gradient in 256 registers, within the encoder's quantisation. */
static long read_file(const char *path, char *buf, long max)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(buf, 1, max, f);
    fclose(f);
    return n;
}

static void encoder_streams_decode(void)
{
    static char buf[16384];
    static const vt_u32 quarter[4] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00 };
    vt_term *t = sixel_term(80, 10);
    vt_image_view v;
    long n = read_file("tests/sixel/quad-magick.six", buf, sizeof(buf)), err, sum = 0, worst = 0;
    int x, y, k, chunk;
    CHECK(n > 0);
    if (n > 0)
        vt_write(t, (const vt_u8 *)buf, n);
    CHECK(vt_row_image(t, 0, 0, &v));
    if (vt_row_image(t, 0, 0, &v)) {
        CHECK_INT(v.w, 20);
        CHECK_INT(v.h, 13);
        for (y = 0; y < 13; y++)
            for (x = 0; x < 20; x++)
                CHECK_INT(pixel(&v, x, y), quarter[(x >= 10) + 2 * (y >= 7)]);
    }
    vt_free(t);

    t = sixel_term(80, 10);
    n = read_file("tests/sixel/gradient-magick.six", buf, sizeof(buf));
    CHECK(n > 4096);
    /* in pieces of odd sizes, as a pty hands them over */
    for (k = 0, chunk = 1; k < n; k += chunk, chunk = chunk * 3 % 97 + 1)
        vt_write(t, (const vt_u8 *)buf + k, k + chunk <= n ? chunk : n - k);
    CHECK(vt_row_image(t, 0, 0, &v));
    if (vt_row_image(t, 0, 0, &v)) {
        CHECK_INT(v.w, 64);
        CHECK_INT(v.h, 48);
        CHECK(v.npal > 200);
        for (y = 0; y < 48; y++)
            for (x = 0; x < 64; x++) {
                vt_u32 p = pixel(&v, x, y);
                int want[3], got[3], c;
                want[0] = x * 4;
                want[1] = y * 5;
                want[2] = (x * 2 + y * 3) % 256;
                got[0] = (int)(p >> 16) & 0xFF;
                got[1] = (int)(p >> 8) & 0xFF;
                got[2] = (int)p & 0xFF;
                for (c = 0; c < 3; c++) {
                    err = got[c] > want[c] ? got[c] - want[c] : want[c] - got[c];
                    sum += err;
                    if (err > worst)
                        worst = err;
                }
            }
        CHECK(worst <= 48);
        CHECK(sum <= 64L * 48 * 3 * 12);
    }
    vt_free(t);
}

void suite_sixel(void)
{
    encoder_streams_decode();
    decodes_a_known_image();
    hls_and_rgb_registers();
    repeat_overprint_and_redefinition();
    aspect_ratio_from_p1();
    streams_across_writes_and_past_256_bytes();
    oversized_images_are_cut();
    scrolls_with_the_text_into_the_scrollback();
    text_and_erase_replace_the_image();
    frames_in_one_place_keep_one_image();
    decsdm_places_at_the_top_left();
    private_and_shared_registers();
    replies_tell_programs_about_sixel();
    cancel_drops_the_image();
    reflow_carries_the_image();
    the_oldest_image_gives_way();
    image_over_half_a_wide_glyph();
}
