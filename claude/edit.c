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

static void cut(cl_edit *e, long a, long z, int keep)
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

static long prev_char(const cl_edit *e, long i)
{
    if (i <= 0)
        return 0;
    i--;
    while (i > 0 && ((unsigned char)e->b[i] & 0xc0) == 0x80)
        i--;
    return i;
}

static long next_char(const cl_edit *e, long i)
{
    unsigned long cp;
    if (i >= e->n)
        return e->n;
    return i + vw_char(e->b + i, e->n - i, &cp);
}

static long line_start(const cl_edit *e, long i)
{
    while (i > 0 && e->b[i - 1] != '\n')
        i--;
    return i;
}

static long line_end(const cl_edit *e, long i)
{
    while (i < e->n && e->b[i] != '\n')
        i++;
    return i;
}

static int is_word(int c)
{
    return c && c != ' ' && c != '\t' && c != '\n' && c != '/' && c != ':';
}

static long word_back(const cl_edit *e, long i)
{
    while (i > 0 && !is_word((unsigned char)e->b[i - 1]))
        i--;
    while (i > 0 && is_word((unsigned char)e->b[i - 1]))
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
        a = next_char(e, a);
        c++;
    }
    return c;
}

static long at_col(const cl_edit *e, long s, long col)
{
    long end = line_end(e, s);
    while (col-- > 0 && s < end)
        s = next_char(e, s);
    return s;
}

static void hist_go(cl_edit *e, int to)
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

int ed_key(cl_edit *e, const cl_key *k)
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
        cut(e, prev_char(e, e->cur), e->cur, 0);
        return 1;
    case K_DEL:
        cut(e, e->cur, next_char(e, e->cur), 0);
        return 1;
    case K_LEFT:
        e->cur = k->mods & (KM_CTRL | KM_ALT) ? word_back(e, e->cur) : prev_char(e, e->cur);
        return 1;
    case K_RIGHT:
        e->cur = k->mods & (KM_CTRL | KM_ALT) ? word_fwd(e, e->cur) : next_char(e, e->cur);
        return 1;
    case K_HOME:
        e->cur = line_start(e, e->cur);
        return 1;
    case K_END:
        e->cur = line_end(e, e->cur);
        return 1;
    case K_UP: {
        long s = line_start(e, e->cur);
        if (s == 0) {
            hist_go(e, e->hpos - 1);
            return 1;
        }
        e->cur = at_col(e, line_start(e, s - 1), chars_between(e, s, e->cur));
        return 1;
    }
    case K_DOWN: {
        long z = line_end(e, e->cur);
        if (z == e->n) {
            hist_go(e, e->hpos + 1);
            return 1;
        }
        e->cur = at_col(e, z + 1, chars_between(e, line_start(e, e->cur), e->cur));
        return 1;
    }
    case K_ALT:
        if (k->ch == 0x7f) {
            cut(e, word_back(e, e->cur), e->cur, 1);
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
            cut(e, e->cur, word_fwd(e, e->cur), 1);
            return 1;
        }
        return 0;
    case K_CTRL:
        switch (k->ch) {
        case 'a':
            e->cur = line_start(e, e->cur);
            return 1;
        case 'e':
            e->cur = line_end(e, e->cur);
            return 1;
        case 'b':
            e->cur = prev_char(e, e->cur);
            return 1;
        case 'f':
            e->cur = next_char(e, e->cur);
            return 1;
        case 'k': {
            long z = line_end(e, e->cur);
            /* at a line's end Ctrl+K joins the next line */
            cut(e, e->cur, z == e->cur && z < e->n ? z + 1 : z, 1);
            return 1;
        }
        case 'u':
            cut(e, line_start(e, e->cur), e->cur, 1);
            return 1;
        case 'w':
            cut(e, word_back(e, e->cur), e->cur, 1);
            return 1;
        case 'h':
            cut(e, prev_char(e, e->cur), e->cur, 0);
            return 1;
        case 'y':
            if (e->kill)
                ed_insert(e, e->kill, (long)strlen(e->kill));
            return 1;
        case 'p':
            hist_go(e, e->hpos - 1);
            return 1;
        case 'n':
            hist_go(e, e->hpos + 1);
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
