/* ANSI music (PC-ANSI CSI M / CSI N / CSI | ... SO), through the engine's
 * parser: the notes the host is handed, and that none of it prints. */
#include "harness.h"

static vt_term *play(const char *s)
{
    vt_term *t = h_new(20, 3, VT_PCANSI);
    h_put(t, s);
    return t;
}

static void note_is(int i, long chz, long ms, long on_ms)
{
    CHECK(i < h_note_n);
    if (i >= h_note_n)
        return;
    CHECK_INT(h_notes[i].chz, chz);
    CHECK_INT(h_notes[i].ms, ms);
    CHECK_INT(h_notes[i].on_ms, on_ms);
}

static void middle_c_quarter_at_120(void)
{
    vt_term *t = play("\x1b[MT120 L4 O3 C\x0e");
    CHECK_INT(h_music_calls, 1);
    CHECK_INT(h_note_n, 1);
    note_is(0, 26163, 500, 437); /* 261.63 Hz, 500 ms, 7/8 sounds */
    CHECK_STR(h_screen(t), "");
    vt_free(t);
}

static void default_octave_is_four(void)
{
    vt_term *t = play("\x1b[MC\x0e");
    note_is(0, 52326, 500, 437); /* O4 C = 523.26 Hz (GW-BASIC) */
    vt_free(t);
}

static void sharp_length_and_dot(void)
{
    vt_term *t = play("\x1b[MFO3C#8.\x0e");
    CHECK_INT(h_music_bg, 0);
    note_is(0, 27718, 375, 328); /* 250 + 125 */
    vt_free(t);
}

static void flats_and_octave_wrap(void)
{
    vt_term *t = play("\x1b[NO3D-C-\x0e");
    note_is(0, 27718, 500, 437); /* D flat = C sharp */
    note_is(1, 24694, 500, 437); /* C flat = O2 B */
    vt_free(t);
}

static void note_numbers_and_rests(void)
{
    vt_term *t = play("\x1b[NN46N0P8R\x0e");
    CHECK_INT(h_note_n, 4);
    note_is(0, 44000, 500, 437); /* N46 = A 440 */
    note_is(1, 0, 500, 0);
    note_is(2, 0, 250, 0);
    note_is(3, 0, 500, 0);
    vt_free(t);
}

static void bar_form_length_and_tempo(void)
{
    vt_term *t = play("\x1b[|L8T60O2A\x0e");
    note_is(0, 22000, 500, 437); /* 240000 / 60 / 8 */
    vt_free(t);
}

static void octave_up_and_down(void)
{
    vt_term *t = play("\x1b[NO3>C<<C\x0e");
    note_is(0, 52326, 500, 437);
    note_is(1, 13081, 500, 437);
    vt_free(t);
}

static void staccato_legato_background(void)
{
    vt_term *t = play("\x1b[MSO3C\x0e");
    note_is(0, 26163, 500, 375);
    vt_free(t);
    t = play("\x1b[MBMLO3C\x0e");
    CHECK_INT(h_music_bg, 1);
    note_is(0, 26163, 500, 500);
    vt_free(t);
}

static void state_persists_across_strings(void)
{
    vt_term *t = play("\x1b[NT60\x0e\x1b[NC\x0e");
    CHECK_INT(h_music_calls, 1); /* the first string had no note */
    note_is(0, 52326, 1000, 875);
    vt_free(t);
}

static void music_text_never_prints(void)
{
    int x, y;
    vt_term *t = play("\x1b[MT120 L4 O3 C\x0eok");
    CHECK_STR(h_screen(t), "ok");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    vt_free(t);
}

static void split_across_writes(void)
{
    vt_term *t = play("\x1b[M");
    CHECK_INT(h_music_calls, 0);
    h_put(t, "O3");
    h_put(t, "C\x0e");
    CHECK_INT(h_music_calls, 1);
    note_is(0, 26163, 500, 437);
    vt_free(t);
}

static void escape_ends_the_string(void)
{
    vt_term *t = play("\x1b[NO3C\x1b[1mX");
    CHECK_INT(h_music_calls, 1);
    note_is(0, 26163, 500, 437);
    CHECK_STR(h_screen(t), "X");
    CHECK(h_cell(t, 0, 0)->attr & VT_ATTR_BOLD);
    vt_free(t);
}

static void long_string_comes_in_parts(void)
{
    char s[256];
    int i, n = 0;
    s[n++] = 0x1b;
    s[n++] = '[';
    s[n++] = 'N';
    for (i = 0; i < 70; i++)
        s[n++] = 'C';
    s[n++] = 0x0e;
    s[n] = 0;
    {
        vt_term *t = play(s);
        CHECK_INT(h_music_calls, 2);
        CHECK_INT(h_note_n, 70);
        vt_free(t);
    }
}

static void music_off_swallows_quietly(void)
{
    vt_term *t = h_new(20, 3, VT_PCANSI);
    vt_set_music(t, 0);
    h_put(t, "\x1b[MO3C\x0eok");
    CHECK_INT(h_music_calls, 0);
    CHECK_STR(h_screen(t), "ok");
    vt_free(t);
}

static void delete_line_stays(void)
{
    vt_term *t = play("aaaa\r\nbbbb\r\ncccc\x1b[1;1H\x1b[2M");
    CHECK_STR(h_screen(t), "cccc");
    vt_free(t);
    t = play("aaaa\r\nbbbb\r\ncccc\x1b[1;1H\x1b[Mxy");
    CHECK_INT(h_music_calls, 0);
    CHECK_STR(h_screen(t), "xybb|cccc"); /* x starts no music: Delete Line */
    vt_free(t);
}

static void xterm_takes_no_music(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    h_put(t, "aaaa\r\nbbbb\x1b[1;1H\x1b[MC\x0e");
    CHECK_INT(h_music_calls, 0);
    CHECK_STR(h_screen(t), "Cbbb"); /* Delete Line, then the C prints */
    vt_free(t);
}

static void paula_periods(void)
{
    CHECK_INT(vt_note_period(26163, 16), 847);
    CHECK_INT(vt_note_period(44000, 16), 503);
    CHECK_INT(vt_note_period(0, 16), 0);
    CHECK_INT(vt_note_period(395104, 16), 124); /* clamped: Paula's fastest */
    CHECK_INT(vt_note_period(1, 16), 65535);
}

void suite_ansimusic(void)
{
    middle_c_quarter_at_120();
    default_octave_is_four();
    sharp_length_and_dot();
    flats_and_octave_wrap();
    note_numbers_and_rests();
    bar_form_length_and_tempo();
    octave_up_and_down();
    staccato_legato_background();
    state_persists_across_strings();
    music_text_never_prints();
    split_across_writes();
    escape_ends_the_string();
    long_string_comes_in_parts();
    music_off_swallows_quietly();
    delete_line_stays();
    xterm_takes_no_music();
    paula_periods();
}
