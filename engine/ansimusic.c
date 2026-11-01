/* ansimusic.c: ANSI music strings (the GW-BASIC PLAY language) to notes.
 *
 * #included by vtengine.c before struct vt_term (ENGINE is the one file);
 * it knows nothing of the terminal. The engine's parser catches the
 * introducer (CSI M / CSI N / CSI |) and feeds the string here byte by
 * byte until SO. Rules from GW-BASIC PLAY (GWSTS.ASM) and SyncTERM:
 *   A-G [#|+|-] [len] [.]...  note; len 1-64 (4 = quarter), default L
 *   N n [.]...                note number 0-84: N1 = C in O0, octave
 *                             (n-1) div 12, N34 = A 440, 0 a rest
 *   P / R [len] [.]...        pause
 *   O n (0-6), < >            octave (O2 A = 440, O2 C = 261.63, O3 C = 523)
 *   L n (1-64, above clamps), T n (32-255)  default length, tempo (quarter notes a minute);
 *                             O and T out of range are clamped
 *   MN ML MS                  normal (7/8 sounds), legato (all), staccato (3/4)
 *   MF MB                     foreground / background (a flag for the host)
 * Each dot multiplies the length by 1.5 (C. 1.5x, C.. 2.25x, C... 3.375x).
 * After the CSI M intro, N or n followed by a digit is the note-number
 * command; N not followed by a digit is MN.
 * Spaces and any other byte (CR, LF, quotes, ...) are skipped and the string
 * plays on: real files carry such bytes inside PLAY strings. A ';' marks a
 * SMILE sound-effect block ("MF 7000;.12;200;15;-100"): once one is seen the
 * string plays nothing -- the notes not yet handed over are dropped and the
 * host is not called. Notes go to the host in parts of VT_MUSIC_NOTES, so a
 * ';' after such a flush cannot recall the earlier parts; sound-effect
 * strings are far shorter than that. Playing the effect itself is not done.
 * Portable C89. */

#define AM_NONE 0

typedef struct vt_music {
    vt_u8 tempo, len, oct, style; /* style 0 normal, 1 legato, 2 staccato */
    vt_u8 bg;                     /* MB given: background */
    vt_u8 cmd;                    /* the command waiting for its argument, AM_NONE none */
    vt_u8 has_num, dots;
    int acc;                      /* -1 flat, +1 sharp */
    long num;
    vt_u8 intro_m;                /* CSI M intro, first command not read yet */
    vt_u8 mn_pending;             /* intro M then N: note number if a digit follows */
    vt_u8 silent;                 /* a ';' was seen: a sound-effect block, no notes */
    int n;                        /* notes in the buffer */
    vt_note notes[VT_MUSIC_NOTES];
} vt_music;

/* Octave 6, C to B, in Hz (GW-BASIC NOTTAB). */
static const vt_u16 am_oct6[12] = {
    4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902
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
    /* GW-BASIC: shift right by 6-octave, ADC 0 adds the last bit shifted out */
    c = am_oct6[semi];
    if (oct < 6)
        c = (c >> (6 - oct)) + ((c >> (5 - oct)) & 1);
    c *= 100; /* whole Hz in centi-Hz */
    return c;
}

/* A len note at the tempo, with dots: whole = 240000 / tempo ms; each dot
 * is 1.5 times the length so far. */
static vt_u32 am_ms(const vt_music *m, long len, int dots)
{
    vt_u32 num = 240000UL, den = (vt_u32)m->tempo * (vt_u32)len, ms;
    while (dots-- > 0) {
        num *= 3; /* 240000 * 3^8 fits 32 bits */
        den *= 2;
    }
    ms = num / den;
    return ms > 65535UL ? 65535UL : ms;
}

static void am_add(vt_music *m, vt_u32 chz, vt_u32 ms)
{
    vt_note *nt;
    if (ms == 0 || m->silent || m->n >= VT_MUSIC_NOTES)
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
    long len = m->has_num && m->num >= 1 ? (m->num > 64 ? 64 : m->num) : m->len;
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
        if (m->has_num)
            m->oct = v > 6 ? 6 : (vt_u8)v;
    } else if (c == 'L') {
        if (m->has_num && v >= 1)
            m->len = v > 64 ? 64 : (vt_u8)v;
    } else if (c == 'T') {
        if (m->has_num)
            m->tempo = v < 32 ? 32 : v > 255 ? 255 : (vt_u8)v;
    }
}

/* One byte of the string. */
static void am_byte(vt_music *m, vt_u8 c)
{
    if (c >= 'a' && c <= 'z')
        c = (vt_u8)(c - 'a' + 'A');
    if (m->mn_pending) { /* CSI M N: a digit makes it the note-number command */
        m->mn_pending = 0;
        if (c >= '0' && c <= '9') {
            m->cmd = 'N';
            m->num = 0;
            m->has_num = 0;
            m->dots = 0;
            m->acc = 0;
        } else {
            m->style = 0; /* MN */
        }
    }
    if (m->cmd == 'M') { /* M and its letter */
        m->cmd = AM_NONE;
        if (c == 'N' && m->intro_m) {
            m->intro_m = 0;
            m->mn_pending = 1;
            return;
        }
        m->intro_m = 0;
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
    if (c == ';') { /* SMILE sound effect: no notes from this string */
        m->silent = 1;
        m->n = 0;
        return;
    }
    if (!((c >= 'A' && c <= 'G') || c == 'N' || c == 'P' || c == 'R' || c == 'O' || c == 'L' || c == 'T' || c == 'M' || c == '<' || c == '>'))
        return; /* unknown byte (quote, ...): skipped, the command waiting goes on */
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
    if (m->mn_pending)
        m->style = 0;
    m->mn_pending = 0;
    if (m->cmd != AM_NONE && m->cmd != 'M')
        am_finish(m);
    m->cmd = AM_NONE;
    if (m->silent)
        m->n = 0;
}

/* Does c start a music string after CSI M (else CSI M is Delete Line)? */
static int am_starts(vt_u32 c)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    return (c >= 'A' && c <= 'G') || c == 'N' || c == 'P' || c == 'R' || c == 'T' || c == 'L' || c == 'O' || c == 'M' || c == 'S' || c == '<' || c == '>' || c == 0x0E;
}

/* A new string with the introducer M (CSI M), N (CSI N) or | (CSI |). */
static void am_start(vt_music *m, vt_u8 intro)
{
    m->cmd = intro == 'M' ? 'M' : AM_NONE; /* the CSI's M is the first M: CSI MF, CSI MB */
    m->intro_m = intro == 'M';
    m->mn_pending = m->silent = 0;
    m->n = 0;
    m->num = 0;
    m->has_num = m->dots = 0;
    m->acc = 0;
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
