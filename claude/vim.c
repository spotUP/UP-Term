/* vim -- the input box's vim mode (A4 1.8), Claude Code's subset
 * (code.claude.com/docs/en/interactive-mode, "Vim editor mode"):
 *   modes      Esc / Ctrl+[ to NORMAL; i I a A o O to INSERT
 *   motions    h j k l Space w e b W E B 0 $ ^ gg G f F t T ; , (with counts)
 *   operators  d c y > < with a motion, doubled for lines (dd cc yy >> <<),
 *              and the text objects iw aw iW aW
 *   edits      x s S r D C Y p P J u
 * j / k (and Up / Down) at the first or last line walk the history. Not
 * here: visual mode (v V), '.', the quote and bracket text objects.
 * Portable C89, host-tested (tests/test_claude_tui.c). */
#include <stdlib.h>
#include <string.h>
#include "edit.h"

static int cls(int c, int big)
{
    if (c == ' ' || c == '\t' || c == '\n')
        return 0;
    if (big)
        return 1;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80)
        return 1;
    return 2;
}

static int at(const cl_edit *e, long i)
{
    return i >= 0 && i < e->n ? (unsigned char)e->b[i] : '\n';
}

/* back to a character's first byte */
static long start_of(const cl_edit *e, long i)
{
    while (i > 0 && i < e->n && ((unsigned char)e->b[i] & 0xc0) == 0x80)
        i--;
    return i;
}

static long word_w(const cl_edit *e, long i, int big)
{
    int c = cls(at(e, i), big);
    if (i >= e->n)
        return e->n;
    if (c)
        while (i < e->n && cls(at(e, i), big) == c)
            i++;
    while (i < e->n && !cls(at(e, i), big))
        i++;
    return i;
}

static long word_e(const cl_edit *e, long i, int big)
{
    int c;
    if (i >= e->n)
        return e->n;
    i++;
    while (i < e->n && !cls(at(e, i), big))
        i++;
    if (i >= e->n)
        return start_of(e, e->n - 1 < 0 ? 0 : e->n - 1);
    c = cls(at(e, i), big);
    while (i + 1 < e->n && cls(at(e, i + 1), big) == c)
        i++;
    return start_of(e, i);
}

static long word_b(const cl_edit *e, long i, int big)
{
    int c;
    if (i <= 0)
        return 0;
    i--;
    while (i > 0 && !cls(at(e, i), big))
        i--;
    c = cls(at(e, i), big);
    while (i > 0 && cls(at(e, i - 1), big) == c)
        i--;
    return i;
}

static long first_nonblank(const cl_edit *e, long i)
{
    long s = ed_lstart(e, i), z = ed_lend(e, i);
    while (s < z && (e->b[s] == ' ' || e->b[s] == '\t'))
        s++;
    return s;
}

static long col_of(const cl_edit *e, long i)
{
    long s = ed_lstart(e, i), c = 0;
    while (s < i) {
        s = ed_next(e, s);
        c++;
    }
    return c;
}

static long to_col(const cl_edit *e, long s, long col)
{
    long z = ed_lend(e, s);
    while (col-- > 0 && s < z)
        s = ed_next(e, s);
    return s;
}

/* NORMAL mode's cursor sits on a character, never after a line's last */
static void clamp(cl_edit *e)
{
    if (e->cur > e->n)
        e->cur = e->n;
    if (e->cur == ed_lend(e, e->cur) && e->cur > ed_lstart(e, e->cur))
        e->cur = ed_prev(e, e->cur);
}

static long line_no(const cl_edit *e, long i)
{
    long k, l = 0;
    for (k = 0; k < i && k < e->n; k++)
        l += e->b[k] == '\n';
    return l;
}

static long line_at(const cl_edit *e, long no)
{
    long i = 0;
    while (no > 0 && i < e->n) {
        if (e->b[i] == '\n')
            no--;
        i++;
    }
    return ed_lstart(e, i);
}

static long find_char(const cl_edit *e, long i, int kind, int ch, long count)
{
    long s = ed_lstart(e, i), z = ed_lend(e, i), p = i, found = -1;
    while (count-- > 0) {
        found = -1;
        if (kind == 'f' || kind == 't') {
            long q = ed_next(e, p);
            if (kind == 't' && q < z && (unsigned char)e->b[q] == ch && count == 0 && p == i)
                q = ed_next(e, q);     /* t onto the next match when already before one */
            for (; q < z; q = ed_next(e, q))
                if ((unsigned char)e->b[q] == ch) {
                    found = q;
                    break;
                }
        } else {
            long q = p;
            while (q > s) {
                q = ed_prev(e, q);
                if ((unsigned char)e->b[q] == ch) {
                    found = q;
                    break;
                }
            }
        }
        if (found < 0)
            return -1;
        p = found;
    }
    if (kind == 't')
        return ed_prev(e, found);
    if (kind == 'T')
        return ed_next(e, found);
    return found;
}

static void set_register(cl_edit *e, long a, long z, int lines)
{
    char *d = (char *)malloc((size_t)(z - a) + 1);
    if (!d)
        return;
    memcpy(d, e->b + a, (size_t)(z - a));
    d[z - a] = 0;
    free(e->kill);
    e->kill = d;
    e->kill_lines = lines;
}

static void insert_mode(cl_edit *e)
{
    e->vim = VIM_INSERT;
}

void vim_escape(cl_edit *e)
{
    e->vim = VIM_NORMAL;
    if (e->cur > ed_lstart(e, e->cur))
        e->cur = ed_prev(e, e->cur);
    e->vcount = e->vopcount = e->vop = e->vpend = 0;
}

void ed_set_vim(cl_edit *e, int on)
{
    e->vim = on ? VIM_INSERT : VIM_OFF;
    e->vcount = e->vopcount = e->vop = e->vpend = 0;
}

int vim_idle(const cl_edit *e)
{
    return !e->vcount && !e->vop && !e->vpend;
}

/* the lines a..z (offsets inside them) as a range, with one newline */
static void line_range(const cl_edit *e, long p, long q, long *a, long *z)
{
    long lo = p < q ? p : q, hi = p < q ? q : p;
    *a = ed_lstart(e, lo);
    *z = ed_lend(e, hi);
    if (*z < e->n)
        (*z)++;
    else if (*a > 0)
        (*a)--;
}

static void shift(cl_edit *e, long p, long q, int right)
{
    long lo = p < q ? p : q, hi = p < q ? q : p, nlines = line_no(e, hi) - line_no(e, lo) + 1;
    long s = ed_lstart(e, lo), k;
    for (k = 0; k < nlines; k++) {
        if (right) {
            if (s < ed_lend(e, s)) {
                e->cur = s;
                ed_insert(e, "  ", 2);
            }
        } else {
            long z = s;
            while (z < s + 2 && z < e->n && e->b[z] == ' ')
                z++;
            ed_cut(e, s, z, 0);
        }
        s = ed_lend(e, s);
        if (s >= e->n)
            break;
        s++;
    }
    e->cur = first_nonblank(e, ed_lstart(e, lo));
}

/* an operator over [a, z) */
static void apply(cl_edit *e, int op, long a, long z, int lines)
{
    if (z < a) {
        long t = a;
        a = z;
        z = t;
    }
    if (op == 'y') {
        set_register(e, a, z, lines);
        e->cur = lines ? first_nonblank(e, a) : a;
        return;
    }
    ed_snap(e);
    if (op == 'c' && lines) {
        /* cc: the lines' text goes, one empty line stays */
        long s = ed_lstart(e, a), t = ed_lend(e, z > a ? z - 1 : a);
        set_register(e, a, z, 1);
        ed_cut(e, s, t, 0);
        insert_mode(e);
        return;
    }
    set_register(e, a, z, lines);
    ed_cut(e, a, z, 0);
    if (op == 'c') {
        insert_mode(e);
        return;
    }
    if (lines)
        e->cur = first_nonblank(e, e->cur < e->n ? e->cur : ed_lstart(e, e->n));
    clamp(e);
}

/* A motion: 1 with *to set (*incl: the target character is included,
 * *lw: whole lines), 0 when c is no motion, -1 when it cannot move. */
static int motion(cl_edit *e, int c, int ch, long count, int op, long *to, int *incl, int *lw)
{
    long i = e->cur, k;
    *incl = 0;
    *lw = 0;
    switch (c) {
    case 'h':
        for (k = 0; k < count && i > ed_lstart(e, e->cur); k++)
            i = ed_prev(e, i);
        break;
    case 'l':
    case ' ': {
        long z = ed_lend(e, e->cur);
        for (k = 0; k < count && i < z; k++)
            i = ed_next(e, i);
        if (!op && i == z && i > ed_lstart(e, i))
            i = ed_prev(e, i);
        break;
    }
    case 'j':
    case 'k': {
        long ln = line_no(e, i), last = line_no(e, e->n), tl = c == 'j' ? ln + count : ln - count;
        if (tl < 0 || tl > last) {
            if (op)
                tl = tl < 0 ? 0 : last;
            else
                return -1;
        }
        i = to_col(e, line_at(e, tl), col_of(e, e->cur));
        *lw = 1;
        break;
    }
    case 'w':
    case 'W':
        if (op == 'c' && cls(at(e, i), c == 'W')) {
            /* cw changes to the word's end, as vim's */
            for (k = 0; k < count; k++) {
                if (k == 0) {
                    int wc = cls(at(e, i), c == 'W');
                    while (i + 1 < e->n && cls(at(e, i + 1), c == 'W') == wc)
                        i++;
                    i = start_of(e, i);
                } else {
                    i = word_e(e, i, c == 'W');
                }
            }
            *incl = 1;
            break;
        }
        for (k = 0; k < count; k++)
            i = word_w(e, i, c == 'W');
        if (op && i > ed_lend(e, e->cur))
            i = ed_lend(e, e->cur);     /* dw at a line's last word stops at its end */
        break;
    case 'e':
    case 'E':
        for (k = 0; k < count; k++)
            i = word_e(e, i, c == 'E');
        *incl = 1;
        break;
    case 'b':
    case 'B':
        for (k = 0; k < count; k++)
            i = word_b(e, i, c == 'B');
        break;
    case '0':
        i = ed_lstart(e, i);
        break;
    case '$':
        i = ed_lend(e, i);
        if (!op && i > ed_lstart(e, i))
            i = ed_prev(e, i);
        else if (op)
            *incl = 0;
        break;
    case '^':
        i = first_nonblank(e, i);
        break;
    case 'G':
        i = count > 1 || e->vcount ? line_at(e, count - 1) : ed_lstart(e, e->n);
        i = first_nonblank(e, i);
        *lw = 1;
        break;
    case 'g':                           /* gg */
        i = first_nonblank(e, line_at(e, count > 1 ? count - 1 : 0));
        *lw = 1;
        break;
    case 'f':
    case 'F':
    case 't':
    case 'T':
        i = find_char(e, i, c, ch, count);
        if (i < 0)
            return -1;
        *incl = c == 'f' || c == 't';
        e->vlastf = c;
        e->vlastc = ch;
        break;
    case ';':
    case ',': {
        int kind = e->vlastf;
        if (!kind)
            return -1;
        if (c == ',')
            kind = kind == 'f' ? 'F' : kind == 'F' ? 'f' : kind == 't' ? 'T' : 't';
        i = find_char(e, i, kind, e->vlastc, count);
        if (i < 0)
            return -1;
        *incl = kind == 'f' || kind == 't';
        break;
    }
    default:
        return 0;
    }
    *to = i;
    return 1;
}

/* the word under the cursor: iw / aw (big: iW / aW) */
static void text_object(cl_edit *e, int around, int big, long *a, long *z)
{
    long i = e->cur;
    int c = cls(at(e, i), big);
    *a = i;
    *z = i;
    while (*a > ed_lstart(e, i) && cls(at(e, *a - 1), big) == c)
        (*a)--;
    while (*z < ed_lend(e, i) && cls(at(e, *z), big) == c)
        (*z)++;
    if (around) {
        long z2 = *z;
        while (z2 < ed_lend(e, i) && !cls(at(e, z2), big))
            z2++;
        if (z2 > *z)
            *z = z2;
        else
            while (*a > ed_lstart(e, i) && !cls(at(e, *a - 1), big))
                (*a)--;
    }
}

static void reset(cl_edit *e)
{
    e->vcount = e->vopcount = e->vop = e->vpend = 0;
}

static void paste(cl_edit *e, int before, long count)
{
    long k, len;
    if (!e->kill || !*e->kill)
        return;
    len = (long)strlen(e->kill);
    ed_snap(e);
    if (e->kill_lines) {
        /* whole lines go below (p) or above (P) the cursor's line */
        long body = e->kill[len - 1] == '\n' ? len - 1 : len, start;
        if (before) {
            e->cur = ed_lstart(e, e->cur);
            start = e->cur;
            for (k = 0; k < count; k++) {
                ed_insert(e, e->kill, body);
                ed_insert(e, "\n", 1);
            }
        } else {
            e->cur = ed_lend(e, e->cur);
            start = e->cur + 1;
            for (k = 0; k < count; k++) {
                ed_insert(e, "\n", 1);
                ed_insert(e, e->kill, body);
            }
        }
        e->cur = first_nonblank(e, start);
        return;
    }
    if (!before && e->cur < ed_lend(e, e->cur))
        e->cur = ed_next(e, e->cur);
    for (k = 0; k < count; k++)
        ed_insert(e, e->kill, len);
    e->cur = ed_prev(e, e->cur);
}

static void join(cl_edit *e, long count)
{
    long k;
    ed_snap(e);
    for (k = 0; k < (count > 1 ? count - 1 : 1); k++) {
        long z = ed_lend(e, e->cur), s;
        if (z >= e->n)
            break;
        s = z + 1;
        while (s < e->n && (e->b[s] == ' ' || e->b[s] == '\t'))
            s++;
        ed_cut(e, z, s, 0);
        if (z > ed_lstart(e, z) && z < e->n && e->b[z] != '\n')
            ed_insert(e, " ", 1);
        e->cur = z;
    }
}

int vim_normal(cl_edit *e, const cl_key *k)
{
    long count, to;
    int c, incl, lw, r;
    if (k->k == K_ESC) {
        if (vim_idle(e))
            return 0;               /* the screen's: interrupt, Esc Esc */
        reset(e);
        return 1;
    }
    if (k->k == K_BS) {
        cl_key h;
        memset(&h, 0, sizeof(h));
        h.k = K_CHAR;
        h.ch = 'h';
        return vim_normal(e, &h);
    }
    if (k->k == K_DEL) {
        cl_key x;
        memset(&x, 0, sizeof(x));
        x.k = K_CHAR;
        x.ch = 'x';
        return vim_normal(e, &x);
    }
    if (k->k == K_ENTER || k->k == K_TAB || k->k == K_BTAB)
        return 0;
    if (k->k == K_UP || k->k == K_DOWN) {
        cl_key m;
        memset(&m, 0, sizeof(m));
        m.k = K_CHAR;
        m.ch = k->k == K_UP ? 'k' : 'j';
        return vim_normal(e, &m);
    }
    if (k->k != K_CHAR)
        return -1;
    c = (int)k->ch;
    count = (e->vcount ? e->vcount : 1) * (e->vopcount ? e->vopcount : 1);
    if (e->vpend) {
        int p = e->vpend;
        e->vpend = 0;
        if (p == 'r') {
            long i = e->cur, n;
            char u[2];
            for (n = 0, i = e->cur; n < count && i < ed_lend(e, e->cur); n++)
                i = ed_next(e, i);
            if (n == count && c >= 0x20 && c < 0x7f) {
                ed_snap(e);
                ed_cut(e, e->cur, i, 0);
                u[0] = (char)c;
                for (n = 0; n < count; n++)
                    ed_insert(e, u, 1);
                e->cur = ed_prev(e, e->cur);
            }
            reset(e);
            return 1;
        }
        if (p == 'i' || p == 'a') {
            long a, z;
            if (c == 'w' || c == 'W') {
                text_object(e, p == 'a', c == 'W', &a, &z);
                apply(e, e->vop, a, z, 0);
            }
            reset(e);
            return 1;
        }
        r = motion(e, p, c, count, e->vop, &to, &incl, &lw);
        goto moved;
    }
    if ((c >= '1' && c <= '9') || (c == '0' && e->vcount)) {
        if (e->vcount < 10000)
            e->vcount = e->vcount * 10 + (c - '0');
        return 1;
    }
    if (c == 'f' || c == 'F' || c == 't' || c == 'T' || c == 'r') {
        e->vpend = c;
        return 1;
    }
    if (c == 'g') {
        e->vpend = 'g';
        return 1;
    }
    if (e->vop && (c == 'i' || c == 'a')) {
        e->vpend = c;
        return 1;
    }
    if (e->vop) {
        if (c == e->vop) {
            /* dd cc yy >> <<: count lines from here */
            long last = line_no(e, e->n), ln = line_no(e, e->cur), tl = ln + count - 1, a, z;
            int op = e->vop;
            if (tl > last)
                tl = last;
            to = line_at(e, tl);
            if (op == '>' || op == '<') {
                ed_snap(e);
                shift(e, e->cur, to, op == '>');
            } else {
                line_range(e, e->cur, to, &a, &z);
                apply(e, op, a, z, 1);
            }
            reset(e);
            return 1;
        }
    }
    r = motion(e, c, 0, count, e->vop, &to, &incl, &lw);
moved:
    if (r != 0) {
        int op = e->vop;
        if (r < 0) {
            /* j / k past the ends: the history, as Up / Down */
            if (!op && (c == 'j' || c == 'k'))
                ed_hist_go(e, e->hpos + (c == 'j' ? 1 : -1));
            if (e->vim == VIM_NORMAL)
                clamp(e);
            reset(e);
            return 1;
        }
        if (!op) {
            e->cur = to;
            clamp(e);
        } else if (op == '>' || op == '<') {
            ed_snap(e);
            shift(e, e->cur, to, op == '>');
        } else if (lw) {
            long a, z;
            line_range(e, e->cur, to, &a, &z);
            apply(e, op, a, z, 1);
        } else {
            long a = e->cur < to ? e->cur : to, z = e->cur < to ? to : e->cur;
            if (incl && z < e->n)
                z = ed_next(e, z);
            apply(e, op, a, z, 0);
        }
        reset(e);
        return 1;
    }
    /* operators and commands */
    switch (c) {
    case 'd':
    case 'c':
    case 'y':
    case '>':
    case '<':
        e->vop = c;
        e->vopcount = e->vcount;
        e->vcount = 0;
        return 1;
    case 'i':
        ed_snap(e);
        insert_mode(e);
        break;
    case 'a':
        ed_snap(e);
        if (e->cur < ed_lend(e, e->cur))
            e->cur = ed_next(e, e->cur);
        insert_mode(e);
        break;
    case 'A':
        ed_snap(e);
        e->cur = ed_lend(e, e->cur);
        insert_mode(e);
        break;
    case 'I':
        ed_snap(e);
        e->cur = first_nonblank(e, e->cur);
        insert_mode(e);
        break;
    case 'o':
        ed_snap(e);
        e->cur = ed_lend(e, e->cur);
        ed_insert(e, "\n", 1);
        insert_mode(e);
        break;
    case 'O':
        ed_snap(e);
        e->cur = ed_lstart(e, e->cur);
        ed_insert(e, "\n", 1);
        e->cur--;
        insert_mode(e);
        break;
    case 'x':
    case 's': {
        long z = e->cur, n;
        for (n = 0; n < count && z < ed_lend(e, e->cur); n++)
            z = ed_next(e, z);
        if (z > e->cur || c == 's')
            apply(e, c == 'x' ? 'd' : 'c', e->cur, z, 0);
        break;
    }
    case 'S': {
        long a, z;
        line_range(e, e->cur, e->cur, &a, &z);
        apply(e, 'c', a, z, 1);
        break;
    }
    case 'D':
    case 'C':
        apply(e, c == 'D' ? 'd' : 'c', e->cur, ed_lend(e, e->cur), 0);
        break;
    case 'Y': {
        long a, z;
        line_range(e, e->cur, line_at(e, line_no(e, e->cur) + count - 1), &a, &z);
        apply(e, 'y', a, z, 1);
        break;
    }
    case 'p':
    case 'P':
        paste(e, c == 'P', count);
        break;
    case 'J':
        join(e, count);
        break;
    case 'u':
        ed_undo(e);
        clamp(e);
        break;
    case '/':
        reset(e);
        return 0;                   /* the screen's: history search */
    default:
        break;                      /* v V . and the rest: not here */
    }
    reset(e);
    return 1;
}
