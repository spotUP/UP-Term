/* vim -- the input box's vim mode (A4 1.8), Claude Code's set
 * (code.claude.com/docs/en/interactive-mode, "Vim editor mode"):
 *   modes      Esc / Ctrl+[ to NORMAL; i I a A o O to INSERT; v V to VISUAL
 *   motions    h j k l Space w e b W E B 0 $ ^ gg G f F t T ; , (with counts)
 *   operators  d c y > < with a motion, doubled for lines (dd cc yy >> <<),
 *              and the text objects iw aw iW aW i" a" i' a' i( a( i[ a[
 *              i{ a{ (the closing bracket, b and B name them too)
 *   edits      x s S r D C Y p P J u, and '.' repeats the last change
 *   visual     motions and text objects extend the selection; d x y c s p
 *              r ~ u U > < J act on it; o swaps its ends; v / V switch
 *              between character- and line-wise or leave
 * j / k (and Up / Down) at the first or last line walk the history.
 * '.' replays the keys of the last change (a count typed before '.'
 * replaces the change's own); a visual change repeats over the same
 * extent from the cursor, as vim's does.
 * Portable C89, host-tested (tests/test_claude_tui.c). */
#include <stdlib.h>
#include <string.h>
#include "edit.h"

static void unrecord(cl_edit *e);
static void replay(cl_edit *e, long count);
static void rec_extent(cl_edit *e);
static void rec_key2(cl_edit *e, int c1, int c2);

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
    vim_after(e);                   /* an INSERT a command began: the change is complete */
}

int vim_remap(cl_edit *e, const cl_key *k)
{
    int i, c = k->k == K_CHAR ? (int)k->ch : -1, first = e->vrp;
    unsigned long since = e->now_ms - e->vrp_ms;
    e->vrp = 0;
    if (c < 0)
        return 0;
    if (first && since <= 1000 && e->cur > 0 && (unsigned char)e->b[e->cur - 1] == first)
        for (i = 0; e->vremap[i] && e->vremap[i + 1]; i += 2)
            if ((unsigned char)e->vremap[i] == first && (unsigned char)e->vremap[i + 1] == c) {
                ed_cut(e, e->cur - 1, e->cur, 0);   /* the pending first key goes */
                vim_escape(e);
                return 1;
            }
    for (i = 0; e->vremap[i] && e->vremap[i + 1]; i += 2)
        if ((unsigned char)e->vremap[i] == c) {
            e->vrp = c;             /* typed now; a second key within a second remaps */
            e->vrp_ms = e->now_ms;
            break;
        }
    return 0;
}

void ed_set_vim(cl_edit *e, int on)
{
    e->vim = on ? VIM_INSERT : VIM_OFF;
    e->vcount = e->vopcount = e->vop = e->vpend = 0;
    e->vrecon = 0;
}

int vim_idle(const cl_edit *e)
{
    return !e->vcount && !e->vop && !e->vpend;
}

const char *vim_label(const cl_edit *e)
{
    switch (e->vim) {
    case VIM_INSERT:
        return "-- INSERT --";
    case VIM_VISUAL:
        return "-- VISUAL --";
    case VIM_VLINE:
        return "-- VISUAL LINE --";
    default:
        return 0;
    }
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
    e->cur = a;
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

/* the word under the cursor: iw / aw (big: iW / aW); a count takes
 * that many (iw: words and the blanks between count apart) */
static void word_object(cl_edit *e, int around, int big, long count, long *a, long *z)
{
    long i = e->cur, end = ed_lend(e, i), k;
    int c = cls(at(e, i), big);
    *a = i;
    *z = i;
    while (*a > ed_lstart(e, i) && cls(at(e, *a - 1), big) == c)
        (*a)--;
    while (*z < end && cls(at(e, *z), big) == c)
        (*z)++;
    for (k = 1; k < count && *z < end; k++) {
        if (around) {
            /* aw: the next word with its blanks */
            while (*z < end && !cls(at(e, *z), big))
                (*z)++;
        }
        c = cls(at(e, *z), big);
        while (*z < end && cls(at(e, *z), big) == c)
            (*z)++;
    }
    if (around) {
        long z2 = *z;
        while (z2 < end && !cls(at(e, z2), big))
            z2++;
        if (z2 > *z)
            *z = z2;
        else
            while (*a > ed_lstart(e, i) && !cls(at(e, *a - 1), big))
                (*a)--;
    }
}

/* a quote character at i (not one escaped with a backslash) */
static int is_quote(const cl_edit *e, long s, long i, int q)
{
    return e->b[i] == q && !(i > s && e->b[i - 1] == '\\');
}

static long next_quote(const cl_edit *e, long i, long end, int q)
{
    long s = ed_lstart(e, i);
    for (; i < end; i++)
        if (is_quote(e, s, i, q))
            return i;
    return -1;
}

static long prev_quote(const cl_edit *e, long s, long i, int q)
{
    while (--i >= s)
        if (is_quote(e, s, i, q))
            return i;
    return -1;
}

/* i" a" (and ' `), as vim finds them on the cursor's line: on a quote,
 * the pair it opens or closes (counted from the line's start); else the
 * quotes before and after the cursor, or the first two after it. a"
 * takes the blanks after the closing quote (or, none, before the
 * opening one). 1 found. */
static int quote_object(cl_edit *e, int around, int q, long *a, long *z)
{
    long s = ed_lstart(e, e->cur), end = ed_lend(e, e->cur), i, open = -1, close = -1;
    if (e->cur < end && e->b[e->cur] == q) {
        /* on a quote: opening or closing, by the quotes before it */
        int before = 0;
        for (i = s; i < e->cur; i++)
            before += is_quote(e, s, i, q);
        if (before % 2 == 0) {
            open = e->cur;
            close = next_quote(e, open + 1, end, q);
        } else {
            close = e->cur;
            open = prev_quote(e, s, close, q);
        }
    } else {
        /* the quote before the cursor and the one after; none before: the
         * first two after it */
        open = prev_quote(e, s, e->cur, q);
        close = next_quote(e, e->cur, end, q);
        if (open < 0 && close >= 0) {
            open = close;
            close = next_quote(e, open + 1, end, q);
        }
    }
    if (open < 0 || close < 0)
        return 0;
    if (!around) {
        *a = open + 1;
        *z = close;
        return 1;
    }
    *a = open;
    *z = close + 1;
    if (*z < end && (e->b[*z] == ' ' || e->b[*z] == '\t')) {
        while (*z < end && (e->b[*z] == ' ' || e->b[*z] == '\t'))
            (*z)++;
    } else {
        while (*a > s && (e->b[*a - 1] == ' ' || e->b[*a - 1] == '\t'))
            (*a)--;
    }
    return 1;
}

/* i( a( i[ a[ i{ a{: the count-th pair of brackets around the cursor
 * (over lines too); a( takes the brackets. 1 found. */
static int bracket_object(cl_edit *e, int around, int open, int close, long count, long *a, long *z)
{
    long i = e->cur, depth, o = -1, c;
    if (i < e->n && e->b[i] == close)
        i--;                            /* on the closing one: its pair */
    else if (i < e->n && e->b[i] == open && count == 1) {
        o = i;
        count = 0;
    }
    for (; count > 0; count--) {
        /* back to the next unmatched opening bracket */
        depth = 0;
        if (o >= 0)
            i = o - 1;
        o = -1;
        for (; i >= 0; i--) {
            if (e->b[i] == close)
                depth++;
            else if (e->b[i] == open) {
                if (!depth) {
                    o = i;
                    break;
                }
                depth--;
            }
        }
        if (o < 0)
            return 0;
    }
    depth = 0;
    for (c = o + 1; c < e->n; c++) {
        if (e->b[c] == open)
            depth++;
        else if (e->b[c] == close) {
            if (!depth)
                break;
            depth--;
        }
    }
    if (c >= e->n)
        return 0;
    *a = around ? o : o + 1;
    *z = around ? c + 1 : c;
    return 1;
}

/* a text object named by its key (after i / a): 1 with [*a, *z) */
static int text_object(cl_edit *e, int around, int key, long count, long *a, long *z)
{
    switch (key) {
    case 'w':
    case 'W':
        word_object(e, around, key == 'W', count, a, z);
        return *z > *a;
    case '"':
    case '\'':
    case '`':
        return quote_object(e, around, key, a, z);
    case '(':
    case ')':
    case 'b':
        return bracket_object(e, around, '(', ')', count, a, z);
    case '[':
    case ']':
        return bracket_object(e, around, '[', ']', count, a, z);
    case '{':
    case '}':
    case 'B':
        return bracket_object(e, around, '{', '}', count, a, z);
    case '<':
    case '>':
        return bracket_object(e, around, '<', '>', count, a, z);
    default:
        return 0;
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
    e->vcmdcount = e->vcount || e->vopcount ? count : 0;
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
            if (text_object(e, p == 'a', c, count, &a, &z))
                apply(e, e->vop, a, z, 0);
            reset(e);
            return 1;
        }
        r = motion(e, p, c, count, e->vop, &to, &incl, &lw);
        goto moved;
    }
    if ((c >= '1' && c <= '9') || (c == '0' && e->vcount)) {
        if (e->vcount < 10000)
            e->vcount = e->vcount * 10 + (c - '0');
        unrecord(e);                /* '.' keeps the count apart */
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
    case 'v':
    case 'V':
        e->vim = c == 'v' ? VIM_VISUAL : VIM_VLINE;
        e->vanchor = e->cur;
        break;
    case '.': {
        long n = e->vcount ? count : e->vdotcount;
        e->vrecon = 0;              /* '.' is no change of its own */
        reset(e);
        replay(e, n);
        return 1;
    }
    case '/':
        reset(e);
        return 0;                   /* the screen's: history search */
    default:
        break;
    }
    reset(e);
    return 1;
}

/* ---- visual mode ---- */

int vim_selection(const cl_edit *e, long *a, long *z)
{
    long lo, hi, an = e->vanchor <= e->n ? e->vanchor : e->n;
    if (e->vim != VIM_VISUAL && e->vim != VIM_VLINE)
        return 0;
    lo = an < e->cur ? an : e->cur;
    hi = an < e->cur ? e->cur : an;
    if (e->vim == VIM_VLINE) {
        line_range(e, lo, hi, a, z);
        return 1;
    }
    *a = lo;
    *z = hi < e->n ? ed_next(e, hi) : e->n;
    return 1;
}

static void leave_visual(cl_edit *e)
{
    e->vim = VIM_NORMAL;
    clamp(e);
}

/* the selection's case: '~' toggled, 'u' lower, 'U' upper (ASCII) */
static void case_range(cl_edit *e, long a, long z, int how)
{
    long i;
    ed_snap(e);
    for (i = a; i < z; i++) {
        int ch = (unsigned char)e->b[i];
        int lo = ch >= 'a' && ch <= 'z', up = ch >= 'A' && ch <= 'Z';
        if ((how == '~' || how == 'U') && lo)
            e->b[i] = (char)(ch - 32);
        else if ((how == '~' || how == 'u') && up)
            e->b[i] = (char)(ch + 32);
    }
}

/* every character of the selection but the line ends becomes ch */
static void replace_range(cl_edit *e, long a, long z, int ch)
{
    long i = a;
    char u[1];
    u[0] = (char)ch;
    ed_snap(e);
    while (i < z && i < e->n) {
        long l;
        if (e->b[i] == '\n') {
            i++;
            continue;
        }
        l = ed_next(e, i) - i;
        ed_cut(e, i, i + l, 0);
        e->cur = i;
        ed_insert(e, u, 1);
        z -= l - 1;                 /* a multi-byte character became one byte */
        i++;
    }
    e->cur = a;
}

/* p over a selection: the register's text takes its place, and the
 * selection goes into the register (as vim's) */
static void put_over(cl_edit *e, long a, long z, int lines)
{
    char *reg;
    int rl = e->kill_lines;
    long len, body, n0 = e->n;
    if (!e->kill || !*e->kill)
        return;
    len = (long)strlen(e->kill);
    reg = (char *)malloc((size_t)len + 1);
    if (!reg)
        return;
    memcpy(reg, e->kill, (size_t)len + 1);
    body = reg[len - 1] == '\n' ? len - 1 : len;
    ed_snap(e);
    set_register(e, a, z, lines);
    ed_cut(e, a, z, 0);
    e->cur = a;
    if (lines) {
        if (a == 0 && z >= n0) {
            /* every line: the register's are all there is */
            ed_insert(e, reg, body);
            e->cur = 0;
        } else if (z >= n0) {
            /* the last lines: line_range took the newline before them */
            ed_insert(e, "\n", 1);
            ed_insert(e, reg, body);
            e->cur = a + 1;
        } else {
            ed_insert(e, reg, body);
            ed_insert(e, "\n", 1);
            e->cur = a;
        }
        e->cur = first_nonblank(e, e->cur);
    } else {
        if (rl)
            ed_insert(e, "\n", 1);
        ed_insert(e, reg, rl ? body : len);
        if (rl)
            ed_insert(e, "\n", 1);
        e->cur = e->cur > a ? ed_prev(e, e->cur) : a;
    }
    free(reg);
}

int vim_visual(cl_edit *e, const cl_key *k)
{
    long count, to, a, z;
    int c, incl, lw, r, lines = e->vim == VIM_VLINE;
    if (k->k == K_ESC) {
        if (!vim_idle(e))
            reset(e);
        else
            leave_visual(e);
        return 1;
    }
    if (k->k == K_ENTER || k->k == K_TAB || k->k == K_BTAB)
        return 0;
    if (k->k == K_UP || k->k == K_DOWN || k->k == K_LEFT || k->k == K_RIGHT || k->k == K_BS || k->k == K_DEL) {
        cl_key m;
        memset(&m, 0, sizeof(m));
        m.k = K_CHAR;
        m.ch = k->k == K_UP ? 'k' : k->k == K_DOWN ? 'j' : k->k == K_RIGHT ? 'l' : k->k == K_DEL ? 'x' : 'h';
        return vim_visual(e, &m);
    }
    if (k->k != K_CHAR)
        return -1;
    c = (int)k->ch;
    count = e->vcount ? e->vcount : 1;
    if (e->vpend) {
        int p = e->vpend;
        e->vpend = 0;
        if (p == 'r') {
            if (c >= 0x20 && c < 0x7f && vim_selection(e, &a, &z)) {
                rec_extent(e);
                rec_key2(e, 'r', c);
                leave_visual(e);
                replace_range(e, a, z, c);
                clamp(e);
            }
            reset(e);
            return 1;
        }
        if (p == 'i' || p == 'a') {
            if (text_object(e, p == 'a', c, count, &a, &z) && z > a) {
                e->vim = VIM_VISUAL;
                e->vanchor = a;
                e->cur = start_of(e, z - 1);
            }
            reset(e);
            return 1;
        }
        r = motion(e, p, c, count, 0, &to, &incl, &lw);
        if (r > 0)
            e->cur = to;
        reset(e);
        return 1;
    }
    if ((c >= '1' && c <= '9') || (c == '0' && e->vcount)) {
        if (e->vcount < 10000)
            e->vcount = e->vcount * 10 + (c - '0');
        return 1;
    }
    if (c == 'f' || c == 'F' || c == 't' || c == 'T' || c == 'r' || c == 'g' || c == 'i' || c == 'a') {
        e->vpend = c;
        return 1;
    }
    r = motion(e, c, 0, count, 0, &to, &incl, &lw);
    if (r != 0) {
        if (r > 0)
            e->cur = to;
        reset(e);
        return 1;
    }
    reset(e);
    switch (c) {
    case 'v':
    case 'V':
        if ((c == 'v') == (e->vim == VIM_VISUAL))
            leave_visual(e);
        else
            e->vim = c == 'v' ? VIM_VISUAL : VIM_VLINE;
        return 1;
    case 'o': {
        long t = e->vanchor <= e->n ? e->vanchor : e->n;
        e->vanchor = e->cur;
        e->cur = t;
        return 1;
    }
    case 'd':
    case 'x':
    case 'X':
    case 'D':
    case 'y':
    case 'Y':
    case 'c':
    case 's':
    case 'S':
    case 'C':
    case 'R': {
        int op = c == 'y' || c == 'Y' ? 'y' : c == 'c' || c == 's' || c == 'S' || c == 'C' || c == 'R' ? 'c' : 'd';
        if (c == 'X' || c == 'D' || c == 'Y' || c == 'S' || c == 'C' || c == 'R') {
            lines = 1;              /* the upper-case ones take whole lines */
            e->vim = VIM_VLINE;
        }
        vim_selection(e, &a, &z);
        if (op != 'y') {
            rec_extent(e);
            rec_key2(e, c, 0);
        }
        e->vim = VIM_NORMAL;
        apply(e, op, a, z, lines);
        if (op == 'y')
            clamp(e);
        return 1;
    }
    case 'p':
    case 'P':
        vim_selection(e, &a, &z);
        rec_extent(e);
        rec_key2(e, c, 0);
        e->vim = VIM_NORMAL;
        put_over(e, a, z, lines);
        clamp(e);
        return 1;
    case '~':
    case 'u':
    case 'U': {
        long an = e->vanchor <= e->n ? e->vanchor : e->n, lo = an < e->cur ? an : e->cur;
        vim_selection(e, &a, &z);
        rec_extent(e);
        rec_key2(e, c, 0);
        case_range(e, a, z, c);
        leave_visual(e);
        e->cur = lines ? first_nonblank(e, a) : lo;
        clamp(e);
        return 1;
    }
    case '>':
    case '<': {
        long p = e->vanchor <= e->n ? e->vanchor : e->n, q = e->cur;
        rec_extent(e);
        rec_key2(e, c, 0);
        leave_visual(e);
        ed_snap(e);
        shift(e, p, q, c == '>');
        return 1;
    }
    case 'J': {
        long p = e->vanchor <= e->n ? e->vanchor : e->n, lo = p < e->cur ? p : e->cur, hi = p < e->cur ? e->cur : p;
        long nl = line_no(e, hi) - line_no(e, lo) + 1;
        rec_extent(e);
        rec_key2(e, c, 0);
        leave_visual(e);
        e->cur = lo;
        join(e, nl < 2 ? 2 : nl);
        return 1;
    }
    default:
        return 1;
    }
}

/* ---- '.' ----
 * The keys of a change are recorded as it is typed: from the key that
 * starts a command in NORMAL mode (counts left out: '.' keeps the count
 * apart) to its end, or, when it began an INSERT, to the Esc. A change
 * made in visual mode records its extent (REC_EXTENT: the mode, lines
 * down, and the characters on one line or the last line's column)
 * followed by its keys. A record that changed nothing is dropped. */

#define REC_EXTENT 0x7f

static int rec_room(cl_edit *e, long need)
{
    char *nb;
    long nc;
    if (e->nvrec + need <= e->cvrec)
        return 1;
    nc = e->cvrec ? e->cvrec : 64;
    while (nc < e->nvrec + need)
        nc *= 2;
    nb = (char *)realloc(e->vrec, (size_t)nc);
    if (!nb) {
        e->vrecon = 0;
        return 0;
    }
    e->vrec = nb;
    e->cvrec = nc;
    return 1;
}

static void put4(char *p, unsigned long v)
{
    p[0] = (char)(v >> 24);
    p[1] = (char)(v >> 16);
    p[2] = (char)(v >> 8);
    p[3] = (char)v;
}

static unsigned long get4(const char *p)
{
    return ((unsigned long)(unsigned char)p[0] << 24) | ((unsigned long)(unsigned char)p[1] << 16) |
           ((unsigned long)(unsigned char)p[2] << 8) | (unsigned long)(unsigned char)p[3];
}

/* one key: kind, mods, the character (4 bytes); a paste also its length
 * (4 bytes) and text */
static void rec_put(cl_edit *e, int kind, int mods, unsigned long ch, const char *text, long n)
{
    long need = 6 + (kind == K_PASTE ? 4 + n : 0);
    if (!rec_room(e, need))
        return;
    e->vreclast = e->nvrec;
    e->vrec[e->nvrec] = (char)kind;
    e->vrec[e->nvrec + 1] = (char)mods;
    put4(e->vrec + e->nvrec + 2, ch);
    if (kind == K_PASTE) {
        put4(e->vrec + e->nvrec + 6, (unsigned long)n);
        memcpy(e->vrec + e->nvrec + 10, text, (size_t)n);
    }
    e->nvrec += need;
}

static void unrecord(cl_edit *e)
{
    if (e->vrecon && !e->vreplay && e->nvrec > e->vreclast)
        e->nvrec = e->vreclast;
}

static void rec_extent(cl_edit *e)
{
    long an = e->vanchor <= e->n ? e->vanchor : e->n, lo = an < e->cur ? an : e->cur, hi = an < e->cur ? e->cur : an;
    long dl = line_no(e, hi) - line_no(e, lo), dc, i;
    if (!e->vrecon || e->vreplay)
        return;
    if (dl == 0)
        for (dc = 1, i = lo; i < hi; i = ed_next(e, i))
            dc++;
    else
        dc = col_of(e, hi);
    e->nvrec = 0;
    e->vcmdcount = 0;
    if (!rec_room(e, 10))
        return;
    e->vrec[0] = (char)REC_EXTENT;
    e->vrec[1] = (char)e->vim;
    put4(e->vrec + 2, (unsigned long)dl);
    put4(e->vrec + 6, (unsigned long)dc);
    e->nvrec = 10;
    e->vreclast = 10;
}

static void rec_key2(cl_edit *e, int c1, int c2)
{
    if (!e->vrecon || e->vreplay)
        return;
    rec_put(e, K_CHAR, 0, (unsigned long)c1, 0, 0);
    if (c2)
        rec_put(e, K_CHAR, 0, (unsigned long)c2, 0, 0);
}

void vim_record(cl_edit *e, const cl_key *k)
{
    if (e->vreplay || k->k == K_CPR)
        return;
    if (e->vim == VIM_NORMAL && vim_idle(e)) {
        e->vrecon = 1;
        e->nvrec = 0;
        e->vreclast = 0;
        e->vsnap0 = e->nsnap;
        e->vcmdcount = 0;
    }
    /* a selection's motions are not the change: rec_extent stands for them */
    if (e->vrecon && e->vim != VIM_VISUAL && e->vim != VIM_VLINE)
        rec_put(e, k->k, k->mods, k->ch, k->text, k->n);
}

void vim_after(cl_edit *e)
{
    if (!e->vrecon || e->vreplay)
        return;
    if (e->vim == VIM_NORMAL && vim_idle(e)) {
        if (e->nsnap != e->vsnap0 && e->nvrec) {
            free(e->vdot);
            e->vdot = e->vrec;
            e->nvdot = e->nvrec;
            e->vdotcount = e->vcmdcount;
            e->vrec = 0;
            e->nvrec = e->cvrec = 0;
        }
        e->vrecon = 0;
    }
}

static void replay(cl_edit *e, long count)
{
    long i = 0;
    char num[16];
    int nd = 0, k;
    if (!e->vdot || !e->nvdot)
        return;
    while (count > 0 && nd < 15) {
        num[nd++] = (char)('0' + count % 10);
        count /= 10;
    }
    e->vreplay = 1;
    while (i < e->nvdot) {
        cl_key key;
        memset(&key, 0, sizeof(key));
        if ((unsigned char)e->vdot[i] == REC_EXTENT) {
            /* the selection again, as big, from the cursor */
            long dl = (long)get4(e->vdot + i + 2), dc = (long)get4(e->vdot + i + 6), ln = line_no(e, e->cur);
            long last = line_no(e, e->n), tl = ln + dl > last ? last : ln + dl, n2;
            e->vim = e->vdot[i + 1];
            e->vanchor = e->cur;
            if (e->vim == VIM_VLINE) {
                e->cur = to_col(e, line_at(e, tl), col_of(e, e->cur));
            } else if (dl == 0) {
                long z = ed_lend(e, e->cur);
                for (n2 = 1; n2 < dc && ed_next(e, e->cur) < z; n2++)
                    e->cur = ed_next(e, e->cur);
            } else {
                e->cur = to_col(e, line_at(e, tl), dc);
                clamp(e);
            }
            i += 10;
            nd = 0;                 /* a visual change keeps its extent: no count */
            continue;
        }
        key.k = (unsigned char)e->vdot[i];
        key.mods = (unsigned char)e->vdot[i + 1];
        key.ch = get4(e->vdot + i + 2);
        if (key.k == K_PASTE) {
            key.n = (long)get4(e->vdot + i + 6);
            key.text = e->vdot + i + 10;
            i += 10 + key.n;
        } else {
            i += 6;
        }
        if (nd && e->vim == VIM_NORMAL && vim_idle(e)) {
            /* the count in front of the change's first key */
            for (k = nd - 1; k >= 0; k--) {
                cl_key d;
                memset(&d, 0, sizeof(d));
                d.k = K_CHAR;
                d.ch = (unsigned long)(unsigned char)num[k];
                ed_key(e, &d);
            }
        }
        nd = 0;
        ed_key(e, &key);
    }
    e->vreplay = 0;
}
