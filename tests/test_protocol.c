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
                                     1004, 1005, 1006, 1034, 1047, 1049, 2004, 2026, 2031, 7727 };
        int i, bad = 0;
        for (i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i++) {
            char want[64];
            ask_mode(t, modes[i], 1);
            fmt3(want, "\033[?", modes[i], ";1$y");
            if (h_reply_len != (int)strlen(want) || memcmp(h_reply, want, strlen(want))) {
                printf("    mode %d set: [%.*s]\n", modes[i], h_reply_len, h_reply);
                bad++;
            }
            h_reply_clear();
            ask_mode(t, modes[i], 0);
            fmt3(want, "\033[?", modes[i], ";2$y");
            if (h_reply_len != (int)strlen(want) || memcmp(h_reply, want, strlen(want))) {
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

void suite_protocol(void)
{
    sync_frame_is_held_up_to_one_second();
    decrqss_conformance_level_matches_da1();
    decrqm_answers_every_mode_the_engine_keeps();
    decrqss_sgr_round_trips_underline_colour_and_font();
    ris_resets_keys_title_stack_and_cursor_shape();
}
