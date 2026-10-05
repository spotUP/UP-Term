/* edit -- see edit.h. */
#include <stdlib.h>
#include <string.h>
#include "edit.h"
#include "../view/vw_text.h"

static char *dup(const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (d) {
        memcpy(d, s, (size_t)n);
        d[n] = 0;
    }
    return d;
}

void ed_init(cl_edit *e)
{
    memset(e, 0, sizeof(*e));
    e->b = dup("", 0);
    e->cap = 1;
}

void ed_free(cl_edit *e)
{
    int i;
    for (i = 0; i < e->nh; i++)
        free(e->hist[i]);
    for (i = 0; i < e->nundo; i++)
        free(e->undo[i].b);
    free(e->b);
    free(e->draft);
    free(e->kill);
    memset(e, 0, sizeof(*e));
}

static int room(cl_edit *e, long need)
{
    char *nb;
    long nc;
    if (need + 1 <= e->cap)
        return 1;
    nc = e->cap < 64 ? 64 : e->cap;
    while (nc < need + 1)
        nc *= 2;
    nb = (char *)realloc(e->b, (size_t)nc);
    if (!nb) {
        e->oom = 1;
        return 0;
    }
    e->b = nb;
    e->cap = nc;
    return 1;
}

void ed_clear(cl_edit *e)
{
    e->n = 0;
    e->cur = 0;
    if (e->b)
        e->b[0] = 0;
    e->hpos = e->nh;
    while (e->nundo) {
        free(e->undo[--e->nundo].b);
        e->undo[e->nundo].b = 0;
    }
    e->lastk = 0;
    if (e->vim)
        e->vim = VIM_INSERT;    /* a new prompt starts in INSERT, as Claude Code's */
    e->vcount = e->vopcount = e->vop = e->vpend = 0;
}

void ed_set(cl_edit *e, const char *s)
{
    long n = (long)strlen(s);
    if (!room(e, n))
        return;
    memcpy(e->b, s, (size_t)n + 1);
    e->n = n;
    e->cur = n;
}

void ed_insert(cl_edit *e, const char *s, long n)
{
    if (n <= 0 || !room(e, e->n + n))
        return;
    memmove(e->b + e->cur + n, e->b + e->cur, (size_t)(e->n - e->cur) + 1);
    memcpy(e->b + e->cur, s, (size_t)n);
    e->n += n;
    e->cur += n;
}

void ed_cut(cl_edit *e, long a, long z, int keep)
{
    if (z <= a)
        return;
    if (keep) {
        free(e->kill);
        e->kill = dup(e->b + a, z - a);
    }
    memmove(e->b + a, e->b + z, (size_t)(e->n - z) + 1);
    e->n -= z - a;
    e->cur = a;
}

long ed_prev(const cl_edit *e, long i)
{
    if (i <= 0)
        return 0;
    i--;
    while (i > 0 && ((unsigned char)e->b[i] & 0xc0) == 0x80)
        i--;
    return i;
}

long ed_next(const cl_edit *e, long i)
{
    unsigned long cp;
    if (i >= e->n)
        return e->n;
    return i + vw_char(e->b + i, e->n - i, &cp);
}

long ed_lstart(const cl_edit *e, long i)
{
    while (i > 0 && e->b[i - 1] != '\n')
        i--;
    return i;
}

long ed_lend(const cl_edit *e, long i)
{
    while (i < e->n && e->b[i] != '\n')
        i++;
    return i;
}

/* Alt+B/F/D: a word is a run of letters and digits (any non-ASCII
 * character counts as a letter); Ctrl+W: back to the previous white space */
static int is_word(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80;
}

static int is_blank(int c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

static long word_back(const cl_edit *e, long i)
{
    while (i > 0 && !is_word((unsigned char)e->b[i - 1]))
        i--;
    while (i > 0 && is_word((unsigned char)e->b[i - 1]))
        i--;
    return i;
}

static long blank_back(const cl_edit *e, long i)
{
    while (i > 0 && is_blank((unsigned char)e->b[i - 1]))
        i--;
    while (i > 0 && !is_blank((unsigned char)e->b[i - 1]))
        i--;
    return i;
}

static long word_fwd(const cl_edit *e, long i)
{
    while (i < e->n && !is_word((unsigned char)e->b[i]))
        i++;
    while (i < e->n && is_word((unsigned char)e->b[i]))
        i++;
    return i;
}

/* the column (in characters) of i on its line, and the offset of a column
 * on the line starting at s */
static long chars_between(const cl_edit *e, long a, long z)
{
    long c = 0;
    while (a < z) {
        a = ed_next(e, a);
        c++;
    }
    return c;
}

static long at_col(const cl_edit *e, long s, long col)
{
    long end = ed_lend(e, s);
    while (col-- > 0 && s < end)
        s = ed_next(e, s);
    return s;
}

void ed_hist_go(cl_edit *e, int to)
{
    if (to < 0 || to > e->nh || to == e->hpos)
        return;
    if (e->hpos == e->nh) {
        free(e->draft);
        e->draft = dup(e->b, e->n);
    }
    e->hpos = to;
    ed_set(e, to == e->nh ? (e->draft ? e->draft : "") : e->hist[to]);
}

void ed_remember(cl_edit *e, const char *s)
{
    char *d;
    if (!*s || (e->nh && !strcmp(e->hist[e->nh - 1], s))) {
        e->hpos = e->nh;
        return;
    }
    d = dup(s, (long)strlen(s));
    if (!d)
        return;
    if (e->nh == ED_HIST) {
        free(e->hist[0]);
        memmove(e->hist, e->hist + 1, sizeof(e->hist[0]) * (ED_HIST - 1));
        e->nh--;
    }
    e->hist[e->nh++] = d;
    e->hpos = e->nh;
}

void ed_snap(cl_edit *e)
{
    char *d = dup(e->b, e->n);
    if (!d)
        return;
    if (e->nundo == ED_UNDO) {
        free(e->undo[0].b);
        memmove(e->undo, e->undo + 1, sizeof(e->undo[0]) * (ED_UNDO - 1));
        e->nundo--;
    }
    e->undo[e->nundo].b = d;
    e->undo[e->nundo].cur = e->cur;
    e->nundo++;
}

int ed_undo(cl_edit *e)
{
    ed_snapshot *s;
    if (!e->nundo)
        return 0;
    s = &e->undo[--e->nundo];
    ed_set(e, s->b);
    e->cur = s->cur <= e->n ? s->cur : e->n;
    free(s->b);
    s->b = 0;
    return 1;
}

static int changes(const cl_key *k)
{
    switch (k->k) {
    case K_CHAR:
    case K_NEWLINE:
    case K_PASTE:
    case K_BS:
    case K_DEL:
        return 1;
    case K_ALT:
        return k->ch == 0x7f || k->ch == 'd';
    case K_CTRL:
        return k->ch == 'k' || k->ch == 'u' || k->ch == 'w' || k->ch == 'h' || k->ch == 'y' || k->ch == 'd';
    default:
        return 0;
    }
}

static int edit_key(cl_edit *e, const cl_key *k);

int ed_key(cl_edit *e, const cl_key *k)
{
    int r, snapped = 0;
    long n0 = e->n, c0 = e->cur;
    if (e->vim == VIM_NORMAL) {
        r = vim_normal(e, k);
        if (r >= 0)
            return r;
    } else if (e->vim == VIM_INSERT && k->k == K_ESC) {
        vim_escape(e);
        return 1;
    }
    if (k->k == K_CTRL && k->ch == '_') {
        ed_undo(e);
        e->lastk = 0;
        return 1;
    }
    /* typed characters in a row undo as one; in vim the command that
     * started INSERT took the snapshot */
    if (changes(k) && e->vim != VIM_INSERT && !(k->k == K_CHAR && e->lastk == K_CHAR)) {
        ed_snap(e);
        snapped = 1;
    }
    r = edit_key(e, k);
    if (snapped && e->n == n0 && e->cur == c0 && e->nundo && !strcmp(e->undo[e->nundo - 1].b, e->b)) {
        /* nothing changed: no undo step */
        free(e->undo[--e->nundo].b);
        e->undo[e->nundo].b = 0;
    }
    e->lastk = changes(k) ? k->k : 0;
    return r;
}

static int edit_key(cl_edit *e, const cl_key *k)
{
    char u[8];
    switch (k->k) {
    case K_CHAR:
        if (k->ch < 0x20 || k->ch == 0x7f)
            return 0;
        ed_insert(e, u, vw_put_utf8(u, k->ch));
        return 1;
    case K_NEWLINE:
        ed_insert(e, "\n", 1);
        return 1;
    case K_PASTE: {
        /* CR LF and CR become LF; other controls but Tab are dropped */
        long i;
        for (i = 0; i < k->n; i++) {
            char c = k->text[i];
            if (c == '\r') {
                if (i + 1 < k->n && k->text[i + 1] == '\n')
                    continue;
                c = '\n';
            }
            if ((unsigned char)c < 0x20 && c != '\n' && c != '\t')
                continue;
            ed_insert(e, &c, 1);
        }
        return 1;
    }
    case K_BS:
        ed_cut(e, ed_prev(e, e->cur), e->cur, 0);
        return 1;
    case K_DEL:
        ed_cut(e, e->cur, ed_next(e, e->cur), 0);
        return 1;
    case K_LEFT:
        e->cur = k->mods & (KM_CTRL | KM_ALT) ? word_back(e, e->cur) : ed_prev(e, e->cur);
        return 1;
    case K_RIGHT:
        e->cur = k->mods & (KM_CTRL | KM_ALT) ? word_fwd(e, e->cur) : ed_next(e, e->cur);
        return 1;
    case K_HOME:
        e->cur = ed_lstart(e, e->cur);
        return 1;
    case K_END:
        e->cur = ed_lend(e, e->cur);
        return 1;
    case K_UP: {
        long s = ed_lstart(e, e->cur);
        if (s == 0) {
            ed_hist_go(e, e->hpos - 1);
            return 1;
        }
        e->cur = at_col(e, ed_lstart(e, s - 1), chars_between(e, s, e->cur));
        return 1;
    }
    case K_DOWN: {
        long z = ed_lend(e, e->cur);
        if (z == e->n) {
            ed_hist_go(e, e->hpos + 1);
            return 1;
        }
        e->cur = at_col(e, z + 1, chars_between(e, ed_lstart(e, e->cur), e->cur));
        return 1;
    }
    case K_ALT:
        if (k->ch == 0x7f) {
            ed_cut(e, word_back(e, e->cur), e->cur, 1);
            return 1;
        }
        if (k->ch == 'b') {
            e->cur = word_back(e, e->cur);
            return 1;
        }
        if (k->ch == 'f') {
            e->cur = word_fwd(e, e->cur);
            return 1;
        }
        if (k->ch == 'd') {
            ed_cut(e, e->cur, word_fwd(e, e->cur), 1);
            return 1;
        }
        return 0;
    case K_CTRL:
        switch (k->ch) {
        case 'a':
            e->cur = ed_lstart(e, e->cur);
            return 1;
        case 'e':
            e->cur = ed_lend(e, e->cur);
            return 1;
        case 'b':
            e->cur = ed_prev(e, e->cur);
            return 1;
        case 'f':
            e->cur = ed_next(e, e->cur);
            return 1;
        case 'k': {
            long z = ed_lend(e, e->cur);
            /* at a line's end Ctrl+K joins the next line */
            ed_cut(e, e->cur, z == e->cur && z < e->n ? z + 1 : z, 1);
            return 1;
        }
        case 'u': {
            long a = ed_lstart(e, e->cur);
            /* at a line's start it joins the line above: repeated, it
             * clears a multi-line text */
            ed_cut(e, a == e->cur && a > 0 ? a - 1 : a, e->cur, 1);
            return 1;
        }
        case 'w':
            ed_cut(e, blank_back(e, e->cur), e->cur, 1);
            return 1;
        case 'd':
            ed_cut(e, e->cur, ed_next(e, e->cur), 0);
            return 1;
        case 'h':
            ed_cut(e, ed_prev(e, e->cur), e->cur, 0);
            return 1;
        case 'y':
            if (e->kill)
                ed_insert(e, e->kill, (long)strlen(e->kill));
            return 1;
        case 'p':
            ed_hist_go(e, e->hpos - 1);
            return 1;
        case 'n':
            ed_hist_go(e, e->hpos + 1);
            return 1;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

int ed_layout(const cl_edit *e, int width, long *a, long *z, int max, int *crow, int *ccol)
{
    long i = 0;
    int r = 0, col = 0;
    long start = 0;
    if (width < 1)
        width = 1;
    *crow = 0;
    *ccol = 0;
    for (;;) {
        if (i == e->cur) {
            *crow = r;
            *ccol = col;
        }
        if (i >= e->n || e->b[i] == '\n') {
            if (r < max) {
                a[r] = start;
                z[r] = i;
            }
            r++;
            if (i >= e->n)
                break;
            i++;
            start = i;
            col = 0;
            continue;
        }
        {
            unsigned long cp;
            int l = vw_char(e->b + i, e->n - i, &cp);
            int w = cp == '\t' ? 1 : vw_cp_width(cp);
            if (w < 0)
                w = 0;
            if (col + w > width) {
                if (r < max) {
                    a[r] = start;
                    z[r] = i;
                }
                r++;
                start = i;
                col = 0;
                if (i == e->cur) {
                    *crow = r;
                    *ccol = 0;
                }
            }
            col += w;
            i += l;
        }
    }
    /* the cursor after the last character of a full row sits on the next */
    if (*ccol >= width) {
        (*crow)++;
        *ccol = 0;
        if (*crow >= r) {
            if (r < max) {
                a[r] = e->n;
                z[r] = e->n;
            }
            r++;
        }
    }
    return r;
}
