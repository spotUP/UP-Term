#include <string.h>
/* xterm personality: parser, grid and the sequences Unix ports send. */
#include "harness.h"

static void parser_splits_sequences_across_writes(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "ab\033");
    h_put(t, "[");
    h_put(t, "2");
    h_put(t, "D");
    h_put(t, "X");
    CHECK_STR(h_row(t, 0), "Xb");
    vt_free(t);
}

static void c0_controls_execute_inside_csi(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "abc\033[\r1C!"); /* CR runs mid-sequence, then CUF 1 */
    CHECK_STR(h_row(t, 0), "a!c");
    vt_free(t);
}

static void can_aborts_a_sequence(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033[3\030X");
    CHECK_STR(h_row(t, 0), "X");
    vt_free(t);
}

static void unknown_private_sequences_are_swallowed(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "a\033[?1;2$pb\033[>4;2mc\033[=5ud");
    CHECK_STR(h_row(t, 0), "abcd");
    vt_free(t);
}

static void osc_title_ends_on_bel_and_st(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033]0;hello\007x");
    CHECK_STR(vt_title(t), "hello");
    h_put(t, "\033]2;w\xc3\xb6rld\033\\y");
    CHECK_STR(vt_title(t), "w\xc3\xb6rld");
    CHECK_STR(h_row(t, 0), "xy");
    vt_free(t);
}

static void dcs_strings_are_ignored(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "a\033P1$r0m\033\\b");
    CHECK_STR(h_row(t, 0), "ab");
    vt_free(t);
}

static void utf8_decodes_to_cells(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\xc3\xa5\xe2\x94\x80\xe2\x82\xac");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0xE5);
    CHECK_INT(h_cell(t, 1, 0)->ch, 0x2500);
    CHECK_INT(h_cell(t, 2, 0)->ch, 0x20AC);
    h_put(t, "\xff" "a");
    CHECK_INT(h_cell(t, 3, 0)->ch, 0xFFFD);
    CHECK_INT(h_cell(t, 4, 0)->ch, 'a');
    vt_free(t);
}

static void wide_glyphs_take_two_cells(void)
{
    vt_term *t = h_new(6, 2, VT_XTERM);
    h_put(t, "a\xe4\xb8\xadz"); /* U+4E2D */
    CHECK_INT(h_cell(t, 1, 0)->width, 2);
    CHECK_INT(h_cell(t, 2, 0)->width, 0);
    CHECK_INT(h_cell(t, 3, 0)->ch, 'z');
    h_put(t, "\033[1;3Hx"); /* overwrite the right half: both halves go */
    CHECK_INT(h_cell(t, 1, 0)->ch, ' ');
    CHECK_INT(h_cell(t, 1, 0)->width, 1);
    CHECK_INT(h_cell(t, 2, 0)->ch, 'x');
    vt_free(t);
}

static void autowrap_is_deferred(void)
{
    vt_term *t = h_new(5, 3, VT_XTERM);
    int x, y;
    h_put(t, "abcde");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 4);
    CHECK_INT(y, 0);
    h_put(t, "\r\n");      /* a full row followed by CR LF: no blank line */
    CHECK_STR(h_screen(t), "abcde");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    h_put(t, "12345X");
    CHECK_STR(h_screen(t), "abcde|12345|X");
    CHECK(vt_row_wrapped(t, 1));
    vt_free(t);
}

static void autowrap_off_overwrites_last_column(void)
{
    vt_term *t = h_new(5, 2, VT_XTERM);
    h_put(t, "\033[?7labcdefg");
    CHECK_STR(h_screen(t), "abcdg");
    vt_free(t);
}

static void erase_uses_current_background(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    h_put(t, "\033[44m\033[2J");
    CHECK_INT(h_cell(t, 5, 1)->bg, 4);
    h_put(t, "\033[0m\033[K");
    CHECK_INT(h_cell(t, 5, 0)->bg, VT_COLOR_DEFAULT);
    vt_free(t);
}

static void erase_display_does_not_home(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    int x, y;
    h_put(t, "\033[2;4H\033[2J");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 3);
    CHECK_INT(y, 1);
    vt_free(t);
}

static void erase_modes(void)
{
    vt_term *t = h_new(5, 3, VT_XTERM);
    h_put(t, "aaaaa\r\nbbbbb\r\nccccc\033[2;3H\033[1J");
    CHECK_STR(h_screen(t), "|   bb|ccccc");
    h_put(t, "\033[0J");
    CHECK_STR(h_screen(t), "");
    h_put(t, "\033[1;1Habcde\033[1;3H\033[1K");
    CHECK_STR(h_row(t, 0), "   de");
    h_put(t, "\033[2K");
    CHECK_STR(h_row(t, 0), "");
    vt_free(t);
}

static void scroll_region_confines_linefeed(void)
{
    vt_term *t = h_new(5, 5, VT_XTERM);
    h_put(t, "1\r\n2\r\n3\r\n4\r\n5");
    h_put(t, "\033[2;4r");          /* region rows 2-4, cursor homes */
    h_put(t, "\033[4;1H\nX");       /* LF at region bottom scrolls 2-4 only */
    CHECK_STR(h_screen(t), "1|3|4|X|5");
    CHECK_INT(vt_scrollback_lines(t), 0); /* region scroll saves nothing */
    vt_free(t);
}

static void reverse_index_at_top_scrolls_down(void)
{
    vt_term *t = h_new(5, 3, VT_XTERM);
    h_put(t, "a\r\nb\r\nc\033[1;1H\033M");
    CHECK_STR(h_screen(t), "|a|b");
    vt_free(t);
}

static void origin_mode_positions_relative_to_region(void)
{
    vt_term *t = h_new(5, 5, VT_XTERM);
    int x, y;
    h_put(t, "\033[2;4r\033[?6h\033[1;1HX");
    CHECK_STR(h_row(t, 1), "X");
    h_put(t, "\033[9;1H");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 3); /* clamped to the region bottom */
    h_reply_clear();
    h_put(t, "\033[6n");
    CHECK_STR(h_reply, "\033[3;1R");
    vt_free(t);
}

static void insert_and_delete_characters(void)
{
    vt_term *t = h_new(6, 1, VT_XTERM);
    h_put(t, "abcdef\033[1;2H\033[2@");
    CHECK_STR(h_row(t, 0), "a  bcd");
    h_put(t, "\033[3P");
    CHECK_STR(h_row(t, 0), "acd");
    h_put(t, "\033[4hXY\033[4l");
    CHECK_STR(h_row(t, 0), "aXYcd");
    vt_free(t);
}

static void insert_and_delete_lines(void)
{
    vt_term *t = h_new(3, 4, VT_XTERM);
    h_put(t, "a\r\nb\r\nc\r\nd\033[2;2H\033[L");
    CHECK_STR(h_screen(t), "a||b|c");
    h_put(t, "\033[2M");
    CHECK_STR(h_screen(t), "a|c");
    vt_free(t);
}

static void scroll_up_down_sequences(void)
{
    vt_term *t = h_new(3, 3, VT_XTERM);
    h_put(t, "a\r\nb\r\nc\033[S");
    CHECK_STR(h_screen(t), "b|c");
    h_put(t, "\033[2T");
    CHECK_STR(h_screen(t), "||b");
    vt_free(t);
}

static void tabs_default_every_eight(void)
{
    vt_term *t = h_new(20, 1, VT_XTERM);
    h_put(t, "a\tb\tc\td");
    CHECK_STR(h_row(t, 0), "a       b       c  d");
    h_put(t, "\033[3g\r\t!"); /* no stops: tab goes to the last column */
    CHECK_INT(h_cell(t, 19, 0)->ch, '!');
    h_put(t, "\033[1;4H\033H\r\tX"); /* HTS at column 4 */
    CHECK_INT(h_cell(t, 3, 0)->ch, 'X');
    vt_free(t);
}

static void back_tab(void)
{
    vt_term *t = h_new(20, 1, VT_XTERM);
    h_put(t, "\033[1;18H\033[ZX");
    CHECK_INT(h_cell(t, 16, 0)->ch, 'X');
    vt_free(t);
}

/* A 24-bit colour is kept exactly: a true-colour screen shows the value
 * the program asked for (it was rgb555, which dropped 3 bits a channel). */
static void rgb_colour_keeps_all_24_bits(void)
{
    vt_term *t = h_new(4, 1, VT_XTERM);
    h_put(t, "\033[38;2;1;130;255;48;2;7;8;9mA");
    CHECK_INT(h_cell(t, 0, 0)->fg == (VT_COLOR_RGB | 0x0182FFUL), 1);
    CHECK_INT(h_cell(t, 0, 0)->bg == (VT_COLOR_RGB | 0x070809UL), 1);
    vt_free(t);
}

/* ---- phase A1: the whole of SGR ---- */

static void underline_styles_and_colour(void)
{
    vt_term *t = h_new(10, 1, VT_XTERM);
    h_put(t, "\033[4:3ma\033[21mb\033[4:0mc\033[4;58;5;196md\033[58:2::1:2:3me\033[59mf\033[24mg");
    CHECK_INT(h_cell(t, 0, 0)->deco & VT_DECO_UL_MASK, VT_UL_CURLY);
    CHECK_INT((h_cell(t, 0, 0)->attr & VT_ATTR_UNDERLINE) != 0, 1);
    CHECK_INT(h_cell(t, 1, 0)->deco & VT_DECO_UL_MASK, VT_UL_DOUBLE);
    CHECK_INT(h_cell(t, 2, 0)->attr & VT_ATTR_UNDERLINE, 0);
    CHECK_INT(h_cell(t, 3, 0)->deco & VT_DECO_UL_MASK, VT_UL_SINGLE);
    CHECK_INT(vt_cell_underline_color(t, h_cell(t, 3, 0)) == 196, 1);
    CHECK_INT(vt_cell_underline_color(t, h_cell(t, 4, 0)) == VT_RGB(1, 2, 3), 1);
    CHECK_INT(vt_cell_underline_color(t, h_cell(t, 5, 0)) == VT_COLOR_DEFAULT, 1);
    CHECK_INT(h_cell(t, 6, 0)->deco & VT_DECO_UL_MASK, 0);
    CHECK_INT(h_cell(t, 6, 0)->attr & VT_ATTR_UNDERLINE, 0);
    vt_free(t);
}

static void fonts_and_fraktur(void)
{
    vt_term *t = h_new(10, 1, VT_XTERM);
    h_put(t, "\033[11ma\033[19mb\033[20mc\033[23md\033[13me\033[10mf");
    CHECK_INT(vt_cell_font(t, h_cell(t, 0, 0)), 1);
    CHECK_INT(vt_cell_font(t, h_cell(t, 1, 0)), 9);
    CHECK_INT(vt_cell_font(t, h_cell(t, 2, 0)), 10);
    CHECK_INT(vt_cell_font(t, h_cell(t, 3, 0)), 0); /* 23: neither italic nor Fraktur */
    CHECK_INT(vt_cell_font(t, h_cell(t, 4, 0)), 3);
    CHECK_INT(vt_cell_font(t, h_cell(t, 5, 0)), 0);
    vt_free(t);
}

static void overline_frames_scripts_ideograms_blink(void)
{
    vt_term *t = h_new(12, 1, VT_XTERM);
    h_put(t, "\033[53;51ma\033[52mb\033[54;55mc\033[73md\033[74me\033[75mf"
             "\033[62mg\033[64mh\033[65mi\033[5mj\033[6mk\033[25ml");
    CHECK_INT(h_cell(t, 0, 0)->attr & (VT_ATTR_OVERLINE | VT_ATTR_FRAMED), VT_ATTR_OVERLINE | VT_ATTR_FRAMED);
    CHECK_INT(h_cell(t, 1, 0)->attr & (VT_ATTR_FRAMED | VT_ATTR_ENCIRCLED), VT_ATTR_ENCIRCLED);
    CHECK_INT(h_cell(t, 2, 0)->attr & (VT_ATTR_OVERLINE | VT_ATTR_FRAMED | VT_ATTR_ENCIRCLED), 0);
    CHECK_INT(h_cell(t, 3, 0)->attr & (VT_ATTR_SUPER | VT_ATTR_SUB), VT_ATTR_SUPER);
    CHECK_INT(h_cell(t, 4, 0)->attr & (VT_ATTR_SUPER | VT_ATTR_SUB), VT_ATTR_SUB);
    CHECK_INT(h_cell(t, 5, 0)->attr & (VT_ATTR_SUPER | VT_ATTR_SUB), 0);
    CHECK_INT((h_cell(t, 6, 0)->deco & VT_DECO_IDEO_MASK) >> VT_DECO_IDEO_SHIFT, VT_IDEO_OVERLINE);
    CHECK_INT((h_cell(t, 7, 0)->deco & VT_DECO_IDEO_MASK) >> VT_DECO_IDEO_SHIFT, VT_IDEO_STRESS);
    CHECK_INT(h_cell(t, 8, 0)->deco & VT_DECO_IDEO_MASK, 0);
    CHECK_INT(h_cell(t, 9, 0)->attr & (VT_ATTR_BLINK | VT_ATTR_RAPID), VT_ATTR_BLINK);
    CHECK_INT(h_cell(t, 10, 0)->attr & (VT_ATTR_BLINK | VT_ATTR_RAPID), VT_ATTR_BLINK | VT_ATTR_RAPID);
    CHECK_INT(h_cell(t, 11, 0)->attr & (VT_ATTR_BLINK | VT_ATTR_RAPID), 0);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* More distinct underline colours than the table holds: entries the grid
 * no longer shows are swept, so the newest colour is always kept. */
static void rare_style_table_is_swept_when_full(void)
{
    vt_term *t = h_new(4, 1, VT_XTERM);
    char b[32];
    int i;
    for (i = 0; i < 600; i++) {
        /* ESC [ 58 ; 2 ; r ; g ; 0 m A, r and g as three digits */
        int r = i & 255, g = i >> 8;
        memcpy(b, "\r\033[58;2;000;000;0mA", 20);
        b[8] = (char)('0' + r / 100); b[9] = (char)('0' + r / 10 % 10); b[10] = (char)('0' + r % 10);
        b[12] = (char)('0' + g / 100); b[13] = (char)('0' + g / 10 % 10); b[14] = (char)('0' + g % 10);
        h_put(t, b);
    }
    CHECK_INT(vt_cell_underline_color(t, h_cell(t, 0, 0)) == VT_RGB(599 & 255, 599 >> 8, 0), 1);
    vt_free(t);
}

/* ---- phase A4: the replies programs ask for ---- */

static void check_reply(const char *want)
{
    CHECK_INT(h_reply_len, (long)strlen(want));
    CHECK_INT(memcmp(h_reply, want, strlen(want)), 0);
    h_reply_clear();
}

/* 24-bit colours to the nearest xterm 256 index (for palette screens). */
static void rgb_to_256(void)
{
    CHECK_INT(vt_rgb_to_256(0x000000UL), 16);
    CHECK_INT(vt_rgb_to_256(0xFFFFFFUL), 231);
    CHECK_INT(vt_rgb_to_256(0xFF0000UL), 196);
    CHECK_INT(vt_rgb_to_256(0x5F87AFUL), 67);    /* exactly a cube colour */
    CHECK_INT(vt_rgb_to_256(0x808080UL), 244);   /* grey 128 is on the ramp */
    CHECK_INT(vt_rgb_to_256(0x0A0A0AUL), 232);   /* dark grey: ramp beats cube black */
    CHECK_INT(vt_rgb_to_256(0xEEEEEEUL), 255);
    CHECK_INT(vt_rgb_to_256(0x0B0700UL), 232);   /* near black, not quite grey */
    {
        /* every index maps back to itself: the palette's own colours */
        vt_term *t = h_new(2, 1, VT_XTERM);
        int i, same = 0;
        for (i = 16; i < 256; i++)
            same += vt_rgb_to_256(vt_palette_rgb(t, i)) == i;
        CHECK_INT(same, 240);
        vt_free(t);
    }
}

static void colour_queries_and_changes(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    vt_set_default_colors(t, 0xC0C0C0UL, 0x000000UL, 0xFFFFFFUL);
    h_reply_clear();
    h_put(t, "\033]4;1;?\007");
    check_reply("\033]4;1;rgb:cdcd/0000/0000\007");
    h_put(t, "\033]4;1;#123456;2;rgb:f/80/ff\033\\\033]4;1;?;2;?\033\\");
    check_reply("\033]4;1;rgb:1212/3434/5656\033\\\033]4;2;rgb:ffff/8080/ffff\033\\");
    CHECK_INT(vt_palette_rgb(t, 1) == 0x123456UL, 1);
    h_put(t, "\033]104;1\007");
    CHECK_INT(vt_palette_rgb(t, 1) == 0xCD0000UL, 1);
    CHECK_INT(vt_palette_rgb(t, 2) == 0xFF80FFUL, 1);
    h_put(t, "\033]104\007");
    CHECK_INT(vt_palette_rgb(t, 2) == 0x00CD00UL, 1);
    h_put(t, "\033]10;?;?\007");
    check_reply("\033]10;rgb:c0c0/c0c0/c0c0\007\033]11;rgb:0000/0000/0000\007");
    h_put(t, "\033]11;#203040\007");
    CHECK_INT(vt_default_color(t, 1) == 0x203040UL, 1);
    h_put(t, "\033]111\007");
    CHECK_INT(vt_default_color(t, 1) == 0x000000UL, 1);
    h_put(t, "\033[?996n");
    check_reply("\033[?997;1n");
    vt_free(t);
}

static void decrqss_decrqm_and_version(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_reply_clear();
    h_put(t, "\033[1;4:3;38;5;196;48;2;1;2;3m\033P$qm\033\\");
    check_reply("\033P1$r0;1;4:3;38;5;196;48;2;1;2;3m\033\\");
    h_put(t, "\033[2;5r\033P$qr\033\\");
    check_reply("\033P1$r2;5r\033\\");
    h_put(t, "\033P$qx\033\\");
    check_reply("\033P0$r\033\\");
    h_put(t, "\033[5 q\033P$q q\033\\");
    check_reply("\033P1$r5 q\033\\");
    CHECK_INT(vt_cursor_style(t), 5);
    h_put(t, "\033[?25$p\033[?25l\033[?25$p\033[?9999$p\033[4$p");
    check_reply("\033[?25;1$y\033[?25;2$y\033[?9999;0$y\033[4;2$y");
    h_put(t, "\033[>q");
    check_reply("\033P>|vtcon 1.0\033\\");
    h_put(t, "\033P+q544E;436F;5858\033\\"); /* XTGETTCAP TN, Co, XX */
    check_reply("\033P1+r544E=7674636F6E\033\\\033P1+r436F=323536\033\\\033P0+r5858\033\\");
    vt_free(t);
}

static void window_reports_and_title_stack(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_reply_clear();
    h_put(t, "\033[14t"); /* no pixel size known yet: no answer */
    CHECK_INT(h_reply_len, 0);
    vt_set_cell_pixels(t, 8, 16);
    h_put(t, "\033[14t\033[16t\033[18t\033[19t");
    check_reply("\033[4;384;640t\033[6;16;8t\033[8;24;80t\033[9;24;80t");
    h_put(t, "\033]2;one\007\033[22;0t\033]2;two\007");
    CHECK_INT(strcmp(vt_title(t), "two"), 0);
    h_put(t, "\033[23;0t");
    CHECK_INT(strcmp(vt_title(t), "one"), 0);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* ---- phase A5: modes ---- */

static void modes_of_phase_a5(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    int x, y;
    CHECK_INT((vt_modes(t) & VT_MODE_AUTOREPEAT) != 0, 1);  /* DECARM on at start */
    h_put(t, "\033[?8l\033[?12h");
    CHECK_INT((vt_modes(t) & VT_MODE_AUTOREPEAT) != 0, 0);
    CHECK_INT((vt_modes(t) & VT_MODE_CURSOR_BLINK) != 0, 1);
    h_put(t, "\033[2;1H\b");                                /* no ?45: stays */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 0);
    CHECK_INT(y, 1);
    h_put(t, "\033[?45h\b");                                /* ?45: up to the line end */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 9);
    CHECK_INT(y, 0);
    /* ESC % G: UTF-8 from here in a Latin-1 window */
    vt_set_charset(t, VT_CS_LATIN1);
    h_put(t, "\033[3;1H\033%G\xc3\xa9");
    CHECK_INT(h_cell(t, 0, 2)->ch, 0xE9);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

static void deccolm_only_when_allowed(void)
{
    vt_term *t = h_new(80, 3, VT_XTERM);
    h_layout_which = 0;
    h_put(t, "x\033[?3h");
    CHECK_INT(h_layout_which, 0);                           /* ?40 off: the window keeps its width */
    CHECK_INT(h_cell(t, 0, 0)->ch, 'x');
    h_put(t, "\033[?40h\033[?3h");
    CHECK_INT(h_layout_which, VT_LAYOUT_COLUMNS);
    CHECK_INT(h_layout_value, 132);
    CHECK_INT(h_cell(t, 0, 0)->ch, ' ');                    /* and the screen cleared */
    vt_free(t);
}

static void scheme_updates_when_asked(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    vt_set_default_colors(t, 0xC0C0C0UL, 0x000000UL, 0xC0C0C0UL);
    h_reply_clear();
    vt_set_default_colors(t, 0x000000UL, 0xFFFFFFUL, 0x000000UL);
    CHECK_INT(h_reply_len, 0);                              /* not asked: nothing */
    h_put(t, "\033[?2031h");
    vt_set_default_colors(t, 0xC0C0C0UL, 0x000000UL, 0xC0C0C0UL);
    CHECK_STR(h_reply, "\033[?997;1n");
    vt_free(t);
}

/* ---- phase A3: DEC double-width and double-height lines ---- */

static void double_width_lines_hold_half_the_columns(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    int x, y;
    h_put(t, "\033#6abcdefg");             /* 5 columns: wraps after e */
    CHECK_INT(vt_row_size(t, 0), VT_LINE_DOUBLE_WIDTH);
    CHECK_STR(h_row(t, 0), "abcde");
    CHECK_STR(h_row(t, 1), "fg");
    h_put(t, "\033[1;9H");                 /* CUP past the half: clamped */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 4);
    h_put(t, "\033[2;1H\033#3\033[3;1H\033#4");
    CHECK_INT(vt_row_size(t, 1), VT_LINE_DOUBLE_TOP);
    CHECK_INT(vt_row_size(t, 2), VT_LINE_DOUBLE_BOTTOM);
    h_put(t, "\033[3;1H\n");               /* a scroll takes the sizes along */
    CHECK_INT(vt_row_size(t, 0), VT_LINE_DOUBLE_TOP);
    CHECK_INT(vt_row_size(t, 1), VT_LINE_DOUBLE_BOTTOM);
    CHECK_INT(vt_row_size(t, 2), 0);
    h_put(t, "\033[1;1H\033#5");
    CHECK_INT(vt_row_size(t, 0), 0);
    h_put(t, "\033[2;1H\033[2J");          /* ED: single size again */
    CHECK_INT(vt_row_size(t, 1), 0);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

static void sgr_colours_and_attributes(void)
{
    vt_term *t = h_new(10, 1, VT_XTERM);
    vt_color f, b;
    h_put(t, "\033[1;31;44ma\033[22;39mb\033[38;5;200;48;2;255;0;0mc\033[38:2::0:255:0md");
    CHECK_INT(h_cell(t, 0, 0)->fg, 1);
    CHECK_INT(h_cell(t, 0, 0)->bg, 4);
    CHECK(h_cell(t, 0, 0)->attr & VT_ATTR_BOLD);
    CHECK_INT(h_cell(t, 1, 0)->attr & VT_ATTR_BOLD, 0);
    CHECK_INT(h_cell(t, 1, 0)->fg, VT_COLOR_DEFAULT);
    CHECK_INT(h_cell(t, 2, 0)->fg, 200);
    CHECK_INT(h_cell(t, 2, 0)->bg, VT_RGB(255, 0, 0));
    CHECK_INT(h_cell(t, 3, 0)->fg, VT_RGB(0, 255, 0));
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, 9); /* bold red is bright red */
    h_put(t, "\033[0;7;32;41me\033[0;95;103mf");
    vt_resolve_colors(t, h_cell(t, 4, 0), &f, &b);
    CHECK_INT(f, 1);
    CHECK_INT(b, 2);
    CHECK_INT(h_cell(t, 5, 0)->fg, 13);
    CHECK_INT(h_cell(t, 5, 0)->bg, 11);
    vt_free(t);
}

static void alt_screen_1049_saves_and_restores(void)
{
    vt_term *t = h_new(5, 3, VT_XTERM);
    int x, y;
    h_put(t, "main\033[2;3H");
    h_put(t, "\033[?1049h");
    CHECK_STR(h_screen(t), "");
    CHECK(vt_modes(t) & VT_MODE_ALT_SCREEN);
    h_put(t, "\033[1;1Hvim");
    h_put(t, "\033[?1049l");
    CHECK_STR(h_screen(t), "main");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(y, 1);
    vt_free(t);
}

static void dec_graphics_draw_boxes(void)
{
    vt_term *t = h_new(5, 1, VT_XTERM);
    h_put(t, "\033(0lqk\033(Bq");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0x250C);
    CHECK_INT(h_cell(t, 1, 0)->ch, 0x2500);
    CHECK_INT(h_cell(t, 2, 0)->ch, 0x2510);
    CHECK_INT(h_cell(t, 3, 0)->ch, 'q');
    vt_free(t);
}

static void shift_out_selects_g1(void)
{
    vt_term *t = h_new(5, 1, VT_XTERM);
    h_put(t, "\033)0\016x\017x");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0x2502);
    CHECK_INT(h_cell(t, 1, 0)->ch, 'x');
    vt_free(t);
}

static void device_reports(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_put(t, "\033[5n");
    CHECK_STR(h_reply, "\033[0n");
    h_reply_clear();
    h_put(t, "\033[c");
    CHECK_STR(h_reply, "\033[?62;22c");
    h_reply_clear();
    h_put(t, "\033[>c");
    CHECK_STR(h_reply, "\033[>1;10;0c");
    h_reply_clear();
    h_put(t, "\033[18t");
    CHECK_STR(h_reply, "\033[8;24;80t");
    h_reply_clear();
    h_put(t, "\033[3;7H\033[6n");
    CHECK_STR(h_reply, "\033[3;7R");
    vt_free(t);
}

static void save_restore_cursor_keeps_attributes(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    h_put(t, "\033[31m\033[1;5H\0337\033[0m\033[2;1H\0338x");
    CHECK_INT(h_cell(t, 4, 0)->fg, 1);
    h_put(t, "\033[2;2H\033[s\033[1;1H\033[uy");
    CHECK_INT(h_cell(t, 1, 1)->ch, 'y');
    vt_free(t);
}

static void repeat_last_character(void)
{
    vt_term *t = h_new(10, 1, VT_XTERM);
    h_put(t, "-\033[4b");
    CHECK_STR(h_row(t, 0), "-----");
    vt_free(t);
}

/* The search the window's Cmd-F asks for: oldest line first, in the
 * scrollback as well as the grid, ASCII-case-insensitive, a match found on a
 * row that the terminal wrapped is the row it starts on. */
static void find_scans_scrollback_then_grid_oldest_first(void)
{
    vt_term *t = h_new(8, 2, VT_XTERM);
    /* two rows, so Alpha and bravo scroll off: sb -2 is Alpha, -1 is bravo,
     * the grid holds CHARLIE on row 0 and delta on row 1 */
    h_put(t, "Alpha\r\nbravo\r\nCHARLIE\r\ndelta");
    CHECK_STR(h_screen(t), "CHARLIE|delta");
    CHECK_INT(vt_scrollback_lines(t), 2);
    /* from before the oldest, the first hit is the oldest matching line */
    CHECK_INT(vt_find(t, "bravo", -3), -1);
    CHECK_INT(vt_find(t, "BRAVO", -3), -1);   /* ASCII case folds */
    CHECK_INT(vt_find(t, "alpha", -3), -2);   /* older than bravo: found first */
    /* then the grid, in reading order */
    CHECK_INT(vt_find(t, "charlie", -3), 0);
    CHECK_INT(vt_find(t, "delta", -3), 1);
    /* from a row the scan starts there and moves on, never back */
    CHECK_INT(vt_find(t, "bravo", -1), -1);
    CHECK_INT(vt_find(t, "delta", 0), 1);
    CHECK_INT(vt_find(t, "charlie", 1), VT_ROW_NONE); /* CHARLIE is row 0 */
    vt_free(t);
}

static void find_reports_what_is_there_to_find_and_nothing_else(void)
{
    vt_term *t = h_new(24, 3, VT_XTERM);
    h_put(t, "root@vtcon:~$ ls -l\r\ntotal 8");
    CHECK_INT(vt_find(t, "total", -1), 1);
    CHECK_INT(vt_find(t, "nowhere", -1), VT_ROW_NONE);
    CHECK_INT(vt_find(t, "", -1), VT_ROW_NONE);        /* an empty query finds nothing */
    CHECK_INT(vt_find(t, "LS -L", -1), 0);             /* whole-row substring */
    CHECK_INT(vt_find(t, "lsl", -1), VT_ROW_NONE);      /* contiguous, not fuzzy */
    CHECK_INT(vt_find(t, "vtcon", -1), 0);
    CHECK_INT(vt_find(t, "$", -1), 0);                 /* a one-byte query */
    /* the empty row at the bottom is searched too and holds nothing */
    CHECK_INT(vt_find(t, " ", 2), VT_ROW_NONE);
    vt_free(t);
}

static void find_takes_a_query_that_crosses_a_wrap(void)
{
    vt_term *t = h_new(10, 4, VT_XTERM);
    /* ten columns: "0123456789" fills row 0 and its wrap flag is set, so
     * "ABC" is the start of row 1 */
    h_put(t, "0123456789ABC\r\nthird");
    CHECK_STR(h_row(t, 0), "0123456789");
    CHECK_INT(vt_row_wrapped(t, 0), 1);
    /* one query, two rows: found, and reported on the row it starts in */
    CHECK_INT(vt_find(t, "6789ABC", -1), 0);
    CHECK_INT(vt_find(t, "23456789ABC", -1), 0);
    /* a query inside either row alone, too */
    CHECK_INT(vt_find(t, "34567", -1), 0);
    CHECK_INT(vt_find(t, "ABC", -1), 0);
    /* searching from row 1 cannot reach back into the wrapped row 0 */
    CHECK_INT(vt_find(t, "6789", 1), VT_ROW_NONE);
    /* the next line is still its own */
    CHECK_INT(vt_find(t, "third", -1), 2);
    vt_free(t);
}

static void find_reads_utf8_and_petscii_as_they_are(void)
{
    vt_term *t = h_new(8, 2, VT_XTERM);
    /* U+00E9 LATIN SMALL LETTER E WITH ACUTE: C3 A9 in UTF-8 */
    h_put(t, "caf\303\251 bar\r\nna\303\257ve");
    CHECK_INT(vt_find(t, "caf", -1), 0);
    CHECK_INT(vt_find(t, "caf\303\251", -1), 0);  /* the accented cell, exact bytes */
    CHECK_INT(vt_find(t, "CAF\303\251", -1), 0);  /* the ASCII part folds */
    CHECK_INT(vt_find(t, "na\303\257ve", -1), 1);
    /* a PETSCII-mode term holds bytes, not UTF-8: only ASCII folds there */
    vt_free(t);
}

static void linefeed_off_the_bottom_fills_scrollback(void)
{
    vt_term *t = h_new(5, 2, VT_XTERM);
    h_put(t, "1\r\n2\r\n3\r\n4");
    CHECK_STR(h_screen(t), "3|4");
    CHECK_INT(vt_scrollback_lines(t), 2);
    CHECK_STR(h_row(t, -1), "2");
    CHECK_STR(h_row(t, -2), "1");
    h_put(t, "\033[3J");
    CHECK_INT(vt_scrollback_lines(t), 0);
    vt_free(t);
}

static void scroll_calls_the_renderer_once_per_write(void)
{
    vt_term *t = h_new(5, 4, VT_XTERM);
    h_scroll_calls = 0;
    h_put(t, "1\r\n2\r\n3\r\n4\r\n5\r\n6"); /* three lines scroll off */
    CHECK_INT(h_scroll_calls, 1);            /* one blit of three rows */
    CHECK_STR(h_screen(t), "3|4|5|6");
    vt_free(t);
}

static void resize_keeps_cursor_row_visible(void)
{
    vt_term *t = h_new(10, 5, VT_XTERM);
    int x, y;
    h_put(t, "1\r\n2\r\n3\r\n4\r\n5");
    vt_resize(t, 8, 3);
    CHECK_STR(h_screen(t), "3|4|5");
    CHECK_INT(vt_scrollback_lines(t), 2);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);
    vt_resize(t, 12, 4);
    CHECK_STR(h_screen(t), "3|4|5");
    h_put(t, "\033[2;1Habcdefghijkl");
    CHECK_STR(h_row(t, 1), "abcdefghijkl");
    vt_free(t);
}

/* vt_set_reflow with xterm's deferred wrap: the cursor waiting at the
 * margin still waits after narrowing, and the next character wraps. */
static void reflow_keeps_the_deferred_wrap(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, "abcdefghij");
    vt_resize(t, 5, 3);
    CHECK_STR(h_screen(t), "abcde|fghij");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 4);
    h_put(t, "X");
    CHECK_STR(h_screen(t), "abcde|fghij|X");
    vt_free(t);
}

/* The alternate screen is resized plainly (xterm does not reflow it); the
 * primary one behind it reflows, its saved cursor with it. */
static void reflow_leaves_the_alternate_screen(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, "0123456789ab\033[?1049h\033[HABCDEFGHIJKL");
    vt_resize(t, 20, 3);
    CHECK_STR(h_screen(t), "ABCDEFGHIJ|KL");
    h_put(t, "\033[?1049l");
    CHECK_STR(h_screen(t), "0123456789ab");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 0);
    CHECK_INT(x, 12);
    vt_free(t);
}

static void resize_with_a_blank_bottom_drops_it(void)
{
    vt_term *t = h_new(10, 5, VT_XTERM);
    h_put(t, "top\033[1;1H");
    vt_resize(t, 10, 2);
    CHECK_STR(h_screen(t), "top");
    CHECK_INT(vt_scrollback_lines(t), 0);
    vt_free(t);
}

static void decaln_fills_with_e(void)
{
    vt_term *t = h_new(3, 2, VT_XTERM);
    h_put(t, "\033#8");
    CHECK_STR(h_screen(t), "EEE|EEE");
    vt_free(t);
}

static void soft_reset_restores_modes(void)
{
    vt_term *t = h_new(5, 5, VT_XTERM);
    h_put(t, "\033[?7l\033[?1h\033[2;3r\033[4h\033[!p");
    CHECK_INT(vt_modes(t) & VT_MODE_APP_CURSOR, 0);
    h_put(t, "\033[5;1Habcdefg");
    CHECK_STR(h_row(t, 3), "abcde"); /* autowrap back on, and the region is */
    CHECK_STR(h_row(t, 4), "fg");    /* full again, so the bottom row scrolls */
    vt_free(t);
}

static void mouse_and_paste_modes(void)
{
    vt_term *t = h_new(5, 5, VT_XTERM);
    h_put(t, "\033[?1000;1006;2004h");
    CHECK(vt_modes(t) & VT_MODE_MOUSE_NORMAL);
    CHECK(vt_modes(t) & VT_MODE_MOUSE_SGR);
    CHECK(vt_modes(t) & VT_MODE_BRACKET_PASTE);
    h_put(t, "\033[?1000l");
    CHECK_INT(vt_modes(t) & VT_MODE_MOUSE_NORMAL, 0);
    vt_free(t);
}

static void ris_resets_everything(void)
{
    vt_term *t = h_new(5, 2, VT_XTERM);
    h_put(t, "abc\033[31m\033[?25l\033c");
    CHECK_STR(h_screen(t), "");
    CHECK(vt_modes(t) & VT_MODE_CURSOR_VISIBLE);
    h_put(t, "x");
    CHECK_INT(h_cell(t, 0, 0)->fg, VT_COLOR_DEFAULT);
    vt_free(t);
}

static void bell_rings(void)
{
    vt_term *t = h_new(5, 2, VT_XTERM);
    h_put(t, "\007\007");
    CHECK_INT(h_bells, 2);
    vt_free(t);
}

static void c1_via_utf8_code_points(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    h_put(t, "ab\xc2\x9b" "1D" "X"); /* U+009B is CSI */
    CHECK_STR(h_row(t, 0), "aX");
    vt_free(t);
}

static void screen_reverse_video_inverts_every_cell(void)
{
    vt_term *t = h_new(5, 1, VT_XTERM);
    vt_color f, b;
    h_put(t, "a\033[7mb\033[?5h");
    CHECK(vt_modes(t) & VT_MODE_SCREEN_REVERSE);
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, VT_COLOR_DEFAULT_BG); /* the defaults swap */
    h_put(t, "\033[0;31;42m");
    h_put(t, "c");
    vt_resolve_colors(t, h_cell(t, 2, 0), &f, &b);
    CHECK_INT(f, 2); /* swapped by the screen */
    CHECK_INT(b, 1);
    vt_resolve_colors(t, h_cell(t, 1, 0), &f, &b);
    CHECK_INT(f, VT_COLOR_DEFAULT); /* inverse cell on an inverse screen: normal */
    CHECK_INT(b, VT_COLOR_DEFAULT_BG);
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, VT_COLOR_DEFAULT_BG); /* a plain cell on an inverse screen */
    CHECK_INT(b, VT_COLOR_DEFAULT);
    h_put(t, "\033[?5l");
    CHECK_INT(vt_modes(t) & VT_MODE_SCREEN_REVERSE, 0);
    vt_free(t);
}

static void latin1_mode_for_amiga_unix_ports(void)
{
    vt_term *t = h_new(10, 1, VT_XTERM);
    vt_u8 out[8];
    vt_set_charset(t, VT_CS_LATIN1);
    h_put(t, "\xe5\xe4\x9b" "1D" "X");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0xE5);
    CHECK_INT(h_cell(t, 1, 0)->ch, 'X'); /* 8-bit CSI moved back over the 0xE4 */
    CHECK_INT(vt_encode_key(t, 0xF6, 0, out), 1);
    CHECK_INT(out[0], 0xF6);
    vt_free(t);
}

static void eight_bit_csi_speaks_amiga_where_the_dialects_collide(void)
{
    vt_term *t = h_new(77, 23, VT_XTERM);
    int x, y;
    /* ixemul's window-size probe: 9B 20 71, answered Amiga style */
    h_put(t, "\x9b q");
    CHECK_STR(h_reply, "\x9b" "1;1;23;77 r");
    h_reply_clear();
    /* the same bytes after ESC [ are DECSCUSR: no reply */
    h_put(t, "\033[ q");
    CHECK_STR(h_reply, "");
    /* 8-bit CSI u sets the line length; ESC [ u restores the cursor */
    h_put(t, "\x9b" "40u");
    CHECK_INT(h_layout_which, VT_LAYOUT_LINE_LENGTH);
    CHECK_INT(h_layout_value, 40);
    h_put(t, "\033[2;3H\033[s\033[5;5H\033[u");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(y, 1);
    /* raw events through the 8-bit form */
    h_put(t, "\x9b" "12{");
    CHECK_INT(vt_raw_events(t), 1L << 12);
    /* shared sequences mean the same either way */
    h_put(t, "\x9b" "1;1H\x9b" "31mZ");
    CHECK_INT(h_cell(t, 0, 0)->ch, 'Z');
    CHECK_INT(h_cell(t, 0, 0)->fg, 1);
    /* UTF-8 still decodes around it */
    h_put(t, "\xc3\xa5");
    CHECK_INT(h_cell(t, 1, 0)->ch, 0xE5);
    vt_free(t);
}

static void cp437_charset_for_ibm_font_programs(void)
{
    vt_term *t = h_new(10, 1, VT_XTERM);
    vt_u8 out[8];
    vt_set_charset(t, VT_CS_CP437);
    h_put(t, "\xdb\xb0\x9b" "1D" "X");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0x2588);
    CHECK_INT(h_cell(t, 1, 0)->ch, 'X'); /* $9B is still the CSI */
    CHECK_INT(vt_encode_key(t, 0xE5, 0, out), 1);
    CHECK_INT(out[0], 0x86); /* a-ring in CP437 */
    vt_free(t);
}

static void onlcr_returns_on_linefeed_without_changing_return(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    vt_u8 out[8];
    vt_set_onlcr(t, 1);
    h_put(t, "ab\ncd");
    CHECK_STR(h_screen(t), "ab|cd");
    CHECK_INT(vt_encode_key(t, VT_KEY_RETURN, 0, out), 1); /* Return is still CR */
    vt_free(t);
}

static void copy_text_joins_wrapped_lines_and_trims_blanks(void)
{
    vt_term *t = h_new(8, 4, VT_XTERM);
    char buf[128];
    h_put(t, "one   \r\n0123456789\r\n\xc3\xa5x");
    /* rows: "one", "01234567" (wrapped), "89", "\xe5x" */
    vt_copy_text(t, 0, 0, 7, 3, buf, sizeof(buf));
    CHECK_STR(buf, "one\n0123456789\n\xc3\xa5x");
    vt_copy_text(t, 2, 1, 1, 2, buf, sizeof(buf)); /* inside, across the wrap */
    CHECK_STR(buf, "234567" "89");
    vt_copy_text(t, 1, 2, 2, 1, buf, sizeof(buf)); /* backwards selection */
    CHECK_STR(buf, "234567" "89"); /* the same range as forwards */
    CHECK_INT(vt_copy_text(t, 0, 0, 7, 3, buf, 5), 4); /* bounded */
    vt_free(t);
}

static void host_settings_palette_bold_cursor(void);

void suite_xterm(void)
{
    rgb_to_256();
    copy_text_joins_wrapped_lines_and_trims_blanks();
    onlcr_returns_on_linefeed_without_changing_return();
    cp437_charset_for_ibm_font_programs();
    eight_bit_csi_speaks_amiga_where_the_dialects_collide();
    latin1_mode_for_amiga_unix_ports();
    screen_reverse_video_inverts_every_cell();
    parser_splits_sequences_across_writes();
    c0_controls_execute_inside_csi();
    can_aborts_a_sequence();
    unknown_private_sequences_are_swallowed();
    osc_title_ends_on_bel_and_st();
    dcs_strings_are_ignored();
    utf8_decodes_to_cells();
    wide_glyphs_take_two_cells();
    autowrap_is_deferred();
    autowrap_off_overwrites_last_column();
    erase_uses_current_background();
    erase_display_does_not_home();
    erase_modes();
    scroll_region_confines_linefeed();
    reverse_index_at_top_scrolls_down();
    origin_mode_positions_relative_to_region();
    insert_and_delete_characters();
    insert_and_delete_lines();
    scroll_up_down_sequences();
    tabs_default_every_eight();
    back_tab();
    sgr_colours_and_attributes();
    rgb_colour_keeps_all_24_bits();
    underline_styles_and_colour();
    fonts_and_fraktur();
    overline_frames_scripts_ideograms_blink();
    rare_style_table_is_swept_when_full();
    colour_queries_and_changes();
    decrqss_decrqm_and_version();
    window_reports_and_title_stack();
    modes_of_phase_a5();
    deccolm_only_when_allowed();
    scheme_updates_when_asked();
    double_width_lines_hold_half_the_columns();
    alt_screen_1049_saves_and_restores();
    dec_graphics_draw_boxes();
    shift_out_selects_g1();
    device_reports();
    save_restore_cursor_keeps_attributes();
    repeat_last_character();
    find_scans_scrollback_then_grid_oldest_first();
    find_reports_what_is_there_to_find_and_nothing_else();
    find_takes_a_query_that_crosses_a_wrap();
    find_reads_utf8_and_petscii_as_they_are();
    linefeed_off_the_bottom_fills_scrollback();
    scroll_calls_the_renderer_once_per_write();
    resize_keeps_cursor_row_visible();
    resize_with_a_blank_bottom_drops_it();
    reflow_keeps_the_deferred_wrap();
    reflow_leaves_the_alternate_screen();
    decaln_fills_with_e();
    soft_reset_restores_modes();
    mouse_and_paste_modes();
    ris_resets_everything();
    bell_rings();
    c1_via_utf8_code_points();
    host_settings_palette_bold_cursor();
}

/* The host-side (profile) settings: palette entries, bold-as-bright, the
 * DECSCUSR and blink defaults, ?5 from the host. */
static void host_settings_palette_bold_cursor(void)
{
    vt_term *t;
    const vt_cell *c;
    vt_color f, b;

    t = h_new(10, 2, VT_XTERM);

    /* The palette starts as xterm's table; the host remaps entries. */
    CHECK_INT(vt_palette_rgb(t, 1), 0xCD0000);
    vt_set_palette(t, 1, 0x00CD00);
    CHECK_INT(vt_palette_rgb(t, 1), 0x00CD00);
    vt_set_palette(t, 255, 0x808080);
    CHECK_INT(vt_palette_rgb(t, 255), 0x808080);
    vt_set_palette(t, 256, 0);
    vt_set_palette(t, -1, 0);
    CHECK_INT(vt_palette_rgb(t, 255), 0x808080);
    vt_clear_palette(t, 1);                       /* a theme without entry 1 */
    CHECK_INT(vt_palette_rgb(t, 1), 0xCD0000);
    vt_clear_palette(t, 300);
    CHECK_INT(vt_palette_rgb(t, 255), 0x808080);

    /* Bold-as-bright: SGR 1 over 0-7 takes 8-15, xterm's way of drawing;
     * the profile option turns only the colour shift off. */
    h_put(t, "\033[31mA\033[1;31mB");
    c = h_cell(t, 0, 0);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 1);
    c = h_cell(t, 1, 0);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 9);
    vt_set_bold_bright(t, 0);
    c = h_cell(t, 1, 0);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 1);
    vt_set_bold_bright(t, 1);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 9);

    /* DECSCUSR and blink defaults; the programs' sequences still override. */
    CHECK_INT(vt_cursor_style(t), 0);
    vt_set_cursor_style(t, 6);
    CHECK_INT(vt_cursor_style(t), 6);
    vt_set_cursor_style(t, 9);
    CHECK_INT(vt_cursor_style(t), 0);
    CHECK(!(vt_modes(t) & VT_MODE_CURSOR_BLINK));
    vt_set_cursor_blink(t, 1);
    CHECK(vt_modes(t) & VT_MODE_CURSOR_BLINK);
    h_put(t, "\033[?12l\033[2 q");
    CHECK(!(vt_modes(t) & VT_MODE_CURSOR_BLINK));
    CHECK_INT(vt_cursor_style(t), 2);

    /* ?5 from the host inverts, as from the program. */
    h_put(t, "\033[H\033[0;31;44mC");
    c = h_cell(t, 0, 0);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 1);
    CHECK_INT(b, 4);
    vt_screen_reverse(t, 1);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 4);
    CHECK_INT(b, 1);
    vt_screen_reverse(t, 0);
    vt_resolve_colors(t, c, &f, &b);
    CHECK_INT(f, 1);
    CHECK_INT(b, 4);

    vt_free(t);
}
