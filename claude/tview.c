/* tview -- Ctrl+O's transcript viewer (A4 1.6), as Claude Code's: the
 * whole conversation with every tool result in full and the thinking
 * text (tui_log keeps it), on the alternate screen so the transcript and
 * its scrollback come back untouched. Keys: Up/Down (k/j), PgUp/PgDn
 * (b/Space), Home/End (g/G); Ctrl+R or / types a search, Enter finds the
 * next older line holding it (shown with the words in reverse), n / N
 * the next older / newer one; q, Esc, Ctrl+C or Ctrl+O leave. A line
 * scroll is one SU/SD and one row, a page a full draw; lines longer than
 * the window are cut (autowrap off), never wrapped. A window resized
 * while it is open is followed: the same first line, its rows now.
 * Portable C89, host-tested on the engine (tests/test_claude_tui.c). */
#include <stdlib.h>
#include <string.h>
#include "tui.h"
#include "util.h"
#include "../view/vw_text.h"

typedef struct tv {
    cl_tui *t;
    const char *p;
    long *ix;           /* line starts; ix[n] = the end */
    long n;
    long top;
    int h;              /* the rows the text gets (the last one is the status) */
    char q[64];
    int typing;         /* the search's query being typed */
    long match;         /* the matched line, -1 none */
    int fail;
    jw o;
} tv;

static void put(tv *v, const char *s)
{
    jw_rawz(&v->o, s);
}

static void flush(tv *v)
{
    if (v->o.n)
        v->t->io->write(v->t->io->u, v->o.p, v->o.n);
    jw_reset(&v->o);
}

static void cup(tv *v, int row)
{
    char b[24], n[12];
    cl_copy(b, "\033[", sizeof(b));
    cl_ltoa(row, n);
    cl_cat(b, n, sizeof(b));
    cl_cat(b, ";1H\033[2K", sizeof(b));
    put(v, b);
}

/* a line without its escape sequences; its length */
static long strip(const char *s, long n, char *out, long cap)
{
    long i = 0, k = 0;
    while (i < n && k < cap - 1) {
        if (s[i] == 0x1b && i + 1 < n && s[i + 1] == '[') {
            i += 2;
            while (i < n && !((unsigned char)s[i] >= 0x40 && (unsigned char)s[i] <= 0x7e))
                i++;
            i++;
            continue;
        }
        if (s[i] == 0x1b && i + 1 < n && s[i + 1] == ']') {
            i += 2;
            while (i < n && s[i] != 7 && s[i] != 0x1b)
                i++;
            i += i < n && s[i] == 0x1b ? 2 : 1;
            continue;
        }
        out[k++] = s[i++];
    }
    out[k] = 0;
    return k;
}

static long line_len(tv *v, long li)
{
    long a = v->ix[li], z = v->ix[li + 1];
    if (z > a && v->p[z - 1] == '\n')
        z--;
    return z - a;
}

static void draw_row(tv *v, int r, long li)
{
    cup(v, r + 1);
    if (li < 0 || li >= v->n)
        return;
    if (li == v->match && v->q[0]) {
        char s[1024];
        long l = strip(v->p + v->ix[li], line_len(v, li), s, sizeof(s));
        const char *m = strstr(s, v->q);
        if (m) {
            long ql = (long)strlen(v->q);
            jw_raw(&v->o, s, (long)(m - s));
            put(v, "\033[7m");
            jw_raw(&v->o, m, ql);
            put(v, "\033[0m");
            jw_raw(&v->o, m + ql, l - (long)(m - s) - ql);
            return;
        }
    }
    jw_raw(&v->o, v->p + v->ix[li], line_len(v, li));
    put(v, "\033[0m");
}

static void status(tv *v)
{
    char m[200], n[16];
    long last = v->top + v->h < v->n ? v->top + v->h : v->n;
    cup(v, v->h + 1);
    put(v, "\033[7m");
    if (v->typing) {
        cl_copy(m, " Search: ", sizeof(m));
        cl_cat(m, v->q, sizeof(m));
        cl_cat(m, "_  (enter finds, esc leaves the search)", sizeof(m));
    } else {
        cl_copy(m, " Transcript ", sizeof(m));
        cl_ltoa(v->n ? v->top + 1 : 0, n);
        cl_cat(m, n, sizeof(m));
        cl_cat(m, "-", sizeof(m));
        cl_ltoa(last, n);
        cl_cat(m, n, sizeof(m));
        cl_cat(m, " of ", sizeof(m));
        cl_ltoa(v->n, n);
        cl_cat(m, n, sizeof(m));
        cl_cat(m, v->fail ? "  (not found)" : "", sizeof(m));
        cl_cat(m, "  q leaves, ctrl+r or / searches, n next ", sizeof(m));
    }
    put(v, m);
    put(v, "\033[0m");
}

static void draw(tv *v)
{
    int r;
    put(v, "\033[?2026h");
    for (r = 0; r < v->h; r++)
        draw_row(v, r, v->top + r);
    status(v);
    put(v, "\033[?2026l");
    flush(v);
}

static void clamp_top(tv *v)
{
    if (v->top > v->n - v->h)
        v->top = v->n - v->h;
    if (v->top < 0)
        v->top = 0;
}

/* the next line holding the query, older (dir -1) or newer (+1) */
static void find(tv *v, int dir)
{
    long i = v->match >= 0 ? v->match + dir : (dir < 0 ? v->top + v->h - 1 : v->top);
    char s[1024];
    v->fail = 1;
    if (!v->q[0])
        return;
    for (; i >= 0 && i < v->n; i += dir) {
        strip(v->p + v->ix[i], line_len(v, i), s, sizeof(s));
        if (strstr(s, v->q)) {
            v->match = i;
            v->fail = 0;
            if (i < v->top || i >= v->top + v->h)
                v->top = i - v->h / 2;
            clamp_top(v);
            return;
        }
    }
}

static void scroll(tv *v, int down)
{
    char b[16];
    if (down) {
        if (v->top + v->h >= v->n)
            return;
        v->top++;
        put(v, "\033[?2026h");
        cl_copy(b, "\033[S", sizeof(b));
        put(v, b);
        draw_row(v, v->h - 1, v->top + v->h - 1);
    } else {
        if (v->top <= 0)
            return;
        v->top--;
        put(v, "\033[?2026h\033[T");
        draw_row(v, 0, v->top);
    }
    status(v);
    put(v, "\033[?2026l");
    flush(v);
}

/* the region for the text rows, the status row below it */
static void set_region(tv *v)
{
    char reg[24], n[12];
    cl_copy(reg, "\033[1;", sizeof(reg));
    cl_ltoa(v->h, n);
    cl_cat(reg, n, sizeof(reg));
    cl_cat(reg, "r", sizeof(reg));
    put(v, reg);
}

/* the window changed size while the viewer is open: the same first line,
 * the rows the window has now */
static void follow_size(tv *v)
{
    if (!tui_resized(v->t))
        return;
    v->h = v->t->rows - 1;
    clamp_top(v);
    put(v, "\033[r\033[H\033[2J");
    set_region(v);
    draw(v);
}

void tui_transcript(cl_tui *t)
{
    tv v;
    long i, k;
    if (!t->started)
        return;
    memset(&v, 0, sizeof(v));
    v.t = t;
    v.p = t->log.p ? t->log.p : "";
    v.match = -1;
    jw_init(&v.o);
    for (i = 0; i < t->log.n; i++)
        v.n += v.p[i] == '\n';
    if (t->log.n && v.p[t->log.n - 1] != '\n')
        v.n++;
    v.ix = (long *)malloc(sizeof(long) * (size_t)(v.n + 1));
    if (!v.ix)
        return;
    for (i = 0, k = 0; k < v.n; k++) {
        v.ix[k] = i;
        while (i < t->log.n && v.p[i] != '\n')
            i++;
        i++;
    }
    v.ix[v.n] = t->log.n;
    v.h = t->rows - 1;
    v.top = v.n - v.h;
    clamp_top(&v);
    t->n_views++;
    /* the alternate screen, autowrap off, the status row outside the region */
    put(&v, "\033[?1049h\033[?25l\033[?7l");
    set_region(&v);
    draw(&v);
    for (;;) {
        cl_key k2;
        int r = tui_key(t, &k2, 500);
        if (r < 0)
            break;
        follow_size(&v);
        if (!r)
            continue;
        if (v.typing) {
            if (k2.k == K_CHAR && k2.ch >= 0x20) {
                char u[8];
                long l = (long)strlen(v.q);
                int ul = vw_put_utf8(u, k2.ch);
                if (l + ul < (long)sizeof(v.q)) {
                    memcpy(v.q + l, u, (size_t)ul);
                    v.q[l + ul] = 0;
                }
            } else if (k2.k == K_BS) {
                long l = (long)strlen(v.q);
                if (l)
                    v.q[l - 1] = 0;
            } else if (k2.k == K_ENTER) {
                v.typing = 0;
                v.match = -1;
                find(&v, -1);
                draw(&v);
                continue;
            } else if (k2.k == K_ESC || (k2.k == K_CTRL && k2.ch == 'c')) {
                v.typing = 0;
            }
            status(&v);
            flush(&v);
            continue;
        }
        if (k2.k == K_ESC || (k2.k == K_CTRL && (k2.ch == 'c' || k2.ch == 'o')) ||
            (k2.k == K_CHAR && k2.ch == 'q'))
            break;
        if (k2.k == K_UP || (k2.k == K_CHAR && k2.ch == 'k'))
            scroll(&v, 0);
        else if (k2.k == K_DOWN || k2.k == K_ENTER || (k2.k == K_CHAR && k2.ch == 'j'))
            scroll(&v, 1);
        else if (k2.k == K_PGUP || (k2.k == K_CHAR && k2.ch == 'b')) {
            v.top -= v.h;
            clamp_top(&v);
            draw(&v);
        } else if (k2.k == K_PGDN || (k2.k == K_CHAR && k2.ch == ' ')) {
            v.top += v.h;
            clamp_top(&v);
            draw(&v);
        } else if (k2.k == K_HOME || (k2.k == K_CHAR && k2.ch == 'g')) {
            v.top = 0;
            draw(&v);
        } else if (k2.k == K_END || (k2.k == K_CHAR && k2.ch == 'G')) {
            v.top = v.n;
            clamp_top(&v);
            draw(&v);
        } else if ((k2.k == K_CTRL && k2.ch == 'r') || (k2.k == K_CHAR && k2.ch == '/')) {
            if (k2.k == K_CTRL && v.q[0] && !v.fail && v.match >= 0) {
                find(&v, -1);           /* Ctrl+R again: the next older */
                draw(&v);
                continue;
            }
            v.typing = 1;
            v.q[0] = 0;
            v.fail = 0;
            status(&v);
            flush(&v);
        } else if (k2.k == K_CHAR && (k2.ch == 'n' || k2.ch == 'N')) {
            find(&v, k2.ch == 'n' ? -1 : 1);
            draw(&v);
        }
    }
    put(&v, "\033[r\033[?7h\033[?1049l");
    flush(&v);
    jw_free(&v.o);
    free(v.ix);
    t->full = 1;
    tui_frame(t);
}
