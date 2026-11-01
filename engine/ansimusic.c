/* ansimusic.c: ANSI music strings (the GW-BASIC PLAY language) to notes.
 *
 * #included by vtengine.c before struct vt_term (ENGINE is the one file);
 * it knows nothing of the terminal. The engine's parser catches the
 * introducer (CSI M / CSI N / CSI |) and feeds the string here byte by
 * byte until SO. Rules from DCTelnet's ansimusic.c and GW-BASIC:
 *   A-G [#|+|-] [len] [.]...  note; len 1-64 (4 = quarter), default L
 *   N n [.]...                note number 1-84 (46 = A 440), 0 a rest
 *   P / R [len] [.]...        pause
 *   O n (0-6), < >            octave (O3 C = middle C)
 *   L n (1-64), T n (32-255)  default length, tempo (quarter notes a minute)
 *   MN ML MS                  normal (7/8 sounds), legato (all), staccato (3/4)
 *   MF MB                     foreground / background (a flag for the host)
 * Spaces and anything unknown are skipped. Portable C89. */

#define AM_NONE 0

typedef struct vt_music {
    vt_u8 tempo, len, oct, style; /* style 0 normal, 1 legato, 2 staccato */
    vt_u8 bg;                     /* MB given: background */
    vt_u8 cmd;                    /* the command waiting for its argument, AM_NONE none */
    vt_u8 has_num, dots;
    int acc;                      /* -1 flat, +1 sharp */
    long num;
    int n;                        /* notes in the buffer */
    vt_note notes[VT_MUSIC_NOTES];
} vt_music;

/* Octave 3, C to B, in centi-Hz. */
static const vt_u32 am_oct3[12] = {
    26163, 27718, 29366, 31113, 32963, 34923, 36999, 39200, 41530, 44000, 46616, 49388
};

static void am_init(vt_music *m)
{
    memset(m, 0, sizeof(*m));
    m->tempo = 120;
    m->len = 4;
    m->oct = 4;
}

static vt_u32 am_chz(int oct, int semi)
{
    vt_u32 c;
    while (semi < 0) {
        semi += 12;
        oct--;
    }
    while (semi > 11) {
        semi -= 12;
        oct++;
    }
    if (oct < 0)
        oct = 0;
    if (oct > 6)
        oct = 6;
    c = am_oct3[semi];
    if (oct > 3)
        c <<= oct - 3;
    else
        c >>= 3 - oct;
    return c;
}

/* A len note at the tempo, with dots: whole = 240000 / tempo ms. */
static vt_u32 am_ms(const vt_music *m, long len, int dots)
{
    vt_u32 add = 240000UL / m->tempo / (vt_u32)len;
    vt_u32 ms = add;
    while (dots-- > 0) {
        add /= 2;
        ms += add;
    }
    return ms > 65535UL ? 65535UL : ms;
}

static void am_add(vt_music *m, vt_u32 chz, vt_u32 ms)
{
    vt_note *nt;
    if (ms == 0 || m->n >= VT_MUSIC_NOTES)
        return;
    nt = &m->notes[m->n++];
    nt->chz = chz;
    nt->ms = (vt_u16)ms;
    if (!chz)
        nt->on_ms = 0;
    else if (m->style == 1)
        nt->on_ms = (vt_u16)ms;
    else if (m->style == 2)
        nt->on_ms = (vt_u16)(ms * 3 / 4);
    else
        nt->on_ms = (vt_u16)(ms * 7 / 8);
}

/* The waiting command has all of its argument: carry it out. */
static void am_finish(vt_music *m)
{
    static const vt_u8 base[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* A..G from C */
    vt_u8 c = m->cmd;
    long v = m->num;
    long len = m->has_num && m->num >= 1 && m->num <= 64 ? m->num : m->len;
    m->cmd = AM_NONE;
    if (c >= 'A' && c <= 'G') {
        am_add(m, am_chz(m->oct, base[c - 'A'] + m->acc), am_ms(m, len, m->dots));
    } else if (c == 'N') {
        if (m->has_num && v >= 1 && v <= 84)
            am_add(m, am_chz((int)((v - 1) / 12), (int)((v - 1) % 12)), am_ms(m, m->len, m->dots));
        else if (m->has_num && v == 0)
            am_add(m, 0, am_ms(m, m->len, m->dots));
    } else if (c == 'P' || c == 'R') {
        am_add(m, 0, am_ms(m, len, m->dots));
    } else if (c == 'O') {
        if (m->has_num && v <= 6)
            m->oct = (vt_u8)v;
    } else if (c == 'L') {
        if (m->has_num && v >= 1 && v <= 64)
            m->len = (vt_u8)v;
    } else if (c == 'T') {
        if (m->has_num && v >= 32 && v <= 255)
            m->tempo = (vt_u8)v;
    }
}

/* One byte of the string. */
static void am_byte(vt_music *m, vt_u8 c)
{
    if (c >= 'a' && c <= 'z')
        c = (vt_u8)(c - 'a' + 'A');
    if (m->cmd == 'M') { /* M and its letter */
        m->cmd = AM_NONE;
        if (c == 'F')
            m->bg = 0;
        else if (c == 'B')
            m->bg = 1;
        else if (c == 'N')
            m->style = 0;
        else if (c == 'L')
            m->style = 1;
        else if (c == 'S')
            m->style = 2;
        else
            am_byte(m, c); /* not a modifier: a command of its own */
        return;
    }
    if (c >= '0' && c <= '9') {
        if (m->cmd != AM_NONE && !m->dots) {
            if (m->num < 100000L)
                m->num = m->num * 10 + (c - '0');
            m->has_num = 1;
        }
        return;
    }
    if (c == '.') {
        if (m->dots < 8)
            m->dots++;
        return;
    }
    if ((c == '#' || c == '+' || c == '-') && m->cmd >= 'A' && m->cmd <= 'G' && !m->has_num) {
        m->acc += c == '-' ? -1 : 1;
        return;
    }
    if (c == ' ')
        return;
    if (m->cmd != AM_NONE)
        am_finish(m);
    if (c == '<') {
        if (m->oct > 0)
            m->oct--;
    } else if (c == '>') {
        if (m->oct < 6)
            m->oct++;
    } else if ((c >= 'A' && c <= 'G') || c == 'N' || c == 'P' || c == 'R' || c == 'O' || c == 'L' || c == 'T' || c == 'M') {
        m->cmd = c;
        m->num = 0;
        m->has_num = 0;
        m->dots = 0;
        m->acc = 0;
    }
}

/* The end of the string (SO): the last command. */
static void am_end(vt_music *m)
{
    if (m->cmd != AM_NONE && m->cmd != 'M')
        am_finish(m);
    m->cmd = AM_NONE;
}

/* Does c start a music string after CSI M (else CSI M is Delete Line)? */
static int am_starts(vt_u32 c)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    return (c >= 'A' && c <= 'G') || c == 'N' || c == 'P' || c == 'R' || c == 'T' || c == 'L' || c == 'O' || c == 'M' || c == 'S' || c == '<' || c == '>' || c == 0x0E;
}

vt_u16 vt_note_period(vt_u32 chz, int wave_len)
{
    vt_u32 d, p;
    if (!chz || wave_len <= 0)
        return 0;
    d = chz * (vt_u32)wave_len; /* 395104 * 16 at most: fits */
    p = (3546895UL / d) * 100UL + (3546895UL % d) * 100UL / d;
    if (p < 124)
        p = 124;
    if (p > 65535UL)
        p = 65535UL;
    return (vt_u16)p;
}
