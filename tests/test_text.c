/* Text: widths, characters beyond the BMP, combining marks, faint colours
 * (gap list items 2, 13, 16; plan 2026-10-04-gaps-g2-text.md). */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../engine/vtwidth.h"

/* glibc's wcwidth on Unicode 16 (tools/gen_width.py): what the remote
 * program counts, so what the cursor must move by. */
static void widths_count_cells_as_glibc_does(void)
{
    CHECK_INT(vt_char_width('a'), 1);
    CHECK_INT(vt_char_width(0x00AD), 1);  /* soft hyphen: glibc's special case */
    CHECK_INT(vt_char_width(0x0301), 0);  /* combining acute */
    CHECK_INT(vt_char_width(0x0E47), 0);  /* Thai mai tai khu */
    CHECK_INT(vt_char_width(0x0941), 0);  /* Devanagari vowel sign u */
    CHECK_INT(vt_char_width(0x1160), 0);  /* Hangul jungseong filler */
    CHECK_INT(vt_char_width(0x11FF), 0);
    CHECK_INT(vt_char_width(0x115F), 2);  /* Hangul choseong filler */
    CHECK_INT(vt_char_width(0x200B), 0);
    CHECK_INT(vt_char_width(0x200D), 0);  /* ZWJ */
    CHECK_INT(vt_char_width(0xFE0F), 0);  /* VS16 */
    CHECK_INT(vt_char_width(0x2060), 0);  /* word joiner */
    CHECK_INT(vt_char_width(0x0600), 1);  /* Arabic number sign: Cf, not ignorable */
    CHECK_INT(vt_char_width(0x231A), 2);  /* watch: emoji presentation */
    CHECK_INT(vt_char_width(0x26A1), 2);
    CHECK_INT(vt_char_width(0x2705), 2);
    CHECK_INT(vt_char_width(0x2B50), 2);
    CHECK_INT(vt_char_width(0x2764), 1);  /* heart: text presentation */
    CHECK_INT(vt_char_width(0x4E2D), 2);
    CHECK_INT(vt_char_width(0xFF21), 2);  /* fullwidth A */
    CHECK_INT(vt_char_width(0x1F600), 2); /* grinning face */
    CHECK_INT(vt_char_width(0x1F1E6), 1); /* regional indicator: N */
    CHECK_INT(vt_char_width(0x20000), 2); /* CJK Ext B */
    CHECK_INT(vt_char_width(0x1D400), 1); /* math bold A */
    CHECK_INT(vt_char_width(0xF0001), 1); /* plane 15 private use (Nerd Font icons) */
    CHECK_INT(vt_char_width(0xE0001), 0); /* language tag: ignorable */
    CHECK_INT(vt_char_width(0x1FAE9), 2); /* Unicode 16: face with bags under eyes */
}

/* An emoji-presentation BMP character is two cells, as the remote counts it. */
static void bmp_emoji_take_two_cells(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    int x, y;
    h_put(t, "\xe2\x8c\x9a" "z"); /* U+231A watch */
    CHECK_INT(h_cell(t, 0, 0)->width, 2);
    CHECK_INT(h_cell(t, 2, 0)->ch, 'z');
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 3);
    vt_free(t);
}

/* The rare-style table (underline colour, font) is swept when full: the
 * rows a reflow pushed above the screen, brought back by the next grow,
 * keep their entries (they were neither marked nor renumbered). */
static void style_sweep_keeps_rows_pushed_above_the_screen(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    char seq[32];
    int i;
    vt_set_reflow(t, 1);
    h_put(t, "\033[4;58:5:1mA\033[m\r\nb\r\nc");
    vt_resize(t, 10, 1); /* rows 0 and 1 go above the screen */
    CHECK_STR(h_row(t, 0), "c");
    for (i = 0; i < 300; i++) { /* 300 underline colours, each overwriting the last */
        int k = 0;
        memcpy(seq, "\r\033[58:2::9:", 11);
        k = 11;
        seq[k++] = (char)('0' + i / 100);
        seq[k++] = ':';
        seq[k++] = (char)('0' + i / 10 % 10);
        seq[k++] = (char)('0' + i % 10);
        seq[k++] = 'm';
        seq[k++] = 'x';
        seq[k] = 0;
        h_put(t, seq);
    }
    vt_resize(t, 10, 3);
    CHECK_STR(h_row(t, 0), "A");
    CHECK_INT(vt_cell_underline_color(t, h_cell(t, 0, 0)), 1);
    vt_free(t);
}

/* What a selection copies, as UTF-8 (the whole of rows ay..by). */
static const char *copied(vt_term *t, int ay, int by)
{
    static char buf[2048];
    vt_copy_text(t, 0, ay, 1000, by, buf, sizeof(buf));
    return buf;
}

#define GRIN "\xf0\x9f\x98\x80"      /* U+1F600 */
#define NERD "\xf3\xb0\x80\x81"      /* U+F0001, plane 15 private use */

/* An emoji is one wide character with its own code point, not U+FFFD. */
static void astral_characters_keep_their_code_point(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    vt_u32 cp[VT_CLUSTER_CPS];
    int x, y;
    CHECK_INT(sizeof(vt_cell), 2 * sizeof(vt_color) + 8); /* no field grew: 16 bytes on the Amiga */
    h_put(t, GRIN "z" NERD "!");
    CHECK_INT(h_cell(t, 0, 0)->width, 2);
    CHECK_INT(h_cell(t, 1, 0)->width, 0);
    CHECK(VT_CELL_IS_CLUSTER(h_cell(t, 0, 0)));
    CHECK_INT(vt_cell_char(t, h_cell(t, 0, 0)), 0x1F600);
    CHECK_INT(vt_cell_text(t, h_cell(t, 0, 0), cp), 1);
    CHECK_INT(h_cell(t, 2, 0)->ch, 'z');
    CHECK_INT(h_cell(t, 3, 0)->width, 1); /* the icon: one cell, as wcwidth says */
    CHECK_INT(vt_cell_char(t, h_cell(t, 3, 0)), 0xF0001);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 5);
    CHECK_STR(copied(t, 0, 0), GRIN "z" NERD "!");
    CHECK_STR(h_row(t, 0), GRIN "z" NERD "!");
    /* REP repeats the character, not a replacement */
    h_put(t, "\r\n" GRIN "\033[2b");
    CHECK_STR(h_row(t, 1), GRIN GRIN GRIN);
    vt_free(t);
}

/* Scrolled into the scrollback, reflowed, overwritten: the cell's
 * reference travels with it, and an overwritten one is plain again. */
static void astral_characters_survive_scroll_and_reflow(void)
{
    vt_term *t = h_new(6, 2, VT_XTERM);
    vt_set_reflow(t, 1);
    h_put(t, "ab" GRIN "cd\r\n\r\n");
    CHECK_STR(copied(t, -1, -1), "ab" GRIN "cd");
    h_put(t, "xy" GRIN GRIN);
    vt_resize(t, 4, 3);
    CHECK_STR(copied(t, 0, 2), "\nxy" GRIN GRIN); /* the blank row above, then the line, rewrapped */
    h_put(t, "\033[H\033[2J" "abcd");
    CHECK(!VT_CELL_IS_CLUSTER(h_cell(t, 0, 0)));
    CHECK_STR(h_row(t, 0), "abcd");
    vt_free(t);
}

/* Thousands of different emoji through a small screen: the entries of the
 * ones that scrolled away are used again, the ones on screen stay right. */
static void cluster_table_reuses_what_scrolled_away(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    char s[8];
    int i, bad = 0;
    vt_set_scrollback(t, 0);
    for (i = 0; i < 5000; i++) {
        vt_u32 c = 0x20000 + (vt_u32)i; /* CJK Extension B: all wide */
        s[0] = (char)(0xF0 | (c >> 18));
        s[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        s[2] = (char)(0x80 | ((c >> 6) & 0x3F));
        s[3] = (char)(0x80 | (c & 0x3F));
        s[4] = 0;
        h_put(t, s);
        if (vt_cell_char(t, h_cell(t, 2 * (i % 5), (i / 5) ? 1 : 0)) != c)
            bad++;
    }
    CHECK_INT(bad, 0);
    vt_free(t);
}

/* Every entry named by a line: the next new character gets U+FFFD, and
 * nothing already there changes. */
static void cluster_table_full_gives_the_replacement(void)
{
    vt_term *t = h_new(80, 2, VT_XTERM);
    char s[8];
    int i;
    vt_set_scrollback(t, 100);
    for (i = 0; i < 2049; i++) {
        vt_u32 c = 0xF0000 + (vt_u32)i; /* plane 15 private use: one cell each */
        s[0] = (char)(0xF0 | (c >> 18));
        s[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        s[2] = (char)(0x80 | ((c >> 6) & 0x3F));
        s[3] = (char)(0x80 | (c & 0x3F));
        s[4] = 0;
        h_put(t, s);
    }
    {
        int x, y;
        vt_cursor(t, &x, &y);
        CHECK_INT(vt_cell_char(t, h_cell(t, x - 1, y)), 0xFFFD);
        CHECK_INT(vt_cell_char(t, h_cell(t, x - 2, y)), 0xF0000 + 2047);
    }
    CHECK_INT(vt_cell_char(t, vt_row(t, -vt_scrollback_lines(t), 0)), 0xF0000);
    vt_free(t);
}

/* A title beyond the BMP reaches the host as UTF-8, whole. */
static void title_keeps_characters_beyond_the_bmp(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    h_put(t, "\033]2;a" GRIN "b\007");
    CHECK_STR(vt_title(t), "a" GRIN "b");
    vt_free(t);
}

/* Combining marks join their character: shown with it, copied with it. */
static void combining_marks_stay_with_their_character(void)
{
    vt_term *t = h_new(6, 3, VT_XTERM);
    vt_u32 cp[VT_CLUSTER_CPS];
    int x, y;
    h_put(t, "e\xcc\x81" "x"); /* e + U+0301 */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(h_cell(t, 1, 0)->ch, 'x');
    CHECK_INT(vt_cell_text(t, h_cell(t, 0, 0), cp), 2);
    CHECK_INT(cp[0], 'e');
    CHECK_INT(cp[1], 0x301);
    CHECK_INT(vt_cell_char(t, h_cell(t, 0, 0)), 'e');
    CHECK_STR(copied(t, 0, 0), "e\xcc\x81x");
    /* the same cluster again shares the entry */
    h_put(t, "e\xcc\x81");
    CHECK_INT(h_cell(t, 2, 0)->ch, h_cell(t, 0, 0)->ch);
    /* on a wide character, and on the last column with the wrap pending */
    h_put(t, "\r\n\xe4\xb8\xad\xcc\x88" "abc" "d\xcc\xa3");
    CHECK_STR(copied(t, 1, 1), "\xe4\xb8\xad\xcc\x88" "abcd\xcc\xa3");
    CHECK_INT(h_cell(t, 1, 1)->width, 0);
    /* at the start of a row there is no character: dropped */
    h_put(t, "\r\n\xcc\x81" "q");
    CHECK_STR(h_row(t, 2), "q");
    vt_free(t);
}

/* Emoji sequences round-trip: VS16 and ZWJ stay with the character before
 * them, the widths stay wcwidth's (a VS16 does not widen). */
static void emoji_sequences_copy_back_whole(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    int x, y;
    h_put(t, "\xe2\x9d\xa4\xef\xb8\x8f"); /* U+2764 U+FE0F */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 1);
    h_put(t, "\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9"); /* man ZWJ woman */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 5);
    CHECK_STR(copied(t, 0, 0), "\xe2\x9d\xa4\xef\xb8\x8f\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9");
    vt_free(t);
}

/* vt_copy_text without a buffer: the length a whole copy needs. */
static void copy_text_measures_without_a_buffer(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    char buf[64];
    h_put(t, "ab" GRIN "\r\ncd");
    CHECK_INT(vt_copy_text(t, 0, 0, 9, 1, 0, 0), 9);
    CHECK_INT(vt_copy_text(t, 0, 0, 9, 1, buf, sizeof(buf)), 9);
    vt_free(t);
}

/* Faint text in any colour is dimmed: halfway to its background, as a
 * direct colour (a palette screen draws it with the nearest pen). Before,
 * only the default and the greys were. */
static void faint_dims_every_colour(void)
{
    vt_term *t = h_new(20, 2, VT_XTERM);
    vt_color f, b;
    vt_set_default_colors(t, 0xC0C0C0UL, 0x000000UL, 0xC0C0C0UL);
    h_put(t, "\033[2;31ma\033[0;2;38;2;200;100;50mb\033[0;2mc\033[0;2;33;44md\033[0;2;7;32me");
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, VT_RGB(0x66, 0, 0));          /* colour 1, 0xCD0000, over black */
    vt_resolve_colors(t, h_cell(t, 1, 0), &f, &b);
    CHECK_INT(f, VT_RGB(100, 50, 25));
    vt_resolve_colors(t, h_cell(t, 2, 0), &f, &b);
    CHECK_INT(f, VT_RGB(0x60, 0x60, 0x60));    /* the default text colour */
    CHECK_INT(b, VT_COLOR_DEFAULT_BG);
    vt_resolve_colors(t, h_cell(t, 3, 0), &f, &b);
    CHECK_INT(f, VT_RGB(0x66, 0x66, 0x77));    /* 0xCDCD00 toward 0x0000EE */
    CHECK_INT(b, 4);
    vt_resolve_colors(t, h_cell(t, 4, 0), &f, &b); /* inverse: the dimmed colour behind */
    CHECK_INT(b, VT_RGB(0, 0x66, 0));
    vt_free(t);
    /* the amiga personality keeps its pens */
    t = h_new(20, 2, VT_AMIGA);
    h_put(t, "\033[2;33mx\033[0;2;37my");
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, 3);
    vt_resolve_colors(t, h_cell(t, 1, 0), &f, &b);
    CHECK_INT(f, 2);
    vt_free(t);
}

void suite_text(void)
{
    style_sweep_keeps_rows_pushed_above_the_screen();
    widths_count_cells_as_glibc_does();
    bmp_emoji_take_two_cells();
    astral_characters_keep_their_code_point();
    astral_characters_survive_scroll_and_reflow();
    cluster_table_reuses_what_scrolled_away();
    cluster_table_full_gives_the_replacement();
    title_keeps_characters_beyond_the_bmp();
    combining_marks_stay_with_their_character();
    emoji_sequences_copy_back_whole();
    copy_text_measures_without_a_buffer();
    faint_dims_every_colour();
}
