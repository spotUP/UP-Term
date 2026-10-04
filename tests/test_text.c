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

void suite_text(void)
{
    style_sweep_keeps_rows_pushed_above_the_screen();
    widths_count_cells_as_glibc_does();
    bmp_emoji_take_two_cells();
}
