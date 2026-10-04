/* The xterm personality's protocol (G3, plan 2026-10-04-gaps-g3-protocol):
 * reports, resets, OSC strings, the kitty keyboard protocol, the VT420
 * editing extras. */
#include "harness.h"
#include <stdlib.h>
#include "../render/synchold.h"
#include "../handler/clipfmt.h"
#include "../config/termurl.h"

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
    REPLY("\033[?62;4;22c");                     /* 4: sixel (G4) */
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
 * and the window showed it half done. Now 1 s, then drawn anyway -- 1 s of
 * the frame clock's waits, whatever its pace (pace.h: 20 to 160 ms). */
static void sync_frame_is_held_up_to_one_second(void)
{
    static const long step[2] = { 20000L, 160000L }; /* pace.h's shortest and longest frame */
    int i;
    for (i = 0; i < 2; i++) {
        long held = 0;
        while (VTWIN_SYNC_HOLD(held))
            held += step[i];
        CHECK(held >= 1000000L && held < 1000000L + step[i]);
    }
    CHECK(VTWIN_SYNC_HOLD(150000L));               /* 150 ms: still waiting */
    CHECK(!VTWIN_SYNC_HOLD(1000000L));             /* 1 s: drawn */
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

/* ---- G3-11: the kitty keyboard protocol ---- */

static const char *kkey(vt_term *t, long key, int mods)
{
    static char b[80];
    int n = vt_encode_key(t, key, mods, (vt_u8 *)b);
    b[n] = 0;
    return b;
}

static const char *kev(vt_term *t, long key, int mods, int event, long shifted, long base, long text)
{
    static char b[80];
    int n = vt_encode_key_kitty(t, key, mods, event, shifted, base, text, (vt_u8 *)b);
    b[n] = 0;
    return b;
}

/* CSI ? u asks, CSI > f u pushes, CSI < n u pops, CSI = f ; m u sets (1),
 * ors (2), clears (3); the main and alternate screens keep stacks of their
 * own; RIS empties both. */
static void kitty_keyboard_flags_stack_per_screen(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033[?u");
    REPLY("\033[?0u");
    h_put(t, "\033[>1u\033[?u");
    REPLY("\033[?1u");
    h_put(t, "\033[>5u\033[?u");
    REPLY("\033[?5u");
    h_put(t, "\033[=2;2u\033[?u");                 /* or */
    REPLY("\033[?7u");
    h_put(t, "\033[=4;3u\033[?u");                 /* and not */
    REPLY("\033[?3u");
    h_put(t, "\033[=24u\033[?u");                  /* set (mode 1 by default) */
    REPLY("\033[?24u");
    h_put(t, "\033[<u\033[?u");                    /* pop one: the 1 below */
    REPLY("\033[?1u");
    h_put(t, "\033[?1049h\033[?u");                /* the alternate screen's own */
    REPLY("\033[?0u");
    h_put(t, "\033[>8u\033[?1049l\033[?u");
    REPLY("\033[?1u");
    h_put(t, "\033[?1049h\033[?u\033[?1049l");
    REPLY("\033[?8u");
    h_put(t, "\033[<9u\033[?u");                   /* popping past the bottom: 0 */
    REPLY("\033[?0u");
    h_put(t, "\033[>1u\033[>1u\033[>1u\033[>1u\033[>1u\033[>1u\033[>1u\033[>1u\033[>2u\033[?u");
    REPLY("\033[?2u");                             /* a full stack drops its oldest */
    h_put(t, "\033c\033[?u");
    REPLY("\033[?0u");
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* Flag 1, disambiguate: Esc, Ctrl/Alt combinations and modified Return,
 * Tab, Backspace as CSI u; plain text, Shift+text, plain Return/Tab/
 * Backspace as before; cursor and F keys always CSI, F3 as CSI 13 ~. */
static void kitty_disambiguate_flag(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033[?1h\033[>1u");                  /* DECCKM does not matter */
    CHECK_STR(kkey(t, VT_KEY_ESCAPE, 0), "\033[27u");
    CHECK_STR(kkey(t, 1, VT_MOD_CTRL), "\033[97;5u");      /* the keymap's ^A */
    CHECK_STR(kkey(t, 'a', VT_MOD_CTRL), "\033[97;5u");
    CHECK_STR(kkey(t, 'a', VT_MOD_ALT), "\033[97;3u");
    CHECK_STR(kkey(t, 'A', VT_MOD_SHIFT | VT_MOD_ALT), "\033[97;4u");
    CHECK_STR(kkey(t, 'A', VT_MOD_SHIFT), "A");
    CHECK_STR(kkey(t, 'a', 0), "a");
    CHECK_STR(kkey(t, 0xE9, 0), "\xc3\xa9");
    CHECK_STR(kkey(t, VT_KEY_RETURN, 0), "\r");
    CHECK_STR(kkey(t, VT_KEY_RETURN, VT_MOD_CTRL), "\033[13;5u");
    CHECK_STR(kkey(t, VT_KEY_TAB, VT_MOD_SHIFT), "\033[9;2u");
    CHECK_STR(kkey(t, VT_KEY_BACKSPACE, 0), "\177");
    CHECK_STR(kkey(t, VT_KEY_BACKSPACE, VT_MOD_ALT), "\033[127;3u");
    CHECK_STR(kkey(t, VT_KEY_UP, 0), "\033[A");
    CHECK_STR(kkey(t, VT_KEY_UP, VT_MOD_CTRL), "\033[1;5A");
    CHECK_STR(kkey(t, VT_KEY_F1, 0), "\033[P");
    CHECK_STR(kkey(t, VT_KEY_F3, 0), "\033[13~");
    CHECK_STR(kkey(t, VT_KEY_F3, VT_MOD_SHIFT), "\033[13;2~");
    CHECK_STR(kkey(t, VT_KEY_F5, 0), "\033[15~");
    CHECK_STR(kkey(t, VT_KEY_DELETE, VT_MOD_CTRL), "\033[3;5~");
    CHECK_STR(kkey(t, VT_KEY_KP_ENTER, 0), "\033[57414u");
    CHECK_STR(kkey(t, VT_KEY_KP_5, 0), "5");
    h_put(t, "\033[<u");                           /* back to the legacy keys */
    CHECK_STR(kkey(t, VT_KEY_ESCAPE, 0), "\033");
    CHECK_STR(kkey(t, VT_KEY_UP, 0), "\033OA");
    vt_free(t);
}

/* Flag 2 reports repeats and releases (the host passes the event); flag 4
 * adds the shifted and the base-layout key; flag 8 makes every key an
 * escape code; flag 16 adds the text. */
static void kitty_event_alternate_all_and_text_flags(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "\033[>1u");
    CHECK_STR(kev(t, 'a', 0, VT_KEY_EV_RELEASE, 0, 0, 'a'), "");   /* flag 2 off */
    CHECK_STR(kev(t, 'a', 0, VT_KEY_EV_REPEAT, 0, 0, 'a'), "a");   /* a repeat is a press */
    h_put(t, "\033[=3u");
    CHECK_STR(kev(t, 'a', 0, VT_KEY_EV_RELEASE, 0, 0, 'a'), "\033[97;1:3u");
    CHECK_STR(kev(t, VT_KEY_UP, 0, VT_KEY_EV_REPEAT, 0, 0, 0), "\033[1;1:2A");
    CHECK_STR(kev(t, VT_KEY_RETURN, 0, VT_KEY_EV_RELEASE, 0, 0, 0), ""); /* only with 8 */
    CHECK_STR(kev(t, 'a', VT_MOD_CTRL, VT_KEY_EV_PRESS, 0, 0, 0), "\033[97;5u");
    h_put(t, "\033[=5u");                          /* 1 + 4: alternates */
    CHECK_STR(kev(t, 'a', VT_MOD_SHIFT, VT_KEY_EV_PRESS, 'A', 0, 'A'), "A");
    CHECK_STR(kev(t, 'a', VT_MOD_SHIFT | VT_MOD_CTRL, VT_KEY_EV_PRESS, 'A', 0, 0), "\033[97:65;6u");
    CHECK_STR(kev(t, 'a', VT_MOD_CTRL, VT_KEY_EV_PRESS, 0, 'q', 0), "\033[97::113;5u");
    h_put(t, "\033[=8u");
    CHECK_STR(kev(t, 'a', 0, VT_KEY_EV_PRESS, 0, 0, 'a'), "\033[97u");
    CHECK_STR(kev(t, 'a', VT_MOD_SHIFT, VT_KEY_EV_PRESS, 'A', 0, 'A'), "\033[97;2u");
    CHECK_STR(kkey(t, VT_KEY_RETURN, 0), "\033[13u");
    CHECK_STR(kkey(t, VT_KEY_KP_5, 0), "\033[57404u");
    CHECK_STR(kkey(t, 'A', VT_MOD_SHIFT), "\033[97;2u");
    h_put(t, "\033[=24u");                         /* 8 + 16: the text too */
    CHECK_STR(kev(t, 'a', 0, VT_KEY_EV_PRESS, 0, 0, 'a'), "\033[97;;97u");
    CHECK_STR(kev(t, 'a', VT_MOD_SHIFT, VT_KEY_EV_PRESS, 'A', 0, 'A'), "\033[97;2;65u");
    CHECK_STR(kev(t, 'a', VT_MOD_CTRL, VT_KEY_EV_PRESS, 0, 0, 1), "\033[97;5u"); /* no control text */
    h_put(t, "\033[=12u");                         /* 4 + 8 */
    CHECK_STR(kev(t, 'a', VT_MOD_SHIFT, VT_KEY_EV_PRESS, 'A', 0, 'A'), "\033[97:65;2u");
    h_put(t, "\033[=0u");                          /* off again */
    CHECK_STR(kev(t, 'a', 0, VT_KEY_EV_RELEASE, 0, 0, 'a'), "");
    CHECK_STR(kev(t, 'a', VT_MOD_CTRL, VT_KEY_EV_PRESS, 0, 0, 1), "\001");
    vt_free(t);
}

/* ---- a terminal with the host callbacks the OSC tests need ---- */

static struct {
    char sel[16];
    unsigned char *data;
    long len;
    int sets;
    const char *give;          /* what the "clipboard" holds for a query */
    char cwd[4200];
    int cwds;
    char note_title[128], note_body[300];
    int notes;
} hb;

static void p_reply(void *u, const vt_u8 *b, long n)
{
    (void)u;
    if (h_reply_len + n < (long)sizeof(h_reply) - 1) {
        memcpy(h_reply + h_reply_len, b, (size_t)n);
        h_reply_len += (int)n;
        h_reply[h_reply_len] = 0;
    }
}

static void p_clip_set(void *u, const char *sel, const vt_u8 *data, long len)
{
    (void)u;
    strncpy(hb.sel, sel, sizeof(hb.sel) - 1);
    free(hb.data);
    hb.data = (unsigned char *)malloc((size_t)len + 1);
    memcpy(hb.data, data, (size_t)len);
    hb.len = len;
    hb.sets++;
}

static long p_clip_get(void *u, vt_u8 *buf, long max)
{
    long n = hb.give ? (long)strlen(hb.give) : 0;
    (void)u;
    if (n > max)
        n = max;
    memcpy(buf, hb.give, (size_t)n);
    return n;
}

static void p_cwd(void *u, const char *uri)
{
    (void)u;
    strncpy(hb.cwd, uri, sizeof(hb.cwd) - 1);
    hb.cwds++;
}

static void p_notify(void *u, const char *title, const char *body)
{
    (void)u;
    strncpy(hb.note_title, title, sizeof(hb.note_title) - 1);
    strncpy(hb.note_body, body, sizeof(hb.note_body) - 1);
    hb.notes++;
}

static vt_term *p_new(int cols, int rows)
{
    vt_callbacks cb;
    vt_term *t;
    memset(&cb, 0, sizeof(cb));
    cb.reply = p_reply;
    cb.clipboard_set = p_clip_set;
    cb.clipboard_get = p_clip_get;
    cb.cwd = p_cwd;
    cb.notify = p_notify;
    t = vt_new(cols, rows, 100, &cb, 0);
    free(hb.data);
    memset(&hb, 0, sizeof(hb));
    h_reply_clear();
    return t;
}

/* ---- G3-12: OSC 52, the clipboard ---- */

static long b64(const unsigned char *in, long n, char *out)
{
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    long i, k = 0;
    for (i = 0; i + 2 < n; i += 3) {
        out[k++] = a[in[i] >> 2];
        out[k++] = a[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
        out[k++] = a[((in[i + 1] & 15) << 2) | (in[i + 2] >> 6)];
        out[k++] = a[in[i + 2] & 63];
    }
    if (n - i == 1) {
        out[k++] = a[in[i] >> 2];
        out[k++] = a[(in[i] & 3) << 4];
        out[k++] = '=';
        out[k++] = '=';
    } else if (n - i == 2) {
        out[k++] = a[in[i] >> 2];
        out[k++] = a[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
        out[k++] = a[(in[i + 1] & 15) << 2];
        out[k++] = '=';
    }
    out[k] = 0;
    return k;
}

/* A program sets the clipboard with OSC 52 ; c ; base64 -- of any size up
 * to 1 MB, in as many writes as it comes (the 256-byte string buffer never
 * held a payload). */
static void osc52_sets_the_clipboard_in_any_size(void)
{
    vt_term *t = p_new(20, 3);
    long n = 300000L, i, k;
    unsigned char *big = (unsigned char *)malloc((size_t)n);
    char *enc = (char *)malloc((size_t)n * 2);
    h_put(t, "\033]52;c;aGVsbG8gd29ybGQ=\007");
    CHECK_INT(hb.sets, 1);
    CHECK_INT(hb.len, 11);
    CHECK_INT(memcmp(hb.data, "hello world", 11), 0);
    CHECK_STR(hb.sel, "c");
    for (i = 0; i < n; i++)
        big[i] = (unsigned char)(i * 7 + (i >> 9));
    k = b64(big, n, enc);
    h_put(t, "\033]52;;");
    for (i = 0; i < k; i += 1000)                  /* in 1000-byte writes */
        vt_write(t, (const vt_u8 *)enc + i, k - i < 1000 ? k - i : 1000);
    h_put(t, "\033\\");
    CHECK_INT(hb.sets, 2);
    CHECK_INT(hb.len, n);
    CHECK_INT(memcmp(hb.data, big, (size_t)n), 0);
    CHECK_STR(h_screen(t), "");                    /* none of it on the screen */
    h_put(t, "\033]52;c;!!!!\007");                /* not base64: ignored */
    CHECK_INT(hb.sets, 2);
    h_put(t, "\033]52;c;\007");                    /* empty: the clipboard emptied */
    CHECK_INT(hb.sets, 3);
    CHECK_INT(hb.len, 0);
    free(big);
    free(enc);
    vt_free(t);
}

/* Past 1 MB the set is dropped whole (no half clipboard), and the
 * terminal goes on as before. */
static void osc52_larger_than_a_megabyte_is_dropped(void)
{
    vt_term *t = p_new(20, 3);
    long i;
    char chunk[4097];
    memset(chunk, 'Q', 4096);
    chunk[4096] = 0;
    h_put(t, "\033]52;c;");
    for (i = 0; i < 342; i++)                      /* 1.4 MB of base64: 1.05 MB of data */
        h_put(t, chunk);
    h_put(t, "\007ok");
    CHECK_INT(hb.sets, 0);
    CHECK_STR(h_screen(t), "ok");
    vt_free(t);
}

/* Reading the clipboard (OSC 52 ; c ; ?) is off unless the host allows
 * it (a profile setting, as xterm's disallowedWindowOps): a remote program
 * must not read what the user copied. */
static void osc52_query_only_when_the_host_allows_it(void)
{
    vt_term *t = p_new(20, 3);
    hb.give = "secret";
    h_put(t, "\033]52;c;?\007");
    CHECK_INT(h_reply_len, 0);
    vt_set_clipboard_access(t, VT_CLIP_WRITE | VT_CLIP_READ);
    h_put(t, "\033]52;c;?\007");
    REPLY("\033]52;c;c2VjcmV0\007");
    h_put(t, "\033]52;p;?\033\\");
    REPLY("\033]52;p;c2VjcmV0\033\\");
    vt_set_clipboard_access(t, 0);                 /* off: no writes either */
    h_put(t, "\033]52;c;aGk=\007");
    CHECK_INT(hb.sets, 0);
    h_put(t, "\033c");                             /* a host setting: RIS leaves it */
    vt_set_clipboard_access(t, VT_CLIP_WRITE);
    h_put(t, "\033c\033]52;c;aGk=\007");
    CHECK_INT(hb.sets, 1);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* An 8-bit window's OSC 52 text and the clipboard's Latin-1: the
 * conversions both ways (clipfmt, which vtwin's OSC 52 callbacks use). */
static void utf8_and_latin1_conversions(void)
{
    char out[32];
    long n = cf_to_latin1("a\xc3\xa9\xe2\x82\xac", 6, out);
    CHECK_INT(n, 3);
    CHECK_INT(memcmp(out, "a\xe9?", 3), 0);        /* the euro sign has no Latin-1 */
    n = cf_from_latin1("a\xe9", 2, out);
    CHECK_INT(n, 3);
    CHECK_INT(memcmp(out, "a\xc3\xa9", 3), 0);
}

/* ---- G3-17: colours by name ---- */

/* OSC 4 / 10-12 take X11 colour names as xterm does (terminfo's Cs passes
 * whatever the user named: tmux's cursor-colour "red"), and tmux's
 * "colourN" as palette entry N. Unknown names change nothing. */
static void osc_colours_take_x11_names(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    vt_set_default_colors(t, 0xC0C0C0UL, 0x000000UL, 0xFFFFFFUL);
    h_put(t, "\033]12;red\007");
    CHECK_INT(vt_default_color(t, 2) == 0xFF0000UL, 1);
    h_put(t, "\033]11;Dark Slate Gray\033\\");      /* case and blanks as X11 */
    CHECK_INT(vt_default_color(t, 1) == 0x2F4F4FUL, 1);
    h_put(t, "\033]10;colour196\007");
    CHECK_INT(vt_default_color(t, 0) == 0xFF0000UL, 1);
    h_put(t, "\033]4;3;navy\007");
    CHECK_INT(vt_palette_rgb(t, 3) == 0x000080UL, 1);
    h_put(t, "\033]12;nosuchcolour\007");
    CHECK_INT(vt_default_color(t, 2) == 0xFF0000UL, 1);
    h_put(t, "\033]112\007");                       /* Cr: back to the host's */
    CHECK_INT(vt_default_color(t, 2) == 0xFFFFFFUL, 1);
    vt_free(t);
}

/* ---- G3-13..16: OSC 7, 8, 133, 9 and 777 ---- */

/* OSC 7 tells the terminal the shell's directory (a file: URL, longer
 * than the 256-byte string buffer when it must be); the host hears it,
 * vt_cwd keeps the last one. */
static void osc7_keeps_the_working_directory(void)
{
    vt_term *t = p_new(20, 3);
    char url[1100];
    CHECK_STR(vt_cwd(t), "");
    h_put(t, "\033]7;file://amiga/Work/src\033\\");
    CHECK_STR(vt_cwd(t), "file://amiga/Work/src");
    CHECK_STR(hb.cwd, "file://amiga/Work/src");
    strcpy(url, "\033]7;file:///Work/");
    memset(url + 17, 'd', 1000);
    strcpy(url + 1017, "\007");
    h_put(t, url);
    CHECK_INT((long)strlen(vt_cwd(t)), 13 + 1000); /* whole, past 256 bytes */
    CHECK_INT(hb.cwds, 2);
    h_put(t, "\033c");                             /* RIS: the shell will say again */
    CHECK_STR(vt_cwd(t), "");
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* OSC 8 ; params ; URI starts a hyperlink, OSC 8 ; ; ends it. The cells
 * keep it through the rare-style index (sizeof(vt_cell) does not grow),
 * through SGR changes and resets, into the scrollback. */
static void osc8_hyperlinks_stay_with_their_cells(void)
{
    vt_term *t = p_new(20, 3);
    char url[700];
    h_put(t, "a\033]8;;https://example.org/x\033\\li\033[1mn\033[0mk\033]8;;\033\\b");
    CHECK(vt_cell_link(t, h_cell(t, 0, 0)) == 0);
    CHECK_STR(vt_cell_link(t, h_cell(t, 1, 0)), "https://example.org/x");
    CHECK_STR(vt_cell_link(t, h_cell(t, 3, 0)), "https://example.org/x");  /* bold */
    CHECK_STR(vt_cell_link(t, h_cell(t, 4, 0)), "https://example.org/x");  /* after SGR 0 */
    CHECK(vt_cell_link(t, h_cell(t, 5, 0)) == 0);
    CHECK_INT(h_cell(t, 3, 0)->attr, VT_ATTR_BOLD);
    /* a long URI, with an id */
    strcpy(url, "\033]8;id=7;http://h/");
    memset(url + 18, 'p', 600);
    strcpy(url + 618, "\007Z\033]8;;\007");
    h_put(t, "\r\n");
    h_put(t, url);
    CHECK_INT((long)strlen(vt_cell_link(t, h_cell(t, 0, 1))), 9 + 600);
    /* the same id and URI again: the same link */
    h_put(t, url);
    CHECK(vt_cell_link(t, h_cell(t, 0, 1)) == vt_cell_link(t, h_cell(t, 1, 1)));
    h_put(t, "\r\n\n\n");                           /* into the scrollback */
    {
        int n;
        const vt_cell *row = vt_row(t, -1, &n);
        CHECK(row && vt_cell_link(t, &row[1]) != 0);
    }
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* Many links over time: the table makes room by dropping links no cell
 * shows any more. */
static void osc8_old_links_make_room(void)
{
    vt_term *t = p_new(10, 2);
    int i;
    for (i = 0; i < 500; i++) {
        char b[64];
        char *e = fmt3(b, "\033]8;;u", i, "\007x\033]8;;\007\033[H\033[2J");
        (void)e;
        h_put(t, b);
    }
    h_put(t, "\033]8;;last\007y\033]8;;\007");
    CHECK_STR(vt_cell_link(t, h_cell(t, 0, 0)), "last");
    vt_free(t);
}

/* OSC 133 marks the prompt (A), the command (B), its output (C) and its
 * end (D); vt_find_mark finds the prompts back, in the scrollback too. */
static void osc133_prompt_marks_and_jumps(void)
{
    vt_term *t = p_new(20, 4);
    int i;
    for (i = 0; i < 3; i++)
        h_put(t, "\033]133;A\007$ \033]133;B\007ls\r\n\033]133;C\007out1\r\nout2\r\n\033]133;D;0\007");
    h_put(t, "\033]133;A\007$ ");
    /* 3 x 3 lines + the last prompt: rows 0..9 of 10, 4 on screen */
    CHECK_INT(vt_row_marks(t, 3) & VT_MARK_PROMPT, VT_MARK_PROMPT);
    CHECK_INT(vt_row_marks(t, 2) & VT_MARK_PROMPT, 0);
    CHECK_INT(vt_row_marks(t, 1) & VT_MARK_OUTPUT, VT_MARK_OUTPUT);
    CHECK_INT(vt_find_mark(t, 3, -1, VT_MARK_PROMPT), 0);
    CHECK_INT(vt_find_mark(t, 0, -1, VT_MARK_PROMPT), -3);
    CHECK_INT(vt_find_mark(t, -3, -1, VT_MARK_PROMPT), -6);
    CHECK_INT(vt_find_mark(t, -6, -1, VT_MARK_PROMPT), VT_ROW_NONE);
    CHECK_INT(vt_find_mark(t, -6, 1, VT_MARK_PROMPT), -3);
    CHECK_INT(vt_find_mark(t, 0, 1, VT_MARK_PROMPT), 3);
    CHECK_INT(vt_find_mark(t, 3, 1, VT_MARK_PROMPT), VT_ROW_NONE);
    h_put(t, "\033[2J");                            /* an erased line forgets its mark */
    CHECK_INT(vt_row_marks(t, 3), 0);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* OSC 9 (iTerm2) and OSC 777 ; notify (urxvt, foot) reach the host as a
 * notification; OSC 9 ; 4 ; ... (a progress report) is not one. */
static void osc9_and_777_notify(void)
{
    vt_term *t = p_new(20, 3);
    h_put(t, "\033]9;build done\007");
    CHECK_INT(hb.notes, 1);
    CHECK_STR(hb.note_title, "");
    CHECK_STR(hb.note_body, "build done");
    h_put(t, "\033]777;notify;make;all 3 targets\033\\");
    CHECK_INT(hb.notes, 2);
    CHECK_STR(hb.note_title, "make");
    CHECK_STR(hb.note_body, "all 3 targets");
    h_put(t, "\033]9;4;1;50\007");                  /* ConEmu progress */
    CHECK_INT(hb.notes, 2);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 0);
    vt_free(t);
}

/* The Amiga side of OSC 7 / 8 (config/termurl): vsh's directory as a
 * file: URL, the URL back to a directory a new tab can start in (when it
 * is this machine's), and the command that opens a link -- never with
 * text that could break out of its quotes. */
static void termurl_round_trips_and_refuses_what_it_cannot_quote(void)
{
    char b[300];
    CHECK(termurl_osc7("Work:src/a b", "amiga", b, sizeof(b)) > 0);
    CHECK_STR(b, "\033]7;file://amiga/Work/src/a%20b\033\\");
    CHECK(termurl_osc7("RAM:", "", b, sizeof(b)) > 0);
    CHECK_STR(b, "\033]7;file:///RAM\033\\");
    CHECK_INT(termurl_osc7("Work:src", "amiga", b, 10), 0);   /* too long: nothing */
    CHECK(termurl_cwd_dir("file://amiga/Work/src/a%20b", "Amiga", b, sizeof(b)));
    CHECK_STR(b, "Work:src/a b");
    CHECK(termurl_cwd_dir("file:///RAM", "amiga", b, sizeof(b)));
    CHECK_STR(b, "RAM:");
    CHECK(termurl_cwd_dir("file://localhost/Work/x/", 0, b, sizeof(b)));
    CHECK_STR(b, "Work:x");
    CHECK(!termurl_cwd_dir("file://server/home/me", "amiga", b, sizeof(b)));  /* remote */
    CHECK(!termurl_cwd_dir("http://amiga/Work", "amiga", b, sizeof(b)));
    CHECK(!termurl_cwd_dir("file:///Work/a%22b", 0, b, sizeof(b)));          /* a quote */
    CHECK(!termurl_cwd_dir("file:///Work/a*b", 0, b, sizeof(b)));
    CHECK(!termurl_cwd_dir("file:///", 0, b, sizeof(b)));
    CHECK(termurl_link_command("OpenURL %s", "https://e.org/?q=1", b, sizeof(b)));
    CHECK_STR(b, "OpenURL \"https://e.org/?q=1\"");
    CHECK(termurl_link_command("C:Run >NIL: OpenURL", "http://x", b, sizeof(b)));
    CHECK_STR(b, "C:Run >NIL: OpenURL \"http://x\"");
    CHECK(!termurl_link_command("OpenURL %s", "http://x\" ; Delete SYS:#?", b, sizeof(b)));
    CHECK(!termurl_link_command("OpenURL %s", "http://x*N", b, sizeof(b)));
    CHECK(!termurl_link_command("OpenURL %s", "http://x", b, 12));
}

/* ---- G3-19: XTGETTCAP says what terminfo/vtcon.terminfo says ---- */

/* A terminfo string value as its bytes (terminfo(5) escapes), written here
 * on its own -- not tools/gen_vtcaps.py's -- so the check is independent. */
static int ti_unescape(const char *v, int n, unsigned char *out)
{
    int i = 0, k = 0;
    while (i < n) {
        char c = v[i];
        if (c == '\\' && i + 1 < n) {
            char d = v[i + 1];
            const char *from = "EenlrtbfS^\\,:", *to = "\033\033\n\n\r\t\b\f \136\\,:";
            const char *p = strchr(from, d);
            if (d == 's') {
                out[k++] = ' ';
                i += 2;
            } else if (p && d != 'S') {
                out[k++] = (unsigned char)to[p - from];
                i += 2;
            } else if (d >= '0' && d <= '7') {
                int j = i + 1, x = 0;
                while (j < n && j < i + 4 && v[j] >= '0' && v[j] <= '7')
                    x = x * 8 + (v[j++] - '0');
                out[k++] = (unsigned char)(x ? x : 0x80);
                i = j;
            } else {
                out[k++] = (unsigned char)d;
                i += 2;
            }
        } else if (c == '^' && i + 1 < n) {
            char d = v[i + 1];
            out[k++] = (unsigned char)(d == '?' ? 0x7F : ((d >= 'a' && d <= 'z' ? d - 32 : d) & 0x1F));
            i += 2;
        } else {
            out[k++] = (unsigned char)c;
            i++;
        }
    }
    return k;
}

static void hexstr(char *out, const unsigned char *s, int n)
{
    static const char hex[] = "0123456789ABCDEF";
    int i;
    for (i = 0; i < n; i++) {
        *out++ = hex[s[i] >> 4];
        *out++ = hex[s[i] & 15];
    }
    *out = 0;
}

static void xtgettcap_answers_every_terminfo_capability(void)
{
    static char text[16384], body[16384];
    vt_term *t = h_new(80, 24, VT_XTERM);
    FILE *f = fopen("terminfo/vtcon.terminfo", "r");
    long n = f ? (long)fread(text, 1, sizeof(text) - 1, f) : 0;
    long i, b = 0;
    int caps = 0, bad = 0, first = 1;
    char *line;
    if (f)
        fclose(f);
    CHECK(n > 0);
    text[n] = 0;
    /* the entry's body: no comment lines, no names line */
    for (line = strtok(text, "\n"); line; line = strtok(0, "\n")) {
        while (*line == ' ' || *line == '\t')
            line++;
        if (!*line || *line == '#')
            continue;
        if (first) {
            first = 0;
            continue;
        }
        b += (long)strlen(strcpy(body + b, line));
        body[b++] = ' ';
    }
    body[b] = 0;
    for (i = 0; i < b;) {
        long e = i;
        char name[32], q[96], want[700];
        unsigned char val[300];
        int vl = 0, nl, kind;
        while (e < b && body[e] != ',')
            e += body[e] == '\\' ? 2 : 1;
        while (i < e && body[i] == ' ')
            i++;
        if (i < e) {
            long eq = i;
            while (eq < e && body[eq] != '=' && body[eq] != '#')
                eq++;
            nl = (int)(eq - i);
            memcpy(name, body + i, (size_t)nl);
            name[nl] = 0;
            kind = eq == e ? 'b' : body[eq];
            if (kind == '=')
                vl = ti_unescape(body + eq + 1, (int)(e - eq - 1), val);
            else if (kind == '#') {
                long x = strtol(body + eq + 1, 0, 0);
                fmt3((char *)val, "", x, "");
                vl = (int)strlen((char *)val);
            }
            if (strcmp(name, "RGB")) { /* RGB answers xterm's 8/8/8 (caps_extra) */
                strcpy(q, "\033P+q");
                hexstr(q + 4, (unsigned char *)name, nl);
                strcat(q, "\033\\");
                h_put(t, q);
                strcpy(want, "\033P1+r");
                hexstr(want + 5, (unsigned char *)name, nl);
                if (kind != 'b') {
                    strcat(want, "=");
                    hexstr(want + strlen(want), val, vl);
                }
                strcat(want, "\033\\");
                if (h_reply_len != (int)strlen(want) || memcmp(h_reply, want, strlen(want))) {
                    printf("    XTGETTCAP %s: [%.*s]\n", name, h_reply_len, h_reply);
                    bad++;
                }
                h_reply_clear();
            }
            caps++;
        }
        i = e + 1;
    }
    CHECK(caps > 250);                             /* the whole entry was read */
    CHECK_INT(bad, 0);
    h_put(t, "\033P+q4E6F7065\033\\");              /* "Nope" */
    REPLY("\033P0+r4E6F7065\033\\");
    h_put(t, "\033P+q524742\033\\");                /* RGB */
    REPLY("\033P1+r524742=382F382F38\033\\");
    vt_free(t);
}

void suite_protocol(void)
{
    xtgettcap_answers_every_terminfo_capability();
    termurl_round_trips_and_refuses_what_it_cannot_quote();
    osc7_keeps_the_working_directory();
    osc8_hyperlinks_stay_with_their_cells();
    osc8_old_links_make_room();
    osc133_prompt_marks_and_jumps();
    osc9_and_777_notify();
    osc_colours_take_x11_names();
    osc52_sets_the_clipboard_in_any_size();
    osc52_larger_than_a_megabyte_is_dropped();
    osc52_query_only_when_the_host_allows_it();
    utf8_and_latin1_conversions();
    kitty_keyboard_flags_stack_per_screen();
    kitty_disambiguate_flag();
    kitty_event_alternate_all_and_text_flags();
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
