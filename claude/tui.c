/* tui -- see tui.h. */
#include <stdlib.h>
#include <string.h>
#include "tui.h"
#include "tools.h"
#include "util.h"
#include "../view/vw_text.h"

/* glyphs: box drawing (drawn by UP-Term at any font) and the stand-ins
 * render/glyphmap.c has for Claude Code's symbols */
#define G_TL "\342\225\255"     /* U+256D */
#define G_TR "\342\225\256"
#define G_BL "\342\225\260"
#define G_BR "\342\225\257"
#define G_H  "\342\224\200"     /* U+2500 */
#define G_V  "\342\224\202"     /* U+2502 */
#define G_PROMPT "\342\235\257" /* U+276F */
#define G_MODE "\342\217\265"   /* U+23F5 */
#define G_DOT "\302\267"        /* U+00B7 */

#define SGR0   "\033[0m"
#define DIM    "\033[2m"
#define BOLD   "\033[1m"
#define C_BOX  "\033[90m"       /* the frame: bright black (grey) */
#define C_SPIN "\033[33m"
#define C_SEL  "\033[36m"
#define C_MODE_ACCEPT "\033[35m"
#define C_MODE_PLAN   "\033[36m"

const char *const tui_mode_names[3] = { "default", "accept edits", "plan" };

static const char *const spin[] = {
    G_DOT, "\342\234\242", "\342\234\263", "\342\234\266", "\342\234\273", "\342\234\275",
    "\342\234\273", "\342\234\266", "\342\234\263", "\342\234\242"
};
#define NSPIN 10

static const char *const verbs[] = {
    "Pondering", "Thinking", "Cogitating", "Musing", "Ruminating", "Percolating", "Noodling",
    "Brewing", "Conjuring", "Mulling", "Tinkering", "Computing", "Deliberating", "Simmering",
    "Whirring", "Blitting", "Meditating", "Clauding", "Wrangling", "Contemplating"
};
#define NVERBS 20

/* ---- small things ---- */

int tui_width(const char *s, long n)
{
    long i = 0, a = 0;
    int w = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b && i + 1 < n) {
            w += vw_width(s + a, i - a);
            if (s[i + 1] == '[') {
                i += 2;
                while (i < n && !((unsigned char)s[i] >= 0x40 && (unsigned char)s[i] <= 0x7e))
                    i++;
                i++;
            } else if (s[i + 1] == ']') {
                i += 2;
                while (i < n && s[i] != 7 && !(s[i] == 0x1b && i + 1 < n && s[i + 1] == '\\'))
                    i++;
                i += i < n && s[i] == 0x1b ? 2 : 1;
            } else {
                i += 2;
            }
            a = i;
            continue;
        }
        i++;
    }
    if (a < n)
        w += vw_width(s + a, n - a);
    return w;
}

static void put(cl_tui *t, const char *s)
{
    jw_rawz(&t->o, s);
}

static void cup(cl_tui *t, int row, int col)
{
    char b[24], n[12];
    cl_copy(b, "\033[", sizeof(b));
    cl_ltoa(row, n);
    cl_cat(b, n, sizeof(b));
    cl_cat(b, ";", sizeof(b));
    cl_ltoa(col, n);
    cl_cat(b, n, sizeof(b));
    cl_cat(b, "H", sizeof(b));
    put(t, b);
}

static void region(cl_tui *t, int bot)
{
    char b[24], n[12];
    cl_copy(b, "\033[1;", sizeof(b));
    cl_ltoa(bot, n);
    cl_cat(b, n, sizeof(b));
    cl_cat(b, "r", sizeof(b));
    put(t, b);
}

static void flush(cl_tui *t)
{
    if (t->o.n) {
        t->io->write(t->io->u, t->o.p, t->o.n);
        t->n_writes++;
    }
    jw_reset(&t->o);
}

static void get_size(cl_tui *t, int *c, int *r)
{
    *c = 80;
    *r = 24;
    if (t->io->size && t->io->size(t->io->u, c, r) == 0) {
        if (*c < 20)
            *c = 20;
        if (*r < 8)
            *r = 8;
        if (*c > 512)
            *c = 512;
        if (*r > 256)
            *r = 256;
    } else {
        *c = 80;
        *r = 24;
    }
}

int tui_init(cl_tui *t, cl_io *io)
{
    memset(t, 0, sizeof(*t));
    t->io = io;
    jw_init(&t->o);
    keys_init(&t->keys);
    ed_init(&t->ed);
    get_size(t, &t->cols, &t->rows);
    t->B = t->rows;
    t->tr = 1;
    t->ctx_left = -1;
    t->full = 1;
    return t->ed.b ? 0 : -1;
}

void tui_free(cl_tui *t)
{
    int i;
    for (i = 0; i < TUI_FOOT; i++) {
        free(t->want[i]);
        free(t->drawn[i]);
        t->want[i] = t->drawn[i] = 0;
    }
    jw_free(&t->o);
    keys_free(&t->keys);
    ed_free(&t->ed);
}

/* ---- building the footer's rows ---- */

typedef struct row {
    jw b;
    int w, max;
} row;

static void r_init(row *r, int max)
{
    jw_init(&r->b);
    r->w = 0;
    r->max = max;
}

static void r_sgr(row *r, const char *s)
{
    jw_rawz(&r->b, s);
}

/* text cut to the room left: each character as it is; controls as '?' */
static void r_text(row *r, const char *s, long n)
{
    long i = 0;
    while (i < n && r->w < r->max) {
        unsigned long cp;
        int l = vw_char(s + i, n - i, &cp), w;
        char u[8];
        if (cp < 0x20 || cp == 0x7f) {
            if (cp == '\t')
                cp = ' ';
            else
                cp = '?';
        }
        w = vw_cp_width(cp);
        if (r->w + w > r->max)
            break;
        jw_raw(&r->b, u, vw_put_utf8(u, cp));
        r->w += w;
        i += l;
    }
}

static void r_textz(row *r, const char *s)
{
    r_text(r, s, (long)strlen(s));
}

/* a glyph one column wide */
static void r_glyph(row *r, const char *g)
{
    if (r->w < r->max) {
        jw_rawz(&r->b, g);
        r->w++;
    }
}

static void r_pad(row *r, int to)
{
    while (r->w < to && r->w < r->max) {
        jw_raw(&r->b, " ", 1);
        r->w++;
    }
}

static void want(cl_tui *t, row *r)
{
    if (t->nwant < TUI_FOOT) {
        free(t->want[t->nwant]);
        jw_rawz(&r->b, "");
        t->want[t->nwant] = r->b.p;
        t->wantw[t->nwant] = r->w;
        t->nwant++;
    } else {
        jw_free(&r->b);
    }
}

static void border(cl_tui *t, const char *l, const char *rr)
{
    row r;
    int i;
    r_init(&r, t->cols);
    r_sgr(&r, C_BOX);
    r_glyph(&r, l);
    for (i = 2; i < t->cols; i++)
        r_glyph(&r, G_H);
    r_glyph(&r, rr);
    r_sgr(&r, SGR0);
    want(t, &r);
}

static void fmt_k(long v, char *out)
{
    char n[16];
    if (v < 1000) {
        cl_ltoa(v, out);
        return;
    }
    cl_ltoa(v / 1000, out);
    if (v < 10000) {
        cl_cat(out, ".", 16);
        cl_ltoa((v / 100) % 10, n);
        cl_cat(out, n, 16);
    }
    cl_cat(out, "k", 16);
}

static void spinner_row(cl_tui *t)
{
    row r;
    char m[96], n[16];
    unsigned long now = t->io->ms ? t->io->ms(t->io->u) : 0;
    long secs = (long)((now - t->t0) / 1000);
    int v = (int)((t->t0 / 7 + secs / 8) % NVERBS);
    r_init(&r, t->cols);
    r_sgr(&r, C_SPIN);
    r_glyph(&r, spin[t->frame % NSPIN]);
    r_text(&r, " ", 1);
    r_textz(&r, verbs[v]);
    r_textz(&r, "... ");
    r_sgr(&r, SGR0 DIM);
    cl_copy(m, "(", sizeof(m));
    cl_ltoa(secs, n);
    cl_cat(m, n, sizeof(m));
    cl_cat(m, "s", sizeof(m));
    if (t->tokens > 0) {
        cl_cat(m, " ", sizeof(m));
        r_textz(&r, m);
        r_glyph(&r, G_DOT);
        fmt_k(t->tokens, n);
        cl_copy(m, " ", sizeof(m));
        cl_cat(m, n, sizeof(m));
        cl_cat(m, " tokens", sizeof(m));
    }
    cl_cat(m, " ", sizeof(m));
    r_textz(&r, m);
    r_glyph(&r, G_DOT);
    r_textz(&r, " esc to interrupt)");
    r_sgr(&r, SGR0);
    want(t, &r);
}

/* the facts' width: each \001 becomes a middle dot */
static int right_width(const char *s)
{
    int w = vw_width(s, (long)strlen(s));
    for (; *s; s++)
        w += *s == '\001';
    return w;
}

static void status_row(cl_tui *t)
{
    row r;
    char right[200], n[16];
    int mode = t->mode ? *t->mode : 0, rw, room;
    r_init(&r, t->cols);
    r_text(&r, "  ", 2);
    if (t->hint[0]) {
        r_sgr(&r, DIM);
        r_textz(&r, t->hint);
    } else if (mode == PERM_ACCEPT) {
        r_sgr(&r, C_MODE_ACCEPT);
        r_glyph(&r, G_MODE);
        r_glyph(&r, G_MODE);
        r_textz(&r, " accept edits on");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (shift+tab to cycle)");
    } else if (mode == PERM_PLAN) {
        r_sgr(&r, C_MODE_PLAN);
        r_textz(&r, "|| plan mode on");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (shift+tab to cycle)");
    } else {
        r_sgr(&r, DIM);
        r_textz(&r, t->expand ? "/ for commands, ctrl+o: results in full" : "/ for commands");
    }
    r_sgr(&r, SGR0);
    /* the facts on the right, the least needed dropped first */
    right[0] = 0;
    {
        int drop;
        for (drop = 0; drop < 4; drop++) {
            right[0] = 0;
            if (drop < 1 && t->root && *t->root) {
                cl_cat(right, t->root, sizeof(right));
                cl_cat(right, " \001 ", sizeof(right));
            }
            if (drop < 2 && t->model) {
                cl_cat(right, t->model, sizeof(right));
                cl_cat(right, " \001 ", sizeof(right));
            }
            if (drop < 3 && t->effort) {
                cl_cat(right, t->effort, sizeof(right));
                cl_cat(right, " \001 ", sizeof(right));
            }
            cl_cat(right, "ctx: ", sizeof(right));
            if (t->ctx_left >= 0) {
                cl_ltoa(t->ctx_left, n);
                cl_cat(right, n, sizeof(right));
                cl_cat(right, "% left", sizeof(right));
            } else {
                cl_cat(right, "100% left", sizeof(right));
            }
            rw = right_width(right);
            if (r.w + 3 + rw <= t->cols - 1)    /* three blanks between, one column spare */
                break;
        }
        rw = right_width(right);
        room = t->cols - 1 - rw;
        if (room >= r.w + 3) {
            const char *p = right;
            r_pad(&r, room);
            r_sgr(&r, DIM);
            while (*p) {
                const char *q = strchr(p, '\001');
                if (!q) {
                    r_textz(&r, p);
                    break;
                }
                r_text(&r, p, (long)(q - p));
                r_glyph(&r, G_DOT);
                p = q + 1;
            }
            r_sgr(&r, SGR0);
        }
    }
    want(t, &r);
}

/* the slash commands matching the box's text: their indexes */
static int slash_matches(cl_tui *t, int *idx, int max)
{
    int i, n = 0;
    const char *s = t->ed.b;
    long len = t->ed.n;
    if (t->modal || !t->cmds || len < 1 || s[0] != '/' || t->mclosed || memchr(s, ' ', (size_t)len) ||
        memchr(s, '\n', (size_t)len))
        return 0;
    for (i = 0; i < t->ncmds && n < max; i++)
        if (!strncmp(t->cmds[i].name, s, (size_t)len))
            idx[n++] = i;
    return n;
}

static void slash_rows(cl_tui *t, int *idx, int n)
{
    int i, wn = 0;
    for (i = 0; i < n; i++) {
        int w = (int)strlen(t->cmds[idx[i]].name);
        if (w > wn)
            wn = w;
    }
    for (i = 0; i < n; i++) {
        row r;
        r_init(&r, t->cols);
        r_text(&r, "  ", 2);
        r_sgr(&r, i == t->msel ? C_SEL BOLD : DIM);
        r_textz(&r, t->cmds[idx[i]].name);
        r_pad(&r, 2 + wn + 3);
        r_sgr(&r, i == t->msel ? SGR0 C_SEL : SGR0 DIM);
        r_textz(&r, t->cmds[idx[i]].help);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

static void box_rows(cl_tui *t, int top_row)
{
    long a[64], z[64];
    int n, crow, ccol, i, show, inner = t->cols - 6;
    if (inner < 4)
        inner = 4;
    n = ed_layout(&t->ed, inner, a, z, 64, &crow, &ccol);
    if (n > 64)
        n = 64;
    if (crow >= 64)
        crow = 63;
    show = n < TUI_BOX_ROWS ? n : TUI_BOX_ROWS;
    if (crow < t->scroll)
        t->scroll = crow;
    if (crow >= t->scroll + show)
        t->scroll = crow - show + 1;
    if (t->scroll > n - show)
        t->scroll = n - show;
    if (t->scroll < 0)
        t->scroll = 0;
    border(t, G_TL, G_TR);
    for (i = 0; i < show; i++) {
        row r;
        int k = t->scroll + i;
        r_init(&r, t->cols);
        r_sgr(&r, C_BOX);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);
        if (k == 0) {
            r_glyph(&r, G_PROMPT);
            r_text(&r, " ", 1);
        } else {
            r_text(&r, "  ", 2);
        }
        r_text(&r, t->ed.b + a[k], z[k] - a[k]);
        r_pad(&r, t->cols - 1);
        r_sgr(&r, C_BOX);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    border(t, G_BL, G_BR);
    if (!t->modal) {
        t->crow = top_row + 2 + (crow - t->scroll);   /* the footer row, from 1 */
        t->ccol = 5 + ccol;
    }
}

static void menu_rows(cl_tui *t)
{
    int i;
    row r;
    border(t, G_TL, G_TR);
    r_init(&r, t->cols);
    r_sgr(&r, C_BOX);
    r_glyph(&r, G_V);
    r_sgr(&r, SGR0 BOLD);
    r_text(&r, " ", 1);
    r_textz(&r, t->m_title ? t->m_title : "");
    r_sgr(&r, SGR0);
    r_pad(&r, t->cols - 1);
    r_sgr(&r, C_BOX);
    r_glyph(&r, G_V);
    r_sgr(&r, SGR0);
    want(t, &r);
    if (t->m_q && *t->m_q) {
        r_init(&r, t->cols);
        r_sgr(&r, C_BOX);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);
        r_textz(&r, t->m_q);
        r_pad(&r, t->cols - 1);
        r_sgr(&r, C_BOX);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    for (i = 0; i < t->m_n; i++) {
        char num[16];
        r_init(&r, t->cols);
        r_sgr(&r, C_BOX);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);
        if (i == t->m_sel) {
            r_sgr(&r, C_SEL);
            r_glyph(&r, G_PROMPT);
        } else {
            r_text(&r, " ", 1);
        }
        r_text(&r, " ", 1);
        cl_ltoa(i + 1, num);
        cl_cat(num, ". ", sizeof(num));
        r_textz(&r, num);
        r_textz(&r, t->m_opt[i]);
        r_sgr(&r, SGR0);
        r_pad(&r, t->cols - 1);
        r_sgr(&r, C_BOX);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    border(t, G_BL, G_BR);
}

static void build(cl_tui *t)
{
    int idx[8], nm;
    t->nwant = 0;
    t->crow = 0;
    if (t->busy)
        spinner_row(t);
    if (t->modal) {
        menu_rows(t);
        return;
    }
    box_rows(t, t->nwant);
    nm = slash_matches(t, idx, 8);
    if (t->msel >= nm)
        t->msel = nm ? nm - 1 : 0;
    if (nm)
        slash_rows(t, idx, nm);
    else
        status_row(t);
}

/* ---- drawing ---- */

/* the transcript region ends at newB: the footer grows (the transcript
 * scrolls up if it would be covered) or shrinks */
static void set_bottom(cl_tui *t, int newB)
{
    if (newB < 1)
        newB = 1;
    if (t->tr > newB + 1) {
        int s = t->tr - (newB + 1), i;
        if (s > t->B)
            s = t->B;
        /* within the old region: its bottom row, then newlines scroll it */
        cup(t, t->B, 1);
        for (i = 0; i < s; i++)
            put(t, "\n");
        t->tr = newB + 1;
    }
    {
        /* what was the footer and is not any more (a smaller footer: the
         * rows the transcript gets back) is cleared with what is the
         * footer now */
        int from = (newB < t->B ? newB : t->B) + 1;
        t->n_full++;
        t->B = newB;
        region(t, newB);
        cup(t, from <= t->rows ? from : t->rows, 1);
        put(t, SGR0 "\033[J");
    }
    t->full = 1;
}

void tui_frame(cl_tui *t)
{
    int i, newB, any = 0;
    if (!t->started)
        return;
    build(t);
    newB = t->rows - t->nwant;
    if (newB == t->B && !t->full && t->nwant == t->ndrawn && t->crow == t->lrow && t->ccol == t->lcol) {
        for (i = 0; i < t->nwant; i++)
            if (!t->drawn[i] || strcmp(t->drawn[i], t->want[i]))
                break;
        if (i == t->nwant)
            return;                 /* nothing changed: nothing sent */
    }
    t->lrow = t->crow;
    t->lcol = t->ccol;
    put(t, "\033[?2026h\033[?25l");
    if (newB != t->B || t->full) {
        set_bottom(t, newB);
        any = 1;
    }
    for (i = 0; i < t->nwant; i++) {
        if (!t->full && i < t->ndrawn && t->drawn[i] && !strcmp(t->drawn[i], t->want[i]))
            continue;
        cup(t, t->B + 1 + i, 1);
        put(t, t->want[i]);
        put(t, SGR0);
        if (t->wantw[i] < t->cols)
            put(t, "\033[K");
        free(t->drawn[i]);
        t->drawn[i] = (char *)malloc(strlen(t->want[i]) + 1);
        if (t->drawn[i])
            strcpy(t->drawn[i], t->want[i]);
        t->n_rows++;
        any = 1;
    }
    for (; i < t->ndrawn; i++) {
        free(t->drawn[i]);
        t->drawn[i] = 0;
    }
    t->ndrawn = t->nwant;
    t->full = 0;
    if (t->crow) {
        cup(t, t->B + t->crow, t->ccol);
        put(t, "\033[?25h");
    } else {
        /* the cursor rests where the transcript goes on */
        cup(t, t->tr <= t->B ? t->tr : t->B, 1);
    }
    put(t, "\033[?2026l");
    if (any)
        t->n_frames++;
    flush(t);
}

void tui_lines(cl_tui *t, const char *s, long n)
{
    long a = 0, i;
    int r0 = t->tr, rows = 0, first = 1;
    if (!t->started) {
        t->io->write(t->io->u, s, n);
        return;
    }
    put(t, "\033[?2026h\033[?25l");
    if (r0 > t->B) {
        cup(t, t->B, 1);
        put(t, "\n");
    } else {
        cup(t, r0, 1);
    }
    for (i = 0; i <= n; i++) {
        if (i < n && s[i] != '\n')
            continue;
        if (i == n && a == n)
            break;
        {
            int w = tui_width(s + a, i - a), k = w ? (w + t->cols - 1) / t->cols : 1;
            if (!first)
                put(t, "\r\n");
            jw_raw(&t->o, s + a, i - a);
            put(t, SGR0);
            rows += k;
            first = 0;
            t->n_lines++;
        }
        a = i + 1;
    }
    if (r0 > t->B)
        t->tr = t->B + 1;
    else
        t->tr = r0 + rows > t->B + 1 ? t->B + 1 : r0 + rows;
    /* the cursor back in the box */
    if (t->crow) {
        cup(t, t->B + t->crow, t->ccol);
        put(t, "\033[?25h");
    }
    put(t, "\033[?2026l");
    flush(t);
}

void tui_clear(cl_tui *t)
{
    if (!t->started)
        return;
    put(t, "\033[r\033[H\033[2J");
    t->B = t->rows;
    t->tr = 1;
    t->full = 1;
    tui_frame(t);
}

static void check_size(cl_tui *t)
{
    int c, r;
    get_size(t, &c, &r);
    if (c == t->cols && r == t->rows)
        return;
    t->cols = c;
    t->rows = r;
    put(t, "\033[r");
    t->B = r;
    if (t->tr > r)
        t->tr = r;
    t->full = 1;
}

int tui_start(cl_tui *t)
{
    unsigned long t0;
    if (t->io->raw && t->io->raw(t->io->u, 1))
        return -1;
    get_size(t, &t->cols, &t->rows);
    /* bracketed paste, kitty's disambiguation (Esc and Shift+Enter come
     * as CSI u), modifyOtherKeys 1 for terminals without it; where is the
     * cursor? */
    t->io->write(t->io->u, "\033[?2004h\033[>1u\033[>4;1m\033[6n", 25);
    t->tr = t->rows;
    t0 = t->io->ms ? t->io->ms(t->io->u) : 0;
    for (;;) {
        char b[64];
        cl_key k;
        long n = t->io->read(t->io->u, b, sizeof(b), 100);
        int got = 0;
        if (n < 0)
            break;
        if (n > 0)
            keys_feed(&t->keys, b, n);
        while (keys_next(&t->keys, &k, n == 0)) {
            if (k.k == K_CPR) {
                t->tr = k.row + (k.col > 1);
                got = 1;
            }
            /* anything typed before the answer is lost: nothing to edit yet */
        }
        if (got || !t->io->ms || t->io->ms(t->io->u) - t0 > 600)
            break;
    }
    if (t->tr < 1)
        t->tr = 1;
    if (t->tr > t->rows + 1)
        t->tr = t->rows + 1;
    t->B = t->rows;
    t->started = 1;
    t->full = 1;
    tui_frame(t);
    return 0;
}

void tui_stop(cl_tui *t)
{
    if (!t->started)
        return;
    put(t, "\033[r");
    cup(t, t->tr <= t->B ? t->tr : t->B + 1 <= t->rows ? t->B + 1 : t->rows, 1);
    if (t->tr > t->B && t->B >= t->rows)
        put(t, "\r\n");
    put(t, SGR0 "\033[J\033[?25h\033[?2004l\033[<u\033[>4;0m");
    flush(t);
    t->started = 0;
    if (t->io->raw)
        t->io->raw(t->io->u, 0);
}

/* ---- keys ---- */

static void cycle_mode(cl_tui *t)
{
    if (t->mode)
        *t->mode = (*t->mode + 1) % 3;
}

static int next_key(cl_tui *t, cl_key *k, long wait)
{
    char b[256];
    long n;
    if (keys_next(&t->keys, k, 0))
        return 1;
    n = t->io->read(t->io->u, b, sizeof(b), wait);
    if (n < 0)
        return -1;
    if (n > 0)
        keys_feed(&t->keys, b, n);
    /* a sequence's bytes come in one burst: what is there now is all */
    return keys_next(&t->keys, k, 1);
}

static void tick(cl_tui *t)
{
    unsigned long now = t->io->ms ? t->io->ms(t->io->u) : 0;
    if (t->busy && now - t->tick >= 200) {
        t->tick = now;
        t->frame++;
    }
}

/* the slash menu's chosen command written into the box; 1 when it took */
static int complete(cl_tui *t)
{
    int idx[8], n = slash_matches(t, idx, 8);
    if (!n)
        return 0;
    ed_set(&t->ed, t->cmds[idx[t->msel < n ? t->msel : 0]].name);
    return 1;
}

long tui_read(cl_tui *t, char *buf, long cap)
{
    t->hint[0] = 0;
    for (;;) {
        cl_key k;
        int r;
        tui_frame(t);
        r = next_key(t, &k, 500);
        if (r < 0)
            return -1;
        if (!r) {
            check_size(t);
            continue;
        }
        if (k.k != K_CTRL || k.ch != 'c') {
            if (t->quit_armed)
                t->hint[0] = 0;
            t->quit_armed = 0;
        }
        switch (k.k) {
        case K_ENTER:
            if (t->ed.cur > 0 && t->ed.b[t->ed.cur - 1] == '\\') {
                cl_key bs;
                memset(&bs, 0, sizeof(bs));
                bs.k = K_BS;
                ed_key(&t->ed, &bs);
                ed_insert(&t->ed, "\n", 1);
                break;
            }
            complete(t);
            if (!t->ed.n)
                break;
            cl_copy(buf, t->ed.b, cap);
            ed_remember(&t->ed, t->ed.b);
            ed_clear(&t->ed);
            t->msel = 0;
            t->mclosed = 0;
            t->hint[0] = 0;
            tui_frame(t);
            return (long)strlen(buf);
        case K_TAB:
            if (complete(t))
                ed_insert(&t->ed, " ", 1);
            break;
        case K_BTAB:
            cycle_mode(t);
            break;
        case K_UP:
        case K_DOWN: {
            int idx[8], n = slash_matches(t, idx, 8);
            if (n) {
                t->msel = (t->msel + (k.k == K_UP ? n - 1 : 1)) % n;
                break;
            }
            ed_key(&t->ed, &k);
            break;
        }
        case K_ESC:
            t->mclosed = 1;
            break;
        case K_CTRL:
            if (k.ch == 'c') {
                if (t->ed.n) {
                    ed_clear(&t->ed);
                    break;
                }
                if (t->quit_armed)
                    return -1;
                t->quit_armed = 1;
                cl_copy(t->hint, "Press Ctrl+C again to exit", sizeof(t->hint));
                break;
            }
            if (k.ch == 'd' && !t->ed.n)
                return -1;
            if (k.ch == 'o') {
                t->expand = !t->expand;
                if (t->on_expand)
                    t->on_expand(t->eu);
                break;
            }
            if (k.ch == 'l') {
                t->full = 1;
                break;
            }
            ed_key(&t->ed, &k);
            break;
        case K_CPR:
            break;
        default: {
            long n0 = t->ed.n;
            ed_key(&t->ed, &k);
            if (t->ed.n != n0) {
                t->mclosed = 0;
                t->msel = 0;
            }
            break;
        }
        }
    }
}

int tui_poll(cl_tui *t)
{
    int stop = 0, r;
    cl_key k;
    while ((r = next_key(t, &k, 0)) > 0) {
        if (k.k == K_ESC || (k.k == K_CTRL && k.ch == 'c'))
            stop = 1;
        else if (k.k == K_BTAB)
            cycle_mode(t);
        else if (k.k == K_CTRL && k.ch == 'o') {
            t->expand = !t->expand;
            if (t->on_expand)
                t->on_expand(t->eu);
        } else if (k.k != K_ENTER)
            ed_key(&t->ed, &k);     /* typed ahead: waits in the box */
    }
    if (r < 0)
        stop = 1;
    tui_tick(t);
    return stop;
}

void tui_tick(cl_tui *t)
{
    check_size(t);
    tick(t);
    tui_frame(t);
}

void tui_busy(cl_tui *t, int on)
{
    t->busy = on;
    if (on) {
        t->t0 = t->tick = t->io->ms ? t->io->ms(t->io->u) : 0;
        t->tokens = 0;
        t->frame = 0;
    }
    tui_frame(t);
}

int tui_menu(cl_tui *t, const char *title, const char *question, const char *const *opt, int n, int sel,
             int esc)
{
    int busy = t->busy, choice = -1;
    t->modal = 1;
    t->m_title = title;
    t->m_q = question;
    t->m_opt = opt;
    t->m_n = n;
    t->m_sel = sel;
    t->busy = 0;
    for (;;) {
        cl_key k;
        int r;
        tui_frame(t);
        r = next_key(t, &k, 500);
        if (r < 0)
            break;
        if (!r) {
            check_size(t);
            continue;
        }
        if (k.k == K_UP)
            t->m_sel = (t->m_sel + n - 1) % n;
        else if (k.k == K_DOWN || k.k == K_TAB)
            t->m_sel = (t->m_sel + 1) % n;
        else if (k.k == K_ENTER) {
            choice = t->m_sel;
            break;
        } else if (k.k == K_ESC || (k.k == K_CTRL && k.ch == 'c')) {
            choice = esc;
            break;
        } else if (k.k == K_CHAR && k.ch >= '1' && k.ch < (unsigned long)('1' + n)) {
            choice = (int)(k.ch - '1');
            break;
        }
    }
    t->modal = 0;
    t->busy = busy;
    tui_frame(t);
    return choice;
}
