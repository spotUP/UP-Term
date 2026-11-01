/* ANSI music (PC-ANSI CSI M / CSI N / CSI | ... SO), through the engine's
 * parser: the notes the host is handed, and that none of it prints. */
#include <ctype.h>
#include <stdlib.h>
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
    note_is(0, 52300, 500, 437); /* O3 C = 523 Hz, 500 ms, 7/8 sounds */
    CHECK_STR(h_screen(t), "");
    vt_free(t);
}

static void default_octave_is_four(void)
{
    vt_term *t = play("\x1b[MC\x0e");
    note_is(0, 104700, 500, 437); /* O4 C = 1046.5 rounded up to 1047 Hz (GW-BASIC) */
    vt_free(t);
}

static void sharp_length_and_dot(void)
{
    vt_term *t = play("\x1b[MFO3C#8.\x0e");
    CHECK_INT(h_music_bg, 0);
    note_is(0, 55400, 375, 328); /* O3 C# = 554 Hz; 250 * 1.5 */
    vt_free(t);
}

static void flats_and_octave_wrap(void)
{
    vt_term *t = play("\x1b[NO3D-C-\x0e");
    note_is(0, 55400, 500, 437); /* D flat = C sharp */
    note_is(1, 49400, 500, 437); /* C flat = O2 B (494 Hz) */
    vt_free(t);
}

static void note_numbers_and_rests(void)
{
    vt_term *t = play("\x1b[NN34N0P8P\x0e");
    CHECK_INT(h_note_n, 4);
    note_is(0, 44000, 500, 437); /* N34 = A 440 */
    note_is(1, 0, 500, 0);
    note_is(2, 0, 250, 0);
    note_is(3, 0, 500, 0);
    vt_free(t);
}

static void bar_form_length_and_tempo(void)
{
    vt_term *t = play("\x1b[|L8T60O2A\x0e");
    note_is(0, 44000, 500, 437); /* O2 A = 440 Hz; 240000 / 60 / 8 */
    vt_free(t);
}

static void octave_up_and_down(void)
{
    vt_term *t = play("\x1b[NO3>C<<C\x0e");
    note_is(0, 104700, 500, 437);
    note_is(1, 26200, 500, 437); /* O3 > C < < C = O2 C = 262 Hz */
    vt_free(t);
}

static void staccato_legato_background(void)
{
    vt_term *t = play("\x1b[MSO3C\x0e");
    note_is(0, 52300, 500, 375);
    vt_free(t);
    t = play("\x1b[MBMLO3C\x0e");
    CHECK_INT(h_music_bg, 1);
    note_is(0, 52300, 500, 500);
    vt_free(t);
}

static void state_persists_across_strings(void)
{
    vt_term *t = play("\x1b[NT60\x0e\x1b[NC\x0e");
    CHECK_INT(h_music_calls, 1); /* the first string had no note */
    note_is(0, 104700, 1000, 875);
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
    note_is(0, 52300, 500, 437);
    vt_free(t);
}

static void escape_ends_the_string(void)
{
    vt_term *t = play("\x1b[NO3C\x1b[1mX");
    CHECK_INT(h_music_calls, 1);
    note_is(0, 52300, 500, 437);
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

/* GW-BASIC pitch: octave-6 table shifted down, last bit shifted out added. */
static void gw_pitch_table(void)
{
    static const long hz[7] = { 65, 131, 262, 523, 1047, 2093, 4186 };
    int i;
    char s[] = "\x1b[NO0C\x0e";
    for (i = 0; i < 7; i++) {
        vt_term *t;
        s[4] = (char)('0' + i);
        t = play(s);
        note_is(0, hz[i] * 100, 500, 437);
        vt_free(t);
    }
    {
        vt_term *t = play("\x1b[NO2A\x0e");
        note_is(0, 44000, 500, 437); /* O2 A = 440 */
        vt_free(t);
    }
}

static void n_command_range(void)
{
    vt_term *t = play("\x1b[NN1N13N84N85N34\x0e");
    CHECK_INT(h_note_n, 4); /* N85 plays nothing */
    note_is(0, 6500, 500, 437);  /* N1 = C in O0 */
    note_is(1, 13100, 500, 437); /* N13 = C in O1 */
    note_is(2, 790200, 500, 437); /* N84 = B in O6 */
    note_is(3, 44000, 500, 437);
    vt_free(t);
}

static void dots_compound(void)
{
    vt_term *t = play("\x1b[NC.C..C...\x0e");
    note_is(0, 104700, 750, 656);
    note_is(1, 104700, 1125, 984);
    note_is(2, 104700, 1687, 1476);
    vt_free(t);
}

static void csi_m_n_digit_is_note_number(void)
{
    vt_term *t = play("\x1b[MN34\x0e");
    CHECK_INT(h_note_n, 1);
    note_is(0, 44000, 500, 437);
    vt_free(t);
    t = play("\x1b[Mn34\x0e");
    note_is(0, 44000, 500, 437);
    vt_free(t);
    t = play("\x1b[MNLO3C\x0e"); /* MN, then L (legato)... */
    CHECK_INT(h_note_n, 1);
    vt_free(t);
    t = play("\x1b[MNO3C\x0e"); /* MN (normal), then O3 C */
    note_is(0, 52300, 500, 437);
    vt_free(t);
    t = play("\x1b[MLMNO3C\x0e"); /* in-string MN stays a modifier */
    note_is(0, 52300, 500, 437);
    vt_free(t);
    t = play("\x1b[MMLN34\x0e"); /* only the first M: here ML, then N34 as a command */
    note_is(0, 44000, 500, 500);
    vt_free(t);
}

static void unknown_bytes_are_skipped_and_the_string_plays(void)
{
    vt_term *t = play("\x1b[NO3C\"X\r\n4R\x0e");
    CHECK_INT(h_music_calls, 1);
    CHECK_INT(h_note_n, 2);
    note_is(0, 52300, 500, 437);
    note_is(1, 0, 500, 0); /* R is a pause; the quote and the line break did not end the C */
    CHECK_STR(h_screen(t), "");
    vt_free(t);
}

static void semicolon_makes_a_sound_effect_block_silent(void)
{
    vt_term *t = play("\x1b[MF 7000;.12;200;15;-100\x0e");
    CHECK_INT(h_music_calls, 0);
    CHECK_STR(h_screen(t), "");
    vt_free(t);
    t = play("\x1b[NO3CDE;F\x0e\x1b[NO3C\x0e"); /* notes before the ';' are dropped */
    CHECK_INT(h_music_calls, 1);
    CHECK_INT(h_note_n, 1);
    vt_free(t);
}

/* Intro sequences of the file bytes, and how many have no ';' before their end. */
static void count_blocks(const unsigned char *b, long n, int *all, int *plain)
{
    long i, j;
    *all = *plain = 0;
    for (i = 0; i + 2 < n; i++) {
        int ok, semi = 0;
        unsigned char c;
        if (b[i] != 0x1b || b[i + 1] != '[')
            continue;
        c = b[i + 2];
        ok = c == 'N' || c == '|';
        if (c == 'M' && i + 3 < n) {
            unsigned char d = (unsigned char)(b[i + 3] | 0x20);
            ok = (d >= 'a' && d <= 'g') || d == 'l' || d == 'm' || d == 'n' || d == 'o' || d == 'p' || d == 's' || d == 't' || d == 'r' || b[i + 3] == '<' || b[i + 3] == '>';
        }
        if (!ok)
            continue;
        for (j = i + 3; j < n && b[j] != 0x0e && b[j] != 0x1b; j++)
            if (b[j] == ';')
                semi = 1;
        (*all)++;
        if (!semi)
            (*plain)++;
    }
}

static long read_file(const char *name, unsigned char *buf, long max)
{
    char path[128];
    FILE *f;
    long n;
    strcpy(path, "tests/data/ansi-music/");
    strcat(path, name);
    f = fopen(path, "rb");
    CHECK(f != 0);
    if (!f)
        return 0;
    n = (long)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

/* Real files: every block without a ';' plays (one call each; none is over
 * 64 notes), none of the music text reaches the screen. */
static void real_files_play_every_block(void)
{
    static const char *const names[3] = { "MUSIC_pavt199.ANS", "RELIGIOU.MUS", "ANSIPL20_INTRO.ANS" };
    static const int want_all[3] = { 1, 52, 23 };
    static unsigned char buf[40000];
    int k;
    for (k = 0; k < 3; k++) {
        long n = read_file(names[k], buf, sizeof buf);
        int all, plain;
        vt_term *t = h_new(80, 25, VT_PCANSI);
        count_blocks(buf, n, &all, &plain);
        CHECK_INT(all, want_all[k]);
        vt_write(t, buf, n);
        CHECK_INT(h_music_calls, plain);
        if (k == 0)
            note_is(0, 52300, 1000, 875); /* the LOGO tune: C at O3, a half note */
        if (k == 2)
            CHECK_INT(plain, 1);
        CHECK(strstr(h_screen(t), "T120") == 0 && strstr(h_screen(t), "T150") == 0 && strstr(h_screen(t), "MF ") == 0);
        vt_free(t);
    }
}

/* Shared vectors (master: DEViLBOX, same file in DCTelnet): each input is fed
 * through the parser and the notes, the gaps and the printed text are compared. */
#define VEC_MASTER "/Users/spot/Code/DEViLBOX/src/lib/import/__tests__/fixtures/ansi-music/vectors.json"
#define VEC_COPY "tests/data/ansi-music-vectors.json"

static int unhex(const char *h, unsigned char *out, int max)
{
    int n = 0;
    while (isxdigit((unsigned char)h[0]) && isxdigit((unsigned char)h[1]) && n < max) {
        unsigned v = 0;
        int k;
        for (k = 0; k < 2; k++) {
            char c = h[k];
            v = v * 16 + (unsigned)(c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0');
        }
        out[n++] = (unsigned char)v;
        h += 2;
    }
    return n;
}

typedef struct {
    char name[48];
    unsigned char in[400], text[400];
    int nin, ntext, nev;
    long hz[64], ms[64], gate[64];
} vec;

static void run_vector(const vec *v)
{
    vt_term *t = h_new(80, 25, VT_PCANSI);
    int i, plain_text = 1;
    vt_write(t, v->in, v->nin);
    if (h_note_n != v->nev)
        printf("[FAIL] vector %s: %d events, expected %d\n", v->name, h_note_n, v->nev);
    CHECK_INT(h_note_n, v->nev);
    for (i = 0; i < v->nev && i < h_note_n; i++) {
        long dm = h_notes[i].ms - v->ms[i], dg = h_notes[i].on_ms - v->gate[i];
        int ok = (long)(h_notes[i].chz / 100) == v->hz[i] && dm >= -1 && dm <= 1 && dg >= -1 && dg <= 1;
        if (!ok)
            printf("[FAIL] vector %s event %d: got %ld Hz %ld ms gate %ld, expected %ld Hz %ld ms gate %ld\n", v->name, i, (long)(h_notes[i].chz / 100), (long)h_notes[i].ms, (long)h_notes[i].on_ms, v->hz[i], v->ms[i], v->gate[i]);
        CHECK(ok);
    }
    for (i = 0; i < v->ntext; i++)
        if (v->text[i] == 0x1b)
            plain_text = 0;
    if (plain_text) {
        char want[401];
        memcpy(want, v->text, (size_t)v->ntext);
        want[v->ntext] = 0;
        if (strcmp(h_screen(t), want) != 0)
            printf("[FAIL] vector %s: screen \"%s\", expected \"%s\"\n", v->name, h_screen(t), want);
        CHECK_STR(h_screen(t), want);
    }
    vt_free(t);
}

static void shared_vectors(void)
{
    static vec v;
    char line[2048];
    FILE *f = fopen(VEC_COPY, "r");
    int count = 0, have = 0;
    CHECK(f != 0);
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *q, *e;
        if ((q = strstr(line, "\"name\": \"")) != 0) {
            if (have)
                run_vector(&v), count++;
            memset(&v, 0, sizeof v);
            have = 1;
            q += 9;
            e = strchr(q, '"');
            if (e && e - q < (long)sizeof v.name)
                memcpy(v.name, q, (size_t)(e - q));
        } else if ((q = strstr(line, "\"input_hex\": \"")) != 0) {
            v.nin = unhex(q + 14, v.in, (int)sizeof v.in);
        } else if ((q = strstr(line, "\"text_hex\": \"")) != 0) {
            v.ntext = unhex(q + 13, v.text, (int)sizeof v.text);
        } else if ((q = strstr(line, "\"hz\": ")) != 0 && v.nev < 64) {
            v.hz[v.nev] = atol(q + 6);
            v.nev++;
        } else if ((q = strstr(line, "\"ms\": ")) != 0 && v.nev > 0) {
            v.ms[v.nev - 1] = (long)(atof(q + 6) + 0.5);
        } else if ((q = strstr(line, "\"gate\": ")) != 0 && v.nev > 0) {
            v.gate[v.nev - 1] = (long)(atof(q + 8) + 0.5);
        }
    }
    fclose(f);
    if (have)
        run_vector(&v), count++;
    CHECK_INT(count, 34); /* DEViLBOX master dd37a194e: + intro-m-8-spaces, intro-m-9-spaces */
}

/* The copy must equal the master; absent master: skipped. */
static void shared_vectors_match_master(void)
{
    FILE *a = fopen(VEC_MASTER, "rb"), *b = fopen(VEC_COPY, "rb");
    int ca, cb, same = 1;
    if (!a) {
        printf("[SKIP] vectors master absent\n");
        if (b)
            fclose(b);
        return;
    }
    CHECK(b != 0);
    if (!b) {
        fclose(a);
        return;
    }
    do {
        ca = getc(a);
        cb = getc(b);
        if (ca != cb)
            same = 0;
    } while (ca != EOF && cb != EOF && same);
    fclose(a);
    fclose(b);
    CHECK(same);
}

static void csi_m_spaces_are_held(void)
{
    vt_term *t = play("\x1b[M   T200C\x0e");
    CHECK_INT(h_music_calls, 1);
    CHECK_STR(h_screen(t), "");
    vt_free(t);
    t = play("aaaa\r\nbbbb\x1b[1;1H\x1b[M  xy"); /* spaces, then not music: Delete Line, spaces print */
    CHECK_INT(h_music_calls, 0);
    CHECK_STR(h_screen(t), "  xy");
    vt_free(t);
    t = play("aaaa\r\nbbbb\x1b[1;1H\x1b[M         C"); /* a 9th space: Delete Line, nine spaces print */
    CHECK_INT(h_music_calls, 0);
    CHECK_STR(h_screen(t), "         C");
    vt_free(t);
}

static void clamps(void)
{
    vt_term *t = play("\x1b[NO9C\x0e");
    note_is(0, 418600, 500, 437); /* O6 */
    vt_free(t);
    t = play("\x1b[NT1C\x0e");
    note_is(0, 104700, 1875, 1640); /* T32: 240000 / 32 / 4 */
    vt_free(t);
    t = play("\x1b[NT999C\x0e");
    note_is(0, 104700, 235, 205); /* T255: 240000 / 255 / 4 */
    vt_free(t);
    t = play("\x1b[NL99C\x0e");
    note_is(0, 104700, 31, 27); /* L99 -> L64: 240000 / 120 / 64 = 31.25 ms */
    vt_free(t);
}

static void terminate_logo(void)
{
    vt_term *t = play("\x1b[MB O3 T120 L2CO4CO3L6BGABO4L2CO3L2CL2AL1GL2CFL4EL6CDEL4FL3DL6O2BO3CDL4EL2C\x0e");
    CHECK_INT(h_music_calls >= 1, 1);
    note_is(0, 52300, 1000, 875); /* C at O3, a half note at T120 */
    CHECK_STR(h_screen(t), "");
    vt_free(t);
}

static void paula_periods(void)
{
    CHECK_INT(vt_note_period(52300, 16), 423);
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
    gw_pitch_table();
    n_command_range();
    dots_compound();
    csi_m_n_digit_is_note_number();
    csi_m_spaces_are_held();
    unknown_bytes_are_skipped_and_the_string_plays();
    semicolon_makes_a_sound_effect_block_silent();
    real_files_play_every_block();
    shared_vectors();
    shared_vectors_match_master();
    clamps();
    terminate_logo();
    paula_periods();
}
