/* iTerm2 inline images (OSC 1337 ; File=...): the pure argument parser, and one
 * run through the engine's input (h_put, the entry point test_kitty.c uses)
 * that proves the new path ran (vt_iterm_placed) and asserts where the picture
 * and the cursor ended. Cells are 2 x 6 pixels; the picture is png_grey8, 3 x 5. */
#include "harness.h"
#include <stdlib.h>
#include <string.h>
#define VT_MALLOC(n) malloc(n)
#define VT_FREE(p) free(p)
#define KIT_RAW_MAX (4L * 1024L * 1024L)
#include "../engine/iterm.c"
#include "kitty_png_vectors.h"

static void parse_of(const char *s, iterm_args *a)
{
    iterm_parse(s, (int)strlen(s), a);
}

static void parser_reads_every_argument(void)
{
    iterm_args a;
    parse_of("name=Zm9v;size=12;width=3;height=50%;preserveAspectRatio=0;inline=1", &a);
    CHECK_INT(a.inl, 1);
    CHECK_INT(a.keep, 0);
    CHECK_INT(a.nomove, 0);
    CHECK_INT(a.size, 12);
    CHECK_INT(a.wu, ITERM_CELLS);
    CHECK_INT(a.wv, 3);
    CHECK_INT(a.hu, ITERM_PERCENT);
    CHECK_INT(a.hv, 50);
    parse_of("width=40px;height=auto;doNotMoveCursor=1;inline=1;unknown=7", &a);
    CHECK_INT(a.wu, ITERM_PIXELS);
    CHECK_INT(a.wv, 40);
    CHECK_INT(a.hu, ITERM_AUTO);
    CHECK_INT(a.nomove, 1);
    CHECK_INT(a.keep, 1);
    CHECK_INT(a.size, -1);
}

static void inline_is_one_or_a_download(void)
{
    iterm_args a;
    parse_of("", &a);
    CHECK_INT(a.inl, 0);
    parse_of("inline=0", &a);
    CHECK_INT(a.inl, 0);
    parse_of("inline=2", &a);
    CHECK_INT(a.inl, 0);
    parse_of("inline=1", &a);
    CHECK_INT(a.inl, 1);
}

static void units_become_cells(void)
{
    iterm_args a;
    CHECK_INT(iterm_cells(ITERM_AUTO, 0, 2, 80), 0);
    CHECK_INT(iterm_cells(ITERM_CELLS, 4, 2, 80), 4);
    CHECK_INT(iterm_cells(ITERM_PIXELS, 5, 2, 80), 3);  /* rounded up to whole cells */
    CHECK_INT(iterm_cells(ITERM_PIXELS, 4, 2, 80), 2);
    CHECK_INT(iterm_cells(ITERM_PERCENT, 50, 2, 80), 40);
    CHECK_INT(iterm_cells(ITERM_PERCENT, 1, 2, 10), 1); /* never 0 once asked for */
    CHECK_INT(iterm_cells(ITERM_CELLS, 500, 2, 80), 80); /* the screen's width at most */
    parse_of("width=0;height=abc", &a);
    CHECK_INT(a.wu, ITERM_AUTO);
    CHECK_INT(a.hu, ITERM_AUTO);
}

static vt_term *iterm_term(int cols, int rows)
{
    vt_term *t = h_new(cols, rows, VT_XTERM);
    vt_set_cell_pixels(t, 2, 6);
    return t;
}

/* ESC ] 1337 ; File = args : base64 <term> */
static void put_file(vt_term *t, const char *args, const char *b64, const char *term)
{
    h_put(t, "\033]1337;File=");
    h_put(t, args);
    h_put(t, ":");
    h_put(t, b64);
    h_put(t, term);
}

/* The one reachability test: bytes in at the engine input, picture on the cells. */
static void file_sequence_places_the_picture_and_moves_the_cursor(void)
{
    vt_term *t = iterm_term(20, 10);
    vt_image_view v;
    int x, y;
    h_put(t, "ab");
    CHECK_INT(vt_iterm_placed(t), 0);
    /* width=4 cells = 8 px for 3 source px: 5 * 8 / 3 = 13 px high = 3 rows of 6 */
    put_file(t, "name=Zm9v;size=100;width=4;inline=1", png_grey8_b64, "\007");
    CHECK_INT(vt_iterm_placed(t), 1);
    CHECK_INT(vt_kitty_placed(t), 1); /* it is the kitty placement underneath */
    CHECK_INT(vt_images(t), 1);
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.col0, 2);              /* at the cursor */
    CHECK_INT(v.w, 8);
    CHECK_INT(v.h, 13);
    CHECK(vt_row_image(t, 2, 0, &v));  /* three rows */
    CHECK(!vt_row_image(t, 3, 0, &v));
    CHECK_INT(h_cell(t, 2, 0)->pad & VT_CELL_IMAGE, VT_CELL_IMAGE);
    CHECK_INT(h_cell(t, 5, 0)->pad & VT_CELL_IMAGE, VT_CELL_IMAGE); /* columns 2-5 */
    CHECK_INT(h_cell(t, 6, 0)->pad & VT_CELL_IMAGE, 0);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 6);                   /* after the last column */
    CHECK_INT(y, 2);                   /* on the last row */
    vt_free(t);
}

static void st_ends_it_too_and_do_not_move_cursor_keeps_it(void)
{
    vt_term *t = iterm_term(20, 10);
    int x, y;
    h_put(t, "ab");
    put_file(t, "width=4;doNotMoveCursor=1;inline=1", png_grey8_b64, "\033\\");
    CHECK_INT(vt_iterm_placed(t), 1);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(y, 0);
    vt_free(t);
}

static void both_sizes_fit_the_box_when_the_ratio_is_kept(void)
{
    vt_term *t = iterm_term(20, 10);
    vt_image_view v;
    /* box 10 x 2 cells = 20 x 12 px; 3 x 5 source: the height decides, 12 px */
    put_file(t, "width=10;height=2;inline=1", png_grey8_b64, "\007");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.h, 12);
    CHECK_INT(v.w, 7); /* 3 * 12 / 5 */
    vt_free(t);
    t = iterm_term(20, 10);
    put_file(t, "width=10;height=2;preserveAspectRatio=0;inline=1", png_grey8_b64, "\007");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.w, 20);
    CHECK_INT(v.h, 12);
    vt_free(t);
}

static void inline_zero_and_missing_inline_place_nothing(void)
{
    vt_term *t = iterm_term(20, 10);
    int x, y;
    put_file(t, "inline=0;width=4", png_grey8_b64, "\007");
    put_file(t, "width=4", png_grey8_b64, "\007"); /* inline defaults to 0 */
    CHECK_INT(vt_iterm_placed(t), 0);
    CHECK_INT(vt_images(t), 0);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 0);
    CHECK_INT(y, 0);
    h_put(t, "z"); /* the swallowed bytes did not stay in the stream */
    CHECK_INT(h_cell(t, 0, 0)->ch, 'z');
    vt_free(t);
}

static void a_long_name_does_not_push_the_arguments_out(void)
{
    vt_term *t = iterm_term(20, 10);
    char args[600];
    int i, n = (int)strlen("name=");
    memcpy(args, "name=", n);
    for (i = 0; i < 400; i++)
        args[n++] = 'A';
    strcpy(args + n, ";width=4;inline=1");
    put_file(t, args, png_grey8_b64, "\007");
    CHECK_INT(vt_iterm_placed(t), 1);
    vt_free(t);
}

static void garbage_and_cancel_place_nothing(void)
{
    vt_term *t = iterm_term(20, 10);
    put_file(t, "inline=1", "!!!!", "\007");
    put_file(t, "inline=1", "AAAA", "\007"); /* not a picture */
    h_put(t, "\033]1337;File=inline=1:iVBORw0KGgo\030"); /* CAN: cancelled */
    h_put(t, "x");
    CHECK_INT(vt_iterm_placed(t), 0);
    CHECK_INT(vt_images(t), 0);
    CHECK_INT(h_cell(t, 0, 0)->ch, 'x');
    vt_free(t);
}

/* A payload past the kitty cap is discarded whole (the cap is KIT_RAW_MAX, as for kitty). */
static void an_oversize_payload_is_discarded_without_a_picture(void)
{
    vt_term *t = iterm_term(20, 10);
    long n = (KIT_RAW_MAX / 3 + 4096) * 4;
    char *big = (char *)malloc((size_t)n + 1);
    long i;
    CHECK(big != 0);
    if (!big) {
        vt_free(t);
        return;
    }
    memset(big, 'A', (size_t)n);
    big[n] = 0;
    h_put(t, "\033]1337;File=inline=1:");
    h_put(t, big);
    h_put(t, "\007");
    free(big);
    (void)i;
    CHECK_INT(vt_iterm_placed(t), 0);
    CHECK_INT(vt_images(t), 0);
    /* and the terminal still takes a good one */
    put_file(t, "width=4;inline=1", png_grey8_b64, "\007");
    CHECK_INT(vt_iterm_placed(t), 1);
    vt_free(t);
}

/* ---- inline=0: saved through the host writer (O5, O10) ---- */
static char sv_path[200];
static unsigned char sv_data[64];
static long sv_len;
static int sv_calls;

static int cap_writer(const char *path, const vt_u8 *data, long n)
{
    sv_calls++;
    strncpy(sv_path, path, sizeof(sv_path) - 1);
    sv_len = n;
    memcpy(sv_data, data, (size_t)(n < 64 ? n : 64));
    return 1;
}

static void sv_reset(void)
{
    sv_calls = 0;
    sv_len = 0;
    sv_path[0] = 0;
}

static void names_are_base_names_only(void)
{
    char o[64];
    iterm_name("Li4vLi4vZXRjL3Bhc3N3ZA==", 24, o, sizeof(o)); /* ../../etc/passwd */
    CHECK(!strcmp(o, "passwd"));
    iterm_name("RGgwOnN5cy9ldmlsLnR4dA==", 24, o, sizeof(o)); /* Dh0:sys/evil.txt */
    CHECK(!strcmp(o, "evil.txt"));
    iterm_name("Li4=", 4, o, sizeof(o)); /* .. */
    CHECK(!strcmp(o, "download"));
    iterm_name("", 0, o, sizeof(o));
    CHECK(!strcmp(o, "download"));
    iterm_name("Zm9v", 4, o, sizeof(o));
    CHECK(!strcmp(o, "foo"));
    iterm_path(o, 20, "h", "verylongname.txt"); /* cut, always terminated */
    CHECK_INT(strlen(o), 19);
}

static void inline_zero_is_saved_through_the_host_writer(void)
{
    vt_term *t = iterm_term(20, 10);
    int x, y;
    sv_reset();
    vt_set_download(t, cap_writer, "am/ig:a1");
    /* name = "dir/note.txt", payload "hello" */
    put_file(t, "name=ZGlyL25vdGUudHh0;inline=0;size=5", "aGVsbG8=", "\007");
    CHECK_INT(sv_calls, 1);
    CHECK(!strcmp(sv_path, "ENVARC:UP-Term/Downloads/amiga1/note.txt"));
    CHECK_INT(sv_len, 5);
    CHECK(!memcmp(sv_data, "hello", 5));
    CHECK_INT(vt_iterm_placed(t), 0);
    CHECK_INT(vt_images(t), 0);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 0);
    /* no name= : a fixed name; no host set: "local" */
    sv_reset();
    vt_set_download(t, cap_writer, 0);
    put_file(t, "inline=0", "aGVsbG8=", "\033\\");
    CHECK_INT(sv_calls, 1);
    CHECK(!strcmp(sv_path, "ENVARC:UP-Term/Downloads/local/download"));
    /* without a writer nothing is saved */
    sv_reset();
    vt_set_download(t, 0, "x");
    put_file(t, "inline=0", "aGVsbG8=", "\007");
    CHECK_INT(sv_calls, 0);
    vt_free(t);
}

static void a_download_over_four_megabytes_is_dropped(void)
{
    vt_term *t = iterm_term(20, 10);
    long n = (KIT_RAW_MAX / 3 + 4096) * 4;
    char *big = (char *)malloc((size_t)n + 1);
    CHECK(big != 0);
    if (!big) {
        vt_free(t);
        return;
    }
    memset(big, 'A', (size_t)n);
    big[n] = 0;
    sv_reset();
    vt_set_download(t, cap_writer, "h");
    h_put(t, "\033]1337;File=name=Zm9v;inline=0:");
    h_put(t, big);
    h_put(t, "\007");
    free(big);
    CHECK_INT(sv_calls, 0);
    put_file(t, "name=Zm9v;inline=0", "aGVsbG8=", "\007"); /* and the next one is taken */
    CHECK_INT(sv_calls, 1);
    vt_free(t);
}

static void report_cell_size_answers_height_then_width(void)
{
    vt_term *t = iterm_term(20, 10);
    h_reply_clear();
    h_put(t, "\033]1337;ReportCellSize\033\\");
    CHECK(!strcmp(h_reply, "\033]1337;ReportCellSize=6;2\033\\"));
    h_reply_clear();
    h_put(t, "\033]1337;Capabilities\033\\");
    CHECK_INT(h_reply_len, 0);
    vt_free(t);
}

void suite_iterm2(void)
{
    names_are_base_names_only();
    inline_zero_is_saved_through_the_host_writer();
    a_download_over_four_megabytes_is_dropped();
    report_cell_size_answers_height_then_width();
    parser_reads_every_argument();
    inline_is_one_or_a_download();
    units_become_cells();
    file_sequence_places_the_picture_and_moves_the_cursor();
    st_ends_it_too_and_do_not_move_cursor_keeps_it();
    both_sizes_fit_the_box_when_the_ratio_is_kept();
    inline_zero_and_missing_inline_place_nothing();
    a_long_name_does_not_push_the_arguments_out();
    garbage_and_cancel_place_nothing();
    an_oversize_payload_is_discarded_without_a_picture();
}
