/* The xterm personality's protocol (G3, plan 2026-10-04-gaps-g3-protocol):
 * reports, resets, OSC strings, the kitty keyboard protocol, the VT420
 * editing extras. */
#include "harness.h"
#include <stdlib.h>
#include "../render/synchold.h"

static void reply_is(const char *want, int line)
{
    h_checks++;
    if (h_reply_len != (int)strlen(want) || memcmp(h_reply, want, strlen(want))) {
        h_failures++;
        printf("  FAIL tests/test_protocol.c:%d: reply [", line);
        fwrite(h_reply, 1, (size_t)h_reply_len, stdout);
        printf("] want [%s]\n", want);
    }
    h_reply_clear();
}
#define REPLY(w) reply_is((w), __LINE__)

/* a + the decimal of v + b, into out (C89 has no snprintf) */
static char *fmt3(char *out, const char *a, long v, const char *b)
{
    char d[12];
    int k = 0;
    strcpy(out, a);
    if (!v)
        d[k++] = '0';
    while (v) {
        d[k++] = (char)('0' + v % 10);
        v /= 10;
    }
    out += strlen(out);
    while (k)
        *out++ = d[--k];
    strcpy(out, b);
    return out;
}

/* the reply so far ends with s */
static int ends_with(const char *s)
{
    int n = (int)strlen(s);
    return h_reply_len >= n && !memcmp(h_reply + h_reply_len - n, s, (size_t)n);
}

/* a mode query for DEC mode m after setting (on) or resetting it */
static void ask_mode(vt_term *t, int m, int on)
{
    char q[48], *e;
    e = fmt3(q, "\033[?", m, on ? "h" : "l");
    fmt3(e + 1, "\033[?", m, "$p");
    h_put(t, q);
}

/* ---- G3-01: DECRQSS and DECRQM say what the terminal is ---- */

/* DA1 says VT220 (62); DECSCL must not claim a VT420 the engine is not. */
static void decrqss_conformance_level_matches_da1(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_put(t, "\033[c");
    REPLY("\033[?62;22c");
    h_put(t, "\033P$q\"p\033\\");
    REPLY("\033P1$r62;1\"p\033\\");
    vt_free(t);
}

/* DECRQM answers every mode set_mode keeps: ?66 (DECNKM) and ?1048 were
 * answered "not recognised" though both are settable. */
static void decrqm_answers_every_mode_the_engine_keeps(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_put(t, "\033[?66$p");
    REPLY("\033[?66;2$y");
    h_put(t, "\033[?66h\033[?66$p");
    REPLY("\033[?66;1$y");
    h_put(t, "\033>\033[?66$p");              /* DECKPNM is the same switch */
    REPLY("\033[?66;2$y");
    h_put(t, "\033[?1048$p");
    REPLY("\033[?1048;2$y");
    h_put(t, "\033[?1048h\033[?1048$p");       /* a cursor saved */
    REPLY("\033[?1048;1$y");
    h_put(t, "\033[?4$p");                     /* DECSCLM: accepted, never smooth */
    REPLY("\033[?4;4$y");
    {
        /* every DEC mode set_mode takes: set, asked, reset, asked */
        static const int modes[] = { 1, 5, 47, 6, 7, 8, 9, 12, 25, 40, 45, 66, 1000, 1002, 1003,
                                     1004, 1005, 1006, 1015, 1016, 1034, 1047, 1049, 2004, 2026, 2031, 2048, 7727 };
        int i, bad = 0;
        for (i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i++) {
            char want[64];
            ask_mode(t, modes[i], 1);
            fmt3(want, "\033[?", modes[i], ";1$y");
            if (!ends_with(want)) { /* ?2048 also reports the size first */
                printf("    mode %d set: [%.*s]\n", modes[i], h_reply_len, h_reply);
                bad++;
            }
            h_reply_clear();
            ask_mode(t, modes[i], 0);
            fmt3(want, "\033[?", modes[i], ";2$y");
            if (!ends_with(want)) {
                printf("    mode %d reset: [%.*s]\n", modes[i], h_reply_len, h_reply);
                bad++;
            }
            h_reply_clear();
        }
        CHECK_INT(bad, 0);
    }
    vt_free(t);
}

/* DECRQSS m gives back the SGR that makes the current rendition, the
 * underline colour and the font included. */
static void decrqss_sgr_round_trips_underline_colour_and_font(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_put(t, "\033[0;4:3;58;2;1;2;3;12;53;73m\033P$qm\033\\");
    REPLY("\033P1$r0;4:3;53;73;12;58;2;1;2;3m\033\\");
    h_put(t, "\033[0;58;5;9;20m\033P$qm\033\\");
    REPLY("\033P1$r0;20;58;5;9m\033\\");
    vt_free(t);
}

/* ---- G3-02: RIS resets what a program set ---- */

/* ESC c left the modifyOtherKeys level, the title stack and the cursor
 * shape of the program before it; the cursor goes back to the host's
 * (profile) shape, not to the engine's. */
static void ris_resets_keys_title_stack_and_cursor_shape(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    vt_set_cursor_style(t, 2);                      /* the profile's steady block */
    h_put(t, "\033[>4;2m\033]2;one\007\033[22;0t\033[5 q");
    CHECK_INT(vt_cursor_style(t), 5);
    h_put(t, "\033c");
    h_put(t, "\033[?4m");
    REPLY("\033[>4;0m");                            /* modifyOtherKeys off */
    CHECK_STR(vt_title(t), "");
    h_put(t, "\033[23;0t");                         /* nothing pushed any more */
    CHECK_STR(vt_title(t), "");
    CHECK_INT(vt_cursor_style(t), 2);
    vt_free(t);
}

/* ---- G3-03: a ?2026 frame is waited for as long as foot and tmux wait ---- */

/* The hold was 3 frames (150 ms): a 68k program's full redraw is longer,
 * and the window showed it half done. Now 1 s, then drawn anyway. */
static void sync_frame_is_held_up_to_one_second(void)
{
    int held = 0;
    while (VTWIN_SYNC_HOLD(held) && held < 1000)
        held++;
    CHECK_INT((long)held * VTWIN_FRAME_MICROS, 1000000L);
    CHECK(VTWIN_SYNC_HOLD(3));                     /* 150 ms: still waiting */
    CHECK(!VTWIN_SYNC_HOLD(20));                   /* 1 s: drawn */
}

/* ---- G3-05: DA3, the locking shifts, media copy ---- */

/* DA3 (CSI = c) is answered with DECRPTUI, a unit id of zeros, as xterm. */
static void da3_reports_a_unit_id(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033[=c");
    REPLY("\033P!|00000000\033\\");
    h_put(t, "\033[=0c");
    REPLY("\033P!|00000000\033\\");
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* LS2 / LS3 (ESC n / ESC o) invoke G2 / G3 into GL; LS1R-LS3R (ESC ~ } |)
 * into GR, which an 8-bit (Latin-1) window then draws through. */
static void locking_shifts_invoke_g2_and_g3(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033*0\033+A\033nq\033o#\033(B\017q");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0x2500);     /* G2 DEC graphics: q is a line */
    CHECK_INT(h_cell(t, 1, 0)->ch, 0xA3);       /* G3 UK: # is a pound sign */
    CHECK_INT(h_cell(t, 2, 0)->ch, 'q');        /* SI: G0 again */
    vt_set_charset(t, VT_CS_LATIN1);
    h_put(t, "\033[2;1H\033)0\033~\xf1\033|\xf1\033}\xf1");
    CHECK_INT(h_cell(t, 0, 1)->ch, 0x2500);     /* GR = G1 (DEC graphics): F1 is q's line */
    CHECK_INT(h_cell(t, 1, 1)->ch, 'q');        /* GR = G3 (UK): F1 is its q */
    CHECK_INT(h_cell(t, 2, 1)->ch, 0x2500);     /* GR = G2 (DEC graphics, set above) */
    h_put(t, "\033c\033[3;1H\xf1");
    CHECK_INT(h_cell(t, 0, 2)->ch, 0xF1);       /* RIS: GR back to Latin-1 */
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* Media copy (xterm-256color's mc0 mc4 mc5): there is no printer, so
 * printer controller mode (CSI 5 i) takes what follows off the screen
 * until CSI 4 i, as a VT102 does; CSI i (print screen) does nothing. */
static void printer_controller_mode_keeps_text_off_the_screen(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "a\033[ib\033[5isecret\033[1mstill\033[4");
    h_put(t, "ic");
    CHECK_STR(h_screen(t), "abc");
    CHECK_INT(h_cell(t, 2, 0)->attr, 0);        /* the SGR inside went to the printer */
    h_put(t, "\033[5ix\x9b" "4id");             /* the 8-bit CSI ends it too */
    CHECK_STR(h_screen(t), "abcd");
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* ---- G3-06 / G3-07: VT420 column and rectangle editing ---- */

static void fill_rows(vt_term *t)
{
    h_put(t, "\033[H\033[2J\033[1;1Habcdef\033[2;1Hghijkl\033[3;1Hmnopqr\033[4;1Hstuvwx");
}

/* DECIC / DECDC insert and delete columns at the cursor, in the rows of
 * the scroll region only; the cursor stays. */
static void decic_and_decdc_move_columns_in_the_region(void)
{
    vt_term *t = h_new(6, 4, VT_XTERM);
    int x, y;
    fill_rows(t);
    h_put(t, "\033[2;3r\033[2;3H\033[2'}");
    CHECK_STR(h_screen(t), "abcdef|gh  ij|mn  op|stuvwx");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(y, 1);
    h_put(t, "\033[1;2H\033[3'~");                 /* outside the region: nothing */
    CHECK_STR(h_screen(t), "abcdef|gh  ij|mn  op|stuvwx");
    h_put(t, "\033[3;2H\033[3'~");
    CHECK_STR(h_screen(t), "abcdef|gij|mop|stuvwx");
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* DECFRA fills, DECERA / DECSERA erase, DECCRA copies a rectangle;
 * DECCARA / DECRARA set and reverse attributes in it (or, by DECSACE,
 * in the stream of cells from its first to its last). */
static void rectangle_fill_erase_copy_and_attributes(void)
{
    vt_term *t = h_new(6, 4, VT_XTERM);
    fill_rows(t);
    h_put(t, "\033[31m\033[88;2;2;3;4$x");         /* X into rows 2-3, columns 2-4 */
    CHECK_STR(h_screen(t), "abcdef|gXXXkl|mXXXqr|stuvwx");
    CHECK_INT(h_cell(t, 1, 1)->fg, 1);             /* in the current rendition */
    h_put(t, "\033[0m\033[1;5;2;6$z");             /* DECERA: rows 1-2, columns 5-6 */
    CHECK_STR(h_screen(t), "abcd|gXXX|mXXXqr|stuvwx");
    h_put(t, "\033[3;5;3;5${");                    /* DECSERA: no protected cells */
    CHECK_STR(h_screen(t), "abcd|gXXX|mXXX r|stuvwx");
    h_put(t, "\033[1;1;2;2;1;3;5;1$v");            /* DECCRA: ab/gX to row 3 col 5 */
    CHECK_STR(h_screen(t), "abcd|gXXX|mXXXab|stuvgX");
    h_put(t, "\033[?6h\033[2;3r\033[1;1;1;2;1;2;1;1$v\033[?6l\033[r"); /* origin: region-relative */
    CHECK_STR(h_screen(t), "abcd|gXXX|gXXXab|stuvgX");
    h_put(t, "\033[2*x\033[1;2;2;3;1;7$r");        /* rectangle: bold + inverse */
    CHECK_INT(h_cell(t, 1, 0)->attr, VT_ATTR_BOLD | VT_ATTR_INVERSE);
    CHECK_INT(h_cell(t, 2, 1)->attr, VT_ATTR_BOLD | VT_ATTR_INVERSE);
    CHECK_INT(h_cell(t, 3, 0)->attr, 0);
    CHECK_INT(h_cell(t, 0, 1)->attr, 0);
    h_put(t, "\033[1;2;2;3;7$t");                  /* DECRARA: inverse back off */
    CHECK_INT(h_cell(t, 1, 0)->attr, VT_ATTR_BOLD);
    h_put(t, "\033[0*x\033[1;5;2;2;4$r");          /* stream: 1,5 .. 2,2 */
    CHECK_INT(h_cell(t, 4, 0)->attr & VT_ATTR_UNDERLINE, VT_ATTR_UNDERLINE);
    CHECK_INT(h_cell(t, 5, 0)->attr & VT_ATTR_UNDERLINE, VT_ATTR_UNDERLINE);
    CHECK_INT(h_cell(t, 0, 1)->attr & VT_ATTR_UNDERLINE, VT_ATTR_UNDERLINE);
    CHECK_INT(h_cell(t, 1, 1)->attr & VT_ATTR_UNDERLINE, VT_ATTR_UNDERLINE);
    CHECK_INT(h_cell(t, 2, 1)->attr & VT_ATTR_UNDERLINE, 0);
    CHECK_INT(h_cell(t, 3, 0)->attr & VT_ATTR_UNDERLINE, 0);
    h_put(t, "\033[1;1;4;6;0$r");                  /* 0: every attribute off */
    CHECK_INT(h_cell(t, 1, 0)->attr, 0);
    h_put(t, "\033[7;1;1;1;1$x");                  /* a control is no fill character */
    CHECK_INT(h_cell(t, 0, 0)->ch, 'a');
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* ---- G3-08: the urxvt and SGR-pixel mouse encodings ---- */

static const char *mouse(vt_term *t, int button, int kind, int x, int y, int px, int py)
{
    static char b[64];
    int n = vt_encode_mouse_px(t, button, kind, x, y, px, py, 0, (vt_u8 *)b);
    b[n] = 0;
    return b;
}

/* ?1015: CSI Cb;Cx;Cy M in decimal (no 223 limit); ?1016: the SGR form
 * with pixel coordinates. SGR (?1006) beats urxvt, SGR-pixel beats SGR. */
static void urxvt_and_sgr_pixel_mouse_reports(void)
{
    vt_term *t = h_new(400, 300, VT_XTERM);
    vt_set_cell_pixels(t, 8, 16);
    h_put(t, "\033[?1000;1015h");
    CHECK_STR(mouse(t, 0, 0, 299, 9, 2395, 150), "\033[32;300;10M");
    CHECK_STR(mouse(t, 0, 1, 299, 9, 2395, 150), "\033[35;300;10M");   /* release: button 3 */
    h_put(t, "\033[?1006h");
    CHECK_STR(mouse(t, 0, 1, 299, 9, 2395, 150), "\033[<0;300;10m");
    h_put(t, "\033[?1016h");
    CHECK_STR(mouse(t, 2, 0, 299, 9, 2395, 150), "\033[<2;2396;151M"); /* pixels from 1 */
    CHECK_STR(mouse(t, 64, 0, 299, 9, 2395, 150), "\033[<64;2396;151M");
    /* without pixels from the host (vt_encode_mouse) the cell's corner */
    {
        char b[64];
        int n = vt_encode_mouse(t, 0, 0, 2, 1, 0, (vt_u8 *)b);
        b[n] = 0;
        CHECK_STR(b, "\033[<0;17;17M");
    }
    h_put(t, "\033[?1016l\033[?1006l\033[?1015l");
    CHECK_STR(mouse(t, 0, 0, 299, 9, 0, 0), "");                       /* X10 cannot say 300 */
    h_put(t, "\033[?1015$p\033[?1016$p");
    REPLY("\033[?1015;2$y\033[?1016;2$y");
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* ---- G3-09: in-band resize reports ---- */

/* ?2048: CSI 48;rows;cols;height px;width px t once on setting the mode,
 * then at every resize, as kitty and foot send it (neovim asks for it). */
static void in_band_resize_reports_follow_the_mode(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    vt_set_cell_pixels(t, 8, 16);
    vt_resize(t, 100, 30);
    CHECK_INT(h_reply_len, 0);                     /* not asked: nothing */
    h_put(t, "\033[?2048h");
    REPLY("\033[48;30;100;480;800t");
    vt_resize(t, 90, 25);
    REPLY("\033[48;25;90;400;720t");
    vt_resize(t, 90, 25);                          /* the same size: no report */
    CHECK_INT(h_reply_len, 0);
    h_put(t, "\033[?2048$p");
    REPLY("\033[?2048;1$y");
    h_put(t, "\033[?2048l");
    vt_resize(t, 80, 24);
    CHECK_INT(h_reply_len, 0);
    vt_free(t);
}

void suite_protocol(void)
{
    in_band_resize_reports_follow_the_mode();
    urxvt_and_sgr_pixel_mouse_reports();
    decic_and_decdc_move_columns_in_the_region();
    rectangle_fill_erase_copy_and_attributes();
    da3_reports_a_unit_id();
    locking_shifts_invoke_g2_and_g3();
    printer_controller_mode_keeps_text_off_the_screen();
    sync_frame_is_held_up_to_one_second();
    decrqss_conformance_level_matches_da1();
    decrqm_answers_every_mode_the_engine_keeps();
    decrqss_sgr_round_trips_underline_colour_and_font();
    ris_resets_keys_title_stack_and_cursor_shape();
}
