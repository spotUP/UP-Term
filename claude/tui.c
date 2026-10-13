/* tui -- see tui.h. */
#include <stdlib.h>
#include <string.h>
#include "tui.h"
#include "tools.h"
#include "conv.h"
#include "util.h"
#include "theme.h"
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

#define G_CHECK  "\342\234\224"  /* U+2714 */
#define G_SQUARE "\342\226\240"  /* U+25A0 */
#define G_CIRCLE "\342\227\213"  /* U+25CB */

#define SGR0   "\033[0m"
#define DIM    "\033[2m"
#define BOLD   "\033[1m"
#define REV    "\033[7m"

const char *const tui_mode_names[4] = { "default", "accept edits", "plan", "bypass permissions" };

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
    int i;
    memset(t, 0, sizeof(*t));
    t->io = io;
    jw_init(&t->o);
    keys_init(&t->keys);
    ed_init(&t->ed);
    get_size(t, &t->cols, &t->rows);
    t->B = t->fb = t->rows;
    t->tr = 1;
    t->ctx_left = -1;
    t->full = 1;
    t->th = theme_get(0);
    hist_init(&t->hist);
    jw_init(&t->todos);
    jw_init(&t->log);
    for (i = 0; i < TUI_DIRS; i++)
        jw_init(&t->dirs[i].names);
    t->m_btab = -1;
    t->m_afk_ms = -1;
    t->m_left = -1;
    t->m_timeout_ms = -1;
    t->m_ctx = KC_SELECT;
    t->focus = -1;
    t->notify_after_s = 10;
    t->edit_path = "T:claude-prompt.txt";
    if (km_init(&t->km))
        return -1;
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
    for (i = 0; i < t->nq; i++)
        free(t->queue[i]);
    t->nq = 0;
    for (i = 0; i < TUI_RING; i++) {
        free(t->ring[i]);
        t->ring[i] = 0;
    }
    free(t->smatch);
    t->smatch = 0;
    tui_dialog_close(t);
    free(t->saved);
    t->saved = 0;
    free(t->stash);
    t->stash = 0;
    free(t->pi_wabove);
    free(t->pi_wbelow);
    t->pi_wabove = t->pi_wbelow = 0;
    for (i = 0; i < TUI_DIRS; i++)
        jw_free(&t->dirs[i].names);
    hist_free(&t->hist);
    km_free(&t->km);
    jw_free(&t->todos);
    jw_free(&t->log);
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

/* the frame's colour: the box's mode's; a prompt's is /color's when set
 * (not on the monochrome theme: no colour there at all) */
static const char *frame(cl_tui *t)
{
    if (t->box == BOX_PROMPT && t->bar && strcmp(t->th->hl, "mono"))
        return t->bar;
    return t->box == BOX_BASH ? t->th->bash : t->box == BOX_MEMORY ? t->th->memory : t->th->box;
}

static void border(cl_tui *t, const char *l, const char *rr)
{
    row r;
    int i;
    r_init(&r, t->cols);
    r_sgr(&r, t->modal ? t->th->box : frame(t));
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
    r_sgr(&r, t->th->spin);
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

/* text with its SGR sequences kept (they take no column); other escape
 * sequences (cursor moves, OSC) dropped, controls as r_text has them */
static void r_ansi(row *r, const char *s, long n)
{
    long i = 0, run = 0;
    while (i < n) {
        if (s[i] != '\033') {
            i++;
            run++;
            continue;
        }
        r_text(r, s + i - run, run);
        run = 0;
        if (i + 1 < n && s[i + 1] == '[') {
            long e = i + 2;
            while (e < n && ((unsigned char)s[e] < 0x40 || (unsigned char)s[e] > 0x7e))
                e++;
            if (e < n && s[e] == 'm') {
                jw_raw(&r->b, s + i, e + 1 - i);
                i = e + 1;
            } else
                i = e < n ? e + 1 : n;
        } else if (i + 1 < n && s[i + 1] == ']') {
            long e = i + 2;
            while (e < n && s[e] != '\007' && !(s[e] == '\033' && e + 1 < n && s[e + 1] == '\\'))
                e++;
            i = e < n ? (s[e] == '\007' ? e + 1 : e + 2) : n;
        } else
            i += i + 1 < n ? 2 : 1;
    }
    r_text(r, s + i - run, run);
}

#define STATUS_ROWS 4           /* the statusLine command's lines drawn at most */

/* the statusLine command's output: a row per line */
static void status_text_rows(cl_tui *t)
{
    const char *p = t->status, *e;
    int k = 0;
    if (!p)
        return;
    while (*p && k < STATUS_ROWS) {
        row r;
        for (e = p; *e && *e != '\n'; e++)
            ;
        r_init(&r, t->cols);
        r_text(&r, "  ", 2);
        r_pad(&r, 2 + t->status_pad);
        r_ansi(&r, p, (long)(e - p));
        r_sgr(&r, SGR0);
        want(t, &r);
        k++;
        p = *e ? e + 1 : e;
    }
}

static void status_row(cl_tui *t)
{
    row r;
    char right[200], n[16];
    int mode = t->perm ? t->perm->mode : 0, rw, room;
    r_init(&r, t->cols);
    r_text(&r, "  ", 2);
    if (t->hint[0]) {
        r_sgr(&r, DIM);
        r_textz(&r, t->hint);
    } else if (vim_label(&t->ed) && !t->hide_vim) {
        r_sgr(&r, DIM);
        r_textz(&r, vim_label(&t->ed));
    } else if (t->box == BOX_BASH) {
        r_sgr(&r, t->th->bash);
        r_textz(&r, "! for bash mode");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (esc to leave)");
    } else if (t->box == BOX_MEMORY) {
        r_sgr(&r, t->th->memory);
        r_textz(&r, "# to memorize");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (enter: choose where)");
    } else if (mode == PERM_ACCEPT) {
        r_sgr(&r, t->th->accept);
        r_glyph(&r, G_MODE);
        r_glyph(&r, G_MODE);
        r_textz(&r, " accept edits on");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (shift+tab to cycle)");
    } else if (mode == PERM_PLAN) {
        r_sgr(&r, t->th->plan);
        r_textz(&r, "|| plan mode on");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (shift+tab to cycle)");
    } else if (mode == PERM_BYPASS) {
        r_sgr(&r, t->th->err);
        r_glyph(&r, G_MODE);
        r_glyph(&r, G_MODE);
        r_textz(&r, " bypass permissions on");
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, " (shift+tab to cycle)");
    } else if (t->nq) {
        r_sgr(&r, DIM);
        r_textz(&r, "queued: sent when Claude is done (up takes them back)");
    } else if (!t->status || !t->status[0]) {
        /* Claude Code drops this hint when a status line is configured */
        r_sgr(&r, DIM);
        r_textz(&r, "/ for commands");
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

/* the most matches C:Claude's menu shows; pi's look shows (and scrolls) all of them */
#define SLASH_MAX(t) ((t)->look == TUI_LOOK_PI ? (t)->ncmds : 8)

/* the slash commands matching the box's text: their indexes (t->smatch, which the caller must not keep past the next call);
 * *from the offset in the box where the text begins that a chosen command replaces, up to the cursor (0 without a filter hook:
 * the whole box) */
static int slash_matches_at(cl_tui *t, int **out, int max, long *from)
{
    int i, n = 0;
    const char *s = t->ed.b;
    long len = t->ed.n;
    *from = 0;
    if (t->modal || t->search || t->box != BOX_PROMPT || !t->cmds || len < 1 || t->mclosed)
        return 0;
    if (!t->slash_filter && (s[0] != '/' || memchr(s, ' ', (size_t)len) || memchr(s, '\n', (size_t)len)))
        return 0;
    if (t->ncmds > t->smatch_cap) {
        int *g = (int *)realloc(t->smatch, sizeof(int) * (size_t)t->ncmds);
        if (!g)
            return 0;
        t->smatch = g;
        t->smatch_cap = t->ncmds;
    }
    *out = t->smatch;
    if (t->slash_filter)
        return t->slash_filter(t->slash_u, t->cmds, t->ncmds, s, len, t->ed.cur, from, t->smatch);
    for (i = 0; i < t->ncmds && n < max; i++)
        if (!strncmp(t->cmds[i].name, s, (size_t)len))
            t->smatch[n++] = i;
    return n;
}

static int slash_matches(cl_tui *t, int **out, int max)
{
    long from;
    return slash_matches_at(t, out, max, &from);
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
        r_sgr(&r, i == t->msel ? t->th->sel : DIM);
        if (i == t->msel)
            r_sgr(&r, BOLD);
        r_textz(&r, t->cmds[idx[i]].name);
        r_pad(&r, 2 + wn + 3);
        r_sgr(&r, SGR0);
        r_sgr(&r, i == t->msel ? t->th->sel : DIM);
        r_textz(&r, t->cmds[idx[i]].help);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

/* a row of the box's text; Ctrl+R's query shown in reverse in the
 * match, vim's visual selection in reverse */
static void box_text(cl_tui *t, row *r, long a, long z)
{
    long m = -1, ml = (long)strlen(t->sq), sa, sz;
    if (t->search && ml) {
        const char *f = strstr(t->ed.b, t->sq);
        m = f ? (long)(f - t->ed.b) : -1;
    } else if (vim_selection(&t->ed, &sa, &sz)) {
        m = sa;
        ml = sz - sa;
    }
    if (m < 0 || m >= z || m + ml <= a) {
        r_text(r, t->ed.b + a, z - a);
        return;
    }
    if (m > a)
        r_text(r, t->ed.b + a, m - a);
    r_sgr(r, REV);
    r_text(r, t->ed.b + (m > a ? m : a), (m + ml < z ? m + ml : z) - (m > a ? m : a));
    r_sgr(r, SGR0);
    if (m + ml < z)
        r_text(r, t->ed.b + m + ml, z - (m + ml));
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
        r_sgr(&r, frame(t));
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);
        if (k == 0) {
            if (t->box == BOX_PROMPT) {
                r_glyph(&r, G_PROMPT);
            } else {
                r_sgr(&r, frame(t));
                r_text(&r, t->box == BOX_BASH ? "!" : "#", 1);
                r_sgr(&r, SGR0);
            }
            r_text(&r, " ", 1);
        } else {
            r_text(&r, "  ", 2);
        }
        box_text(t, &r, a[k], z[k]);
        if (k == 0 && !t->ed.n && t->suggest[0] && t->box == BOX_PROMPT && !t->search) {
            r_sgr(&r, DIM);
            r_text(&r, t->suggest, (long)strlen(t->suggest));
            r_sgr(&r, SGR0);
        }
        r_pad(&r, t->cols - 1);
        r_sgr(&r, frame(t));
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
    r_sgr(&r, t->th->box);
    r_glyph(&r, G_V);
    r_sgr(&r, SGR0 BOLD);
    r_text(&r, " ", 1);
    r_textz(&r, t->m_title ? t->m_title : "");
    r_sgr(&r, SGR0);
    r_pad(&r, t->cols - 1);
    r_sgr(&r, t->th->box);
    r_glyph(&r, G_V);
    r_sgr(&r, SGR0);
    want(t, &r);
    if (t->m_q && *t->m_q) {
        r_init(&r, t->cols);
        r_sgr(&r, t->th->box);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);
        r_textz(&r, t->m_q);
        r_pad(&r, t->cols - 1);
        r_sgr(&r, t->th->box);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    for (i = 0; i < t->m_n; i++) {
        char num[16];
        r_init(&r, t->cols);
        r_sgr(&r, t->th->box);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);
        if (i == t->m_sel) {
            r_sgr(&r, t->th->sel);
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
        r_sgr(&r, t->th->box);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        want(t, &r);
        if (i == t->m_noting || (i == t->m_n - 1 && t->m_comment && t->m_noting < 0)) {
            /* the comment field under its option, or the hint for it */
            r_init(&r, t->cols);
            r_sgr(&r, t->th->box);
            r_glyph(&r, G_V);
            r_sgr(&r, SGR0);
            if (i == t->m_noting) {
                r_textz(&r, "       > ");
                r_textz(&r, t->m_note);
                r_sgr(&r, REV);
                r_text(&r, " ", 1);
                r_sgr(&r, SGR0 DIM);
                r_textz(&r, "  enter answers with it, tab closes");
            } else {
                r_sgr(&r, DIM);
                r_textz(&r, "   tab on Yes or No adds a comment for Claude");
            }
            r_sgr(&r, SGR0);
            r_pad(&r, t->cols - 1);
            r_sgr(&r, t->th->box);
            r_glyph(&r, G_V);
            r_sgr(&r, SGR0);
            want(t, &r);
        }
    }
    if (t->m_left > 0) {
        /* the auto-continue countdown (askUserQuestionTimeout) */
        char m[96], num[16];
        cl_copy(m, "   No answer: going on without you in ", sizeof(m));
        cl_ltoa(t->m_left, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, "s (any key waits)", sizeof(m));
        r_init(&r, t->cols);
        r_sgr(&r, t->th->box);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0 DIM);
        r_textz(&r, m);
        r_sgr(&r, SGR0);
        r_pad(&r, t->cols - 1);
        r_sgr(&r, t->th->box);
        r_glyph(&r, G_V);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    border(t, G_BL, G_BR);
}

/* prompts typed ahead: dim, under the spinner, until they are sent */
static void queue_rows(cl_tui *t)
{
    int i;
    for (i = 0; i < t->nq && i < 3; i++) {
        row r;
        const char *q = t->queue[i], *nl = strchr(q, '\n');
        r_init(&r, t->cols);
        r_sgr(&r, DIM);
        r_textz(&r, "  > ");
        r_text(&r, q, nl ? (long)(nl - q) : (long)strlen(q));
        if (nl)
            r_textz(&r, " ...");
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    if (t->nq > 3) {
        row r;
        char m[40], n[12];
        r_init(&r, t->cols);
        cl_copy(m, "  ... and ", sizeof(m));
        cl_ltoa(t->nq - 3, n);
        cl_cat(m, n, sizeof(m));
        cl_cat(m, " more", sizeof(m));
        r_sgr(&r, DIM);
        r_textz(&r, m);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

/* the @-completion list: up to 8 rows around the selection */
static void comp_rows(cl_tui *t)
{
    int i, first = t->csel < 8 ? 0 : t->csel - 7;
    for (i = first; i < t->ncomp && i < first + 8; i++) {
        row r;
        r_init(&r, t->cols);
        r_text(&r, "  ", 2);
        r_sgr(&r, i == t->csel ? t->th->sel : DIM);
        if (t->cskip)
            r_text(&r, "@", 1);
        r_textz(&r, t->comp[i]);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

/* ?: Claude Code's shortcuts, in as many columns as the window takes */
static const char *const help_items[] = {
    "! for bash mode",          "/ for commands",          "@ for file paths",
    "# to memorize",            "\\ + enter for newline",  "double tap esc to clear input",
    "shift+tab to cycle modes", "ctrl+o for the transcript", "ctrl+t to show todos",
    "ctrl+r to search history", "ctrl+g to edit in $EDITOR", "ctrl+s to stash the prompt",
    "ctrl+_ to undo",           "ctrl+y / alt+y to paste", "ctrl+b to background a command",
    "ctrl+enter to send now",   "alt+p to switch model",   "alt+t to toggle thinking",
    "ctrl+l to redraw"
};
#define NHELP ((int)(sizeof(help_items) / sizeof(help_items[0])))

static void help_rows(cl_tui *t)
{
    int cw = 32, ncol = (t->cols - 2) / cw, nrow, i, c;
    if (ncol < 1)
        ncol = 1;
    if (ncol > 3)
        ncol = 3;
    nrow = (NHELP + ncol - 1) / ncol;
    for (i = 0; i < nrow; i++) {
        row r;
        r_init(&r, t->cols);
        r_sgr(&r, DIM);
        for (c = 0; c < ncol; c++) {
            int k = c * nrow + i;
            if (k >= NHELP)
                break;
            r_pad(&r, 2 + c * cw);
            r_textz(&r, help_items[k]);
        }
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

/* Ctrl+R: the query in place of the status line */
static void search_row(cl_tui *t)
{
    row r;
    r_init(&r, t->cols);
    r_text(&r, "  ", 2);
    r_textz(&r, t->sfail ? "(failing reverse-i-search)`" : "(reverse-i-search)`");
    r_textz(&r, t->sq);
    r_textz(&r, "': ");
    r_sgr(&r, DIM);
    r_textz(&r, "ctrl+r older, tab edit, enter send, ctrl+c cancel");
    r_sgr(&r, SGR0);
    want(t, &r);
}

/* Ctrl+T: the todo list, five rows at most, the first unfinished ones */
static void todo_rows(cl_tui *t)
{
    const char *p = t->todos.p, *e = p ? p + t->todos.n : 0;
    int k = 0, skip = 0, total = 0, done = 0;
    if (!p || !t->show_todos)
        return;
    /* skip finished items while more than five are left to show */
    for (; p < e; p++)
        if (*p == '\n')
            total++;
    for (p = t->todos.p; p < e && *p == 'c' && total - done > TUI_TODOS; done++) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(e - p));
        p = nl ? nl + 1 : e;
        skip++;
    }
    while (p < e && k < TUI_TODOS) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(e - p));
        row r;
        if (!nl)
            nl = e;
        r_init(&r, t->cols);
        r_text(&r, "  ", 2);
        if (*p == 'c') {
            r_sgr(&r, t->th->ok);
            r_glyph(&r, G_CHECK);
            r_sgr(&r, SGR0 DIM);
        } else if (*p == 'p') {
            r_sgr(&r, t->th->accent);
            r_glyph(&r, G_SQUARE);
            r_sgr(&r, SGR0 BOLD);
        } else {
            r_glyph(&r, G_CIRCLE);
        }
        r_text(&r, " ", 1);
        r_text(&r, p + 1, (long)(nl - p - 1));
        r_sgr(&r, SGR0);
        want(t, &r);
        k++;
        p = nl < e ? nl + 1 : e;
    }
    (void)skip;
}

/* ---- pi's look (TUI_LOOK_PI; amiga-pi plan N0): rows as pi's interactive mode draws them, from the
 * goldens in amiga-pi tests/pi/tui (pi 1.0.4, capture-pane -e) ---- */

static const char *const pi_spin[] = {
    "\342\240\213", "\342\240\231", "\342\240\271", "\342\240\270", "\342\240\274", "\342\240\264", "\342\240\246",
    "\342\240\247", "\342\240\207", "\342\240\217"
};
#define PI_NSPIN 10

static void pi_role(cl_tui *t, row *r, int role)
{
    char b[48];
    pitheme_sgr(t->pith, role, b);
    r_sgr(r, b);
}

/* a row of the editor's rule: the working label in it while the agent works */
static void pi_rule(cl_tui *t, int work)
{
    row r;
    char b[8];
    int i;
    r_init(&r, t->cols);
    /* a line that starts with ! is a command: the rules take the bash colour (pi updateEditorBorderColor) */
    pi_role(t, &r, t->ed.n && t->ed.b[0] == '!' ? PR_BASH_MODE : t->pi_rule);
    (void)b;
    if (work && t->pi_work[0] && t->cols >= (int)strlen(t->pi_work) + 5) {
        r_glyph(&r, G_H);
        r_glyph(&r, G_H);
        r_text(&r, " ", 1);
        r_glyph(&r, pi_spin[t->frame % PI_NSPIN]);
        r_sgr(&r, SGR0);
        r_text(&r, " ", 1);         /* pi draws the space between the spinner and its label unstyled */
        pi_role(t, &r, t->pi_rule);
        r_textz(&r, t->pi_work);
        r_text(&r, " ", 1);
    }
    for (i = r.w; i < t->cols; i++)
        r_glyph(&r, G_H);
    r_sgr(&r, SGR0);
    want(t, &r);
}

/* a rule with a scroll indicator ("─── ↑ 3 more ───"): pi's createScrollBorder */
static void pi_scroll_rule(cl_tui *t, const char *arrow, int hidden)
{
    row r;
    char label[48], num[16];
    int lw, left, i;
    r_init(&r, t->cols);
    pi_role(t, &r, t->pi_rule);
    cl_copy(label, " ", sizeof(label));
    cl_cat(label, arrow, sizeof(label));
    cl_cat(label, " ", sizeof(label));
    cl_ltoa(hidden, num);
    cl_cat(label, num, sizeof(label));
    cl_cat(label, " more ", sizeof(label));
    lw = vw_width(label, (long)strlen(label));
    if (lw + 2 <= t->cols) {
        left = (t->cols - lw) / 2;
        for (i = 0; i < left; i++)
            r_glyph(&r, G_H);
        r_textz(&r, label);
    } else {
        r_glyph(&r, G_H);
        r_glyph(&r, G_H);
        r_glyph(&r, G_H);
        r_textz(&r, label);
    }
    while (r.w < t->cols)
        r_glyph(&r, G_H);
    r_sgr(&r, SGR0);
    want(t, &r);
}

/* the text rows of the editor: no frame, the cursor a reverse cell (pi's editor.js render) */
static void pi_editor_rows(cl_tui *t)
{
    long a[64], z[64];
    int n, crow, ccol, show, maxv, i, lw = t->cols > 1 ? t->cols - 1 : 1;
    maxv = t->rows * 3 / 10;
    if (maxv < 5)
        maxv = 5;
    n = ed_layout(&t->ed, lw, a, z, 64, &crow, &ccol);
    if (n > 64)
        n = 64;
    if (crow >= 64)
        crow = 63;
    show = n < maxv ? n : maxv;
    if (crow < t->scroll)
        t->scroll = crow;
    if (crow >= t->scroll + show)
        t->scroll = crow - show + 1;
    if (t->scroll > n - show)
        t->scroll = n - show;
    if (t->scroll < 0)
        t->scroll = 0;
    if (t->busy && t->pi_work[0] && t->scroll == 0)
        pi_rule(t, 1);
    else if (t->scroll > 0)
        pi_scroll_rule(t, "\342\206\221", t->scroll);
    else
        pi_rule(t, 0);
    for (i = 0; i < show; i++) {
        row r;
        int k = t->scroll + i, w = 0;
        long b = a[k];
        r_init(&r, t->cols);
        if (k == crow) {
            /* the text up to the cursor, the cursor cell reversed (the character under it, or a space) */
            while (b < z[k] && w < ccol) {
                unsigned long cp;
                int l = vw_char(t->ed.b + b, z[k] - b, &cp);
                w += vw_cp_width(cp);
                b += l;
            }
            r_text(&r, t->ed.b + a[k], b - a[k]);
            r_sgr(&r, REV);
            if (b < z[k]) {
                unsigned long cp;
                int l = vw_char(t->ed.b + b, z[k] - b, &cp);
                r_text(&r, t->ed.b + b, l);
                b += l;
            } else
                r_text(&r, " ", 1);
            r_sgr(&r, SGR0);
            r_text(&r, t->ed.b + b, z[k] - b);
        } else
            r_text(&r, t->ed.b + a[k], z[k] - a[k]);
        want(t, &r);
    }
    if (n - (t->scroll + show) > 0)
        pi_scroll_rule(t, "\342\206\223", n - (t->scroll + show));
    else
        pi_rule(t, 0);
}

/* pi's SelectList (select-list.js): a window of five around the selection, "→ " on it, descriptions in a
 * column as wide as the widest label + 2 (12..32), "(n/total)" when there is more than the window */
static void pi_list_rows(cl_tui *t, const char *const *label, const char *const *desc, int n, int sel)
{
    int i, start, end, colw = 0, maxv = 5;
    for (i = 0; i < n; i++) {
        int w = vw_width(label[i], (long)strlen(label[i])) + 2;
        if (w > colw)
            colw = w;
    }
    colw = colw < 12 ? 12 : colw > 32 ? 32 : colw;
    start = sel - maxv / 2;
    if (start > n - maxv)
        start = n - maxv;
    if (start < 0)
        start = 0;
    end = start + maxv < n ? start + maxv : n;
    for (i = start; i < end; i++) {
        row r;
        int lw = vw_width(label[i], (long)strlen(label[i]));
        r_init(&r, t->cols);
        if (i == sel)
            pi_role(t, &r, PR_ACCENT);
        r_text(&r, i == sel ? "\342\206\222 " : "  ", i == sel ? 4 : 2);
        r.w = r.w;              /* "→" is one column: r_text counted its bytes' width */
        r_textz(&r, label[i]);
        if (desc && desc[i] && desc[i][0] && t->cols > 40) {
            int room;
            if (i != sel)
                pi_role(t, &r, PR_DIM);
            r_pad(&r, 2 + (lw + 2 > colw ? lw + 2 : colw));
            /* select-list.ts:199-203: the description gets what is left of the width less two, when that is more than ten */
            room = t->cols - r.w - 2;
            if (room > 10) {
                r.max = r.w + room;
                r_text(&r, desc[i], (long)strlen(desc[i]));
            }
        }
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    if (start > 0 || end < n) {
        row r;
        char m[48], num[16];
        r_init(&r, t->cols);
        pi_role(t, &r, PR_DIM);
        cl_copy(m, "  (", sizeof(m));
        cl_ltoa(sel + 1, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, "/", sizeof(m));
        cl_ltoa(n, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, ")", sizeof(m));
        r_textz(&r, m);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

static void pi_slash_rows(cl_tui *t, int *idx, int n)
{
    const char **label = (const char **)malloc(sizeof(char *) * 2 * (size_t)n), **desc;
    int i;
    if (!label)
        return;
    desc = label + n;
    for (i = 0; i < n; i++) {
        label[i] = t->cmds[idx[i]].name[0] == '/' ? t->cmds[idx[i]].name + 1 : t->cmds[idx[i]].name;
        desc[i] = t->cmds[idx[i]].help;
    }
    pi_list_rows(t, label, desc, n, t->msel);
    free(label);
}

static char *dup_text(const char *z)
{
    char *c;
    if (!z || !z[0])
        return 0;
    c = (char *)malloc(strlen(z) + 1);
    if (c)
        strcpy(c, z);
    return c;
}

void tui_pi_widgets(cl_tui *t, const char *above, int above_cut, const char *below, int below_cut)
{
    free(t->pi_wabove);
    free(t->pi_wbelow);
    t->pi_wabove = dup_text(above);
    t->pi_wbelow = dup_text(below);
    t->pi_wabove_cut = t->pi_wabove ? above_cut : 0;
    t->pi_wbelow_cut = t->pi_wbelow ? below_cut : 0;
}

void tui_pi_ext_status(cl_tui *t, const char *text)
{
    cl_copy(t->pi_extstat, text ? text : "", sizeof(t->pi_extstat));
}

static void pi_wrapped(cl_tui *t, const char *sgr, const char *s, int pad);

/* the widget lines (each a Text component with one column of padding: wrapped), then pi's muted note when the widgets were cut */
static void pi_widget_rows(cl_tui *t, const char *lines, int cut)
{
    const char *p = lines;
    row r;
    while (*p) {
        const char *e = strchr(p, '\n');
        char buf[400];
        long n = e ? (long)(e - p) : (long)strlen(p);
        if (n > (long)sizeof(buf) - 1)
            n = (long)sizeof(buf) - 1;
        memcpy(buf, p, (size_t)n);
        buf[n] = 0;
        pi_wrapped(t, "", buf, 1);
        p = e ? e + 1 : p + n;
        if (!e)
            break;
    }
    if (cut) {
        r_init(&r, t->cols);
        r_text(&r, " ", 1);
        pi_role(t, &r, PR_MUTED);
        r_textz(&r, "... (widget truncated)");
        r_sgr(&r, SGR0);
        want(t, &r);
    }
}

/* the bytes of s (n long) that fit in room columns */
static long cut_cols(const char *s, long n, int room)
{
    long k = 0;
    int w = 0;
    while (k < n) {
        unsigned long cp;
        int l = vw_char(s + k, n - k, &cp), c1 = vw_cp_width(cp);
        if (w + c1 > room)
            break;
        w += c1;
        k += l;
    }
    return k;
}

/* pi's footer.js: the directory, then the usage with the model on the right (padding of at least two). The usage wider than
 * the window is cut to it with "..." and the model dropped (footer.ts:171-176, 231-233); truncateToWidth puts a reset on each
 * side of the dots, so they are not dim */
static void pi_footer_rows(cl_tui *t)
{
    row r;
    int lw, cw, rw, pad;
    r_init(&r, t->cols);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, t->pi_cwd);
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    lw = vw_width(t->pi_left, (long)strlen(t->pi_left));
    cw = vw_width(t->pi_ctx, (long)strlen(t->pi_ctx));
    rw = vw_width(t->pi_right, (long)strlen(t->pi_right));
    if (lw && t->pi_ctx[0])
        lw++;                   /* the space between the usage and the context figure */
    if (lw + cw > t->cols) {
        int room = t->cols - 3;
        long k = cut_cols(t->pi_left, (long)strlen(t->pi_left), room);
        pi_role(t, &r, PR_DIM);
        r_text(&r, t->pi_left, k);
        if (t->pi_left[k] == 0 && t->pi_ctx[0] && r.w + (t->pi_left[0] ? 1 : 0) <= room) {
            if (t->pi_left[0])
                r_text(&r, " ", 1);
            if (t->pi_ctx_role)
                pi_role(t, &r, t->pi_ctx_role - 1);
            r_text(&r, t->pi_ctx, cut_cols(t->pi_ctx, (long)strlen(t->pi_ctx), room - r.w));
        }
        r_sgr(&r, SGR0);
        r_textz(&r, "...");
        r_sgr(&r, SGR0);
        want(t, &r);
        return;
    }
    pad = t->cols - lw - cw - rw;
    pi_role(t, &r, PR_DIM);
    if (t->pi_left[0]) {
        r_textz(&r, t->pi_left);
        if (t->pi_ctx[0])
            r_text(&r, " ", 1);
    }
    r_sgr(&r, SGR0);
    if (t->pi_ctx[0]) {
        if (t->pi_ctx_role)
            pi_role(t, &r, t->pi_ctx_role - 1);
        else
            pi_role(t, &r, PR_DIM);
        r_textz(&r, t->pi_ctx);
        r_sgr(&r, SGR0);
    }
    pi_role(t, &r, PR_DIM);
    if (pad < 2) {
        pad = 2;
    }
    while (pad-- > 0 && r.w < t->cols)
        r_text(&r, " ", 1);
    r_textz(&r, t->pi_right);
    r_sgr(&r, SGR0);
    want(t, &r);
    if (t->pi_extstat[0]) {
        /* footer.ts:289-298: the extensions' statuses on one row, cut to the width with a dim "..." */
        long n = (long)strlen(t->pi_extstat), w = vw_width(t->pi_extstat, n);
        r_init(&r, t->cols);
        if (w <= t->cols)
            r_textz(&r, t->pi_extstat);
        else {
            long k = cut_cols(t->pi_extstat, n, t->cols - 3);
            r_text(&r, t->pi_extstat, k);
            pi_role(t, &r, PR_DIM);
            r_textz(&r, "...");
            r_sgr(&r, SGR0);
        }
        want(t, &r);
    }
}

/* the messages waiting for the agent: "Steering: ..." then "Follow-up: ...", and the hint */
static void pi_queue_rows(cl_tui *t)
{
    int i;
    row r;
    if (!t->n_steer && !t->n_follow)
        return;
    for (i = 0; i < t->n_steer + t->n_follow; i++) {
        const char *s = i < t->n_steer ? t->pi_steer[i] : t->pi_follow[i - t->n_steer];
        r_init(&r, t->cols);
        r_text(&r, " ", 1);
        pi_role(t, &r, PR_DIM);
        r_textz(&r, i < t->n_steer ? "Steering: " : "Follow-up: ");
        r_textz(&r, s);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    pi_role(t, &r, PR_DIM);
    r_glyph(&r, "\342\206\263");
    r_textz(&r, " Option+Up to edit all queued messages");
    r_sgr(&r, SGR0);
    want(t, &r);
}

/* the words of s on rows of at most width columns (pi's Text component), one row each, to want() */
static void pi_wrapped(cl_tui *t, const char *sgr, const char *s, int pad)
{
    int width = t->cols - pad * 2, lw;
    const char *p = s;
    if (width < 8)
        width = 8;
    while (1) {
        const char *e = p;
        long n;
        int w = 0;
        row r;
        const char *brk = 0;
        while (*e && *e != '\n') {
            unsigned long cp;
            int l = vw_char(e, (long)strlen(e), &cp), cw = vw_cp_width(cp);
            if (w + cw > width)
                break;
            if (cp == ' ')
                brk = e;
            w += cw;
            e += l;
        }
        if (*e == ' ') {
            n = (long)(e - p);          /* the line is full and a space follows: it ends there */
            lw = 1;
        } else if (*e && *e != '\n' && brk && brk > p) {
            n = (long)(brk - p);
            lw = 1;
        } else {
            n = (long)(e - p);
            lw = 0;
        }
        r_init(&r, t->cols);
        r_text(&r, "          ", pad);
        r_sgr(&r, sgr);
        r_text(&r, p, n);
        r_sgr(&r, SGR0);
        want(t, &r);
        p += n + lw;
        if (*p == '\n')
            p++;
        else if (!*p && !(lw && 0))
            break;
        if (!*p && (e == p || *(p - 1) != '\n'))
            break;
    }
}

/* a question (tui_menu): pi's selector -- a rule in the border colour, the title in bold, the options with
 * "→" on the choice, the keys, a rule */
static void pi_menu_rows(cl_tui *t)
{
    row r;
    int i;
    char b[48];
    r_init(&r, t->cols);
    pitheme_sgr(t->pith, PR_BORDER, b);
    r_sgr(&r, b);
    for (i = 0; i < t->cols; i++)
        r_glyph(&r, G_H);
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    want(t, &r);
    if (t->m_ext) {
        /* pi's ExtensionSelectorComponent: the title in bold and the accent colour, with the seconds left when it times out */
        char sgr[64], acc[48], title[600];
        cl_copy(sgr, BOLD, sizeof(sgr));
        pitheme_sgr(t->pith, PR_ACCENT, acc);
        cl_cat(sgr, acc, sizeof(sgr));
        cl_copy(title, t->m_title ? t->m_title : "", sizeof(title));
        if (t->m_left > 0) {
            char num[16];
            cl_cat(title, " (", sizeof(title));
            cl_ltoa(t->m_left, num);
            cl_cat(title, num, sizeof(title));
            cl_cat(title, "s)", sizeof(title));
        }
        pi_wrapped(t, sgr, title, 1);
    } else
        pi_wrapped(t, BOLD, t->m_title ? t->m_title : "", 1);
    if (t->m_q && *t->m_q) {
        r_init(&r, t->cols);
        want(t, &r);
        pi_wrapped(t, BOLD, t->m_q, 1);
    }
    r_init(&r, t->cols);
    want(t, &r);
    for (i = 0; i < t->m_n; i++) {
        r_init(&r, t->cols);
        r_text(&r, " ", 1);
        if (t->m_ext) {
            pi_role(t, &r, i == t->m_sel ? PR_ACCENT : PR_TEXT);
            r_text(&r, i == t->m_sel ? "\342\206\222 " : "  ", i == t->m_sel ? 4 : 2);
            r_textz(&r, t->m_opt[i]);
            r_sgr(&r, SGR0);
        } else {
            r_text(&r, i == t->m_sel ? "\342\206\222 " : "  ", i == t->m_sel ? 4 : 2);
            r_textz(&r, t->m_opt[i]);
        }
        want(t, &r);
    }
    r_init(&r, t->cols);
    want(t, &r);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "\342\206\221\342\206\223 navigate");
    r_sgr(&r, SGR0);
    r_text(&r, "  ", 2);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "enter select");
    r_sgr(&r, SGR0);
    r_text(&r, "  ", 2);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "escape/ctrl+c cancel");
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    want(t, &r);
    r_init(&r, t->cols);
    r_sgr(&r, b);
    for (i = 0; i < t->cols; i++)
        r_glyph(&r, G_H);
    r_sgr(&r, SGR0);
    want(t, &r);
}

static void pi_rule_row(cl_tui *t, int role)
{
    row r;
    int i;
    r_init(&r, t->cols);
    pi_role(t, &r, role);
    for (i = 0; i < t->cols; i++)
        r_glyph(&r, G_H);
    r_sgr(&r, SGR0);
    want(t, &r);
}

static void pi_blank_row(cl_tui *t)
{
    row r;
    r_init(&r, t->cols);
    want(t, &r);
}

/* pi's OAuthSelectorComponent (modes/interactive/components/oauth-selector.ts:75-130, updateList 147-190) */
static void pi_auth_pick_rows(cl_tui *t)
{
    const tui_pick *p = t->pick;
    row r;
    int i, start, end, maxv = p->maxrows > 0 ? p->maxrows : 8;
    char line[600], num[16];
    pi_rule_row(t, PR_BORDER);
    pi_blank_row(t);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    r_sgr(&r, BOLD);
    pi_role(t, &r, PR_ACCENT);
    r_textz(&r, p->title);
    r_sgr(&r, SGR0);
    want(t, &r);
    pi_blank_row(t);
    r_init(&r, t->cols);
    r_text(&r, "> ", 2);
    {
        /* a query longer than the row shows its end, from a character boundary */
        const char *shown = p->query;
        long l = (long)strlen(shown), room = (long)t->cols - 4;
        if (room > 0 && l > room) {
            shown += l - room;
            while (*shown && ((unsigned char)*shown & 0xc0) == 0x80)
                shown++;
        }
        r_textz(&r, shown);
    }
    r_sgr(&r, REV);
    r_text(&r, " ", 1);
    r_sgr(&r, SGR0);
    want(t, &r);
    pi_blank_row(t);
    start = p->sel - maxv / 2;
    if (start > p->n - maxv)
        start = p->n - maxv;
    if (start < 0)
        start = 0;
    end = start + maxv < p->n ? start + maxv : p->n;
    for (i = start; i < end; i++) {
        line[0] = 0;
        p->row(p->u, i, i == p->sel, line, sizeof(line));
        r_init(&r, t->cols);
        r_text(&r, " ", 1);
        r_ansi(&r, line, (long)strlen(line));
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    if (start > 0 || end < p->n) {
        r_init(&r, t->cols);
        r_text(&r, " ", 1);
        pi_role(t, &r, PR_MUTED);
        cl_copy(line, "  (", sizeof(line));
        cl_ltoa(p->sel + 1, num);
        cl_cat(line, num, sizeof(line));
        cl_cat(line, "/", sizeof(line));
        cl_ltoa(p->n, num);
        cl_cat(line, num, sizeof(line));
        cl_cat(line, ")", sizeof(line));
        r_textz(&r, line);
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    if (!p->n) {
        r_init(&r, t->cols);
        r_text(&r, " ", 1);
        pi_role(t, &r, PR_MUTED);
        r_text(&r, "  ", 2);
        r_textz(&r, p->empty ? p->empty : "No matching providers");
        r_sgr(&r, SGR0);
        want(t, &r);
    }
    pi_blank_row(t);
    pi_rule_row(t, PR_BORDER);
}

static void pi_pick_rows(cl_tui *t)
{
    const tui_pick *p = t->pick;
    row r;
    int i, start, end, maxv = p->maxrows > 0 ? p->maxrows : 5;
    char line[600], num[16];
    if (p->layout == TUI_PICK_AUTH) {
        pi_auth_pick_rows(t);
        return;
    }
    pi_blank_row(t);
    pi_rule_row(t, PR_BORDER);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    r_sgr(&r, BOLD);
    r_text(&r, "  ", 2);
    r_textz(&r, p->title);
    r_sgr(&r, SGR0);
    want(t, &r);
    pi_wrapped(t, DIM, p->help, 0);
    r_init(&r, t->cols);
    r_text(&r, "  ", 2);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "Type to search:");
    r_sgr(&r, SGR0);
    if (p->query[0]) {
        r_text(&r, " ", 1);
        pi_role(t, &r, PR_ACCENT);
        r_textz(&r, p->query);
        r_sgr(&r, SGR0);
    }
    want(t, &r);
    pi_rule_row(t, PR_BORDER);
    pi_blank_row(t);
    if (!p->n) {
        r_init(&r, t->cols);
        pi_role(t, &r, PR_DIM);
        r_textz(&r, "  No entries found");
        r_sgr(&r, SGR0);
        want(t, &r);
    } else {
        start = p->sel - maxv / 2;
        if (start > p->n - maxv)
            start = p->n - maxv;
        if (start < 0)
            start = 0;
        end = start + maxv < p->n ? start + maxv : p->n;
        for (i = start; i < end; i++) {
            line[0] = 0;
            p->row(p->u, i, i == p->sel, line, sizeof(line));
            r_init(&r, t->cols);
            r_ansi(&r, line, (long)strlen(line));
            r_sgr(&r, SGR0);
            want(t, &r);
        }
    }
    r_init(&r, t->cols);
    pi_role(t, &r, PR_DIM);
    cl_copy(line, "  (", sizeof(line));
    cl_ltoa(p->n ? p->sel + 1 : 0, num);
    cl_cat(line, num, sizeof(line));
    cl_cat(line, "/", sizeof(line));
    cl_ltoa(p->n, num);
    cl_cat(line, num, sizeof(line));
    cl_cat(line, ")", sizeof(line));
    cl_cat(line, p->status, sizeof(line));
    r_textz(&r, line);
    r_sgr(&r, SGR0);
    want(t, &r);
    pi_blank_row(t);
    pi_rule_row(t, PR_BORDER);
}

/* pi's ExtensionInputComponent: a rule, the title in the accent colour (the seconds left after it), the input, the keys, a rule */
static void pi_ext_input_rows(cl_tui *t)
{
    row r;
    char title[300];
    pi_rule_row(t, PR_BORDER);
    r_init(&r, t->cols);
    want(t, &r);
    cl_copy(title, t->pi_in_title, sizeof(title));
    if (t->m_left > 0) {
        char num[16];
        cl_cat(title, " (", sizeof(title));
        cl_ltoa(t->m_left, num);
        cl_cat(title, num, sizeof(title));
        cl_cat(title, "s)", sizeof(title));
    }
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    pi_role(t, &r, PR_ACCENT);
    r_textz(&r, title);
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    want(t, &r);
    r_init(&r, t->cols);
    r_text(&r, "> ", 2);
    r_textz(&r, t->pi_in);
    r_sgr(&r, REV);
    r_text(&r, " ", 1);
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    want(t, &r);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "enter submit");
    r_sgr(&r, SGR0);
    r_text(&r, "  ", 2);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "escape/ctrl+c cancel");
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    want(t, &r);
    pi_rule_row(t, PR_BORDER);
}

/* ---- the sign-in dialog ---- */

typedef struct dlg_line {
    int kind, role;
    char *text;
} dlg_line;

struct tui_dialog {
    char title[200];
    dlg_line *l;
    int n, cap;
};

void tui_dialog_open(cl_tui *t, const char *title)
{
    struct tui_dialog *d;
    tui_dialog_close(t);
    d = (struct tui_dialog *)calloc(1, sizeof(*d));
    if (!d)
        return;
    cl_copy(d->title, title, sizeof(d->title));
    t->dlg = d;
    tui_frame(t);
}

void tui_dialog_clear(cl_tui *t)
{
    int i;
    if (!t->dlg)
        return;
    for (i = 0; i < t->dlg->n; i++)
        free(t->dlg->l[i].text);
    t->dlg->n = 0;
    tui_frame(t);
}

void tui_dialog_add(cl_tui *t, int kind, int role, const char *text)
{
    struct tui_dialog *d = t->dlg;
    char *copy;
    if (!d)
        return;
    if (d->n == d->cap) {
        int cap = d->cap ? d->cap * 2 : 16;
        dlg_line *g = (dlg_line *)realloc(d->l, sizeof(dlg_line) * (size_t)cap);
        if (!g)
            return;
        d->l = g;
        d->cap = cap;
    }
    copy = (char *)malloc(strlen(text ? text : "") + 1);
    if (!copy)
        return;
    strcpy(copy, text ? text : "");
    d->l[d->n].kind = kind;
    d->l[d->n].role = role;
    d->l[d->n].text = copy;
    d->n++;
    tui_frame(t);
}

void tui_dialog_close(cl_tui *t)
{
    int i;
    if (!t->dlg)
        return;
    for (i = 0; i < t->dlg->n; i++)
        free(t->dlg->l[i].text);
    free(t->dlg->l);
    free(t->dlg);
    t->dlg = 0;
    if (t->started)
        tui_frame(t);
}

static void pi_dialog_rows(cl_tui *t)
{
    const struct tui_dialog *d = t->dlg;
    row r;
    int i, last_in = -1, typing = t->modal == 3 && t->pi_in_ext == 2;
    pi_rule_row(t, PR_BORDER);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    r_sgr(&r, BOLD);
    pi_role(t, &r, PR_ACCENT);
    r_textz(&r, d->title);
    r_sgr(&r, SGR0);
    want(t, &r);
    for (i = 0; i < d->n; i++)
        if (d->l[i].kind == TUI_DLG_INPUT)
            last_in = i;
    for (i = 0; i < d->n; i++) {
        const dlg_line *x = &d->l[i];
        if (x->kind == TUI_DLG_BLANK) {
            pi_blank_row(t);
        } else if (x->kind == TUI_DLG_TEXT) {
            char sgr[48];
            pitheme_sgr(t->pith, x->role, sgr);
            pi_wrapped(t, sgr, x->text, 1);
        } else if (x->kind == TUI_DLG_ANSI) {
            r_init(&r, t->cols);
            r_text(&r, " ", 1);
            r_ansi(&r, x->text, (long)strlen(x->text));
            r_sgr(&r, SGR0);
            want(t, &r);
        } else {
            const char *shown = typing && i == last_in ? t->pi_in : x->text;
            long l = (long)strlen(shown), room = (long)t->cols - 4;
            r_init(&r, t->cols);
            r_text(&r, "> ", 2);
            if (room > 0 && l > room) {
                shown += l - room;
                while (*shown && ((unsigned char)*shown & 0xc0) == 0x80)
                    shown++;
            }
            r_textz(&r, shown);
            if (typing && i == last_in) {
                r_sgr(&r, REV);
                r_text(&r, " ", 1);
                r_sgr(&r, SGR0);
            }
            want(t, &r);
        }
    }
    pi_rule_row(t, PR_BORDER);
}

static void pi_input_rows(cl_tui *t)
{
    row r;
    if (t->pi_in_ext == 2) {
        pi_dialog_rows(t);
        return;
    }
    if (t->pi_in_ext) {
        pi_ext_input_rows(t);
        return;
    }
    pi_blank_row(t);
    pi_rule_row(t, PR_BORDER);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    r_sgr(&r, BOLD);
    r_textz(&r, t->pi_in_title);
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    {
        /* a line longer than the row shows its end, from a character boundary */
        const char *shown = t->pi_in;
        long l = (long)strlen(shown), room = (long)t->cols - 4;
        if (room > 0 && l > room) {
            shown += l - room;
            while (*shown && ((unsigned char)*shown & 0xc0) == 0x80)
                shown++;
        }
        r_textz(&r, shown);
    }
    r_sgr(&r, REV);
    r_text(&r, " ", 1);
    r_sgr(&r, SGR0);
    want(t, &r);
    r_init(&r, t->cols);
    r_text(&r, " ", 1);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, t->pi_in_hint ? t->pi_in_hint : "enter save");
    r_sgr(&r, SGR0);
    r_text(&r, "  ", 2);
    pi_role(t, &r, PR_DIM);
    r_textz(&r, "escape cancel");
    r_sgr(&r, SGR0);
    want(t, &r);
    pi_rule_row(t, PR_BORDER);
}

static void build_pi(cl_tui *t)
{
    int *idx = 0, nm;
    t->nwant = 0;
    t->crow = 0;
    if (t->dlg && !t->modal) {
        pi_dialog_rows(t);
        pi_footer_rows(t);
        return;
    }
    if (t->modal) {
        if (t->modal == 2 && t->pick)
            pi_pick_rows(t);
        else if (t->modal == 3)
            pi_input_rows(t);
        else
            pi_menu_rows(t);
        if (!t->pi_nofoot)
            pi_footer_rows(t);
        return;
    }
    pi_queue_rows(t);
    {
        row r;
        r_init(&r, t->cols);           /* the blank row between the transcript and the editor (the widgets' Spacer(1)) */
        want(t, &r);
    }
    if (t->pi_wabove)
        pi_widget_rows(t, t->pi_wabove, t->pi_wabove_cut);
    pi_editor_rows(t);
    if (t->pi_wbelow)
        pi_widget_rows(t, t->pi_wbelow, t->pi_wbelow_cut);
    nm = slash_matches(t, &idx, SLASH_MAX(t));
    if (t->msel >= nm)
        t->msel = nm ? nm - 1 : 0;
    if (nm)
        pi_slash_rows(t, idx, nm);
    pi_footer_rows(t);
}

static void build(cl_tui *t)
{
    int *idx = 0, nm;
    if (t->look == TUI_LOOK_PI) {
        build_pi(t);
        return;
    }
    t->nwant = 0;
    t->crow = 0;
    if (t->busy)
        spinner_row(t);
    if (t->modal) {
        menu_rows(t);
        return;
    }
    queue_rows(t);
    box_rows(t, t->nwant);
    if (t->copen && t->ncomp) {
        comp_rows(t);
        return;
    }
    if (t->show_help) {
        help_rows(t);
        return;
    }
    nm = slash_matches(t, &idx, SLASH_MAX(t));
    if (t->msel >= nm)
        t->msel = nm ? nm - 1 : 0;
    if (nm) {
        slash_rows(t, idx, nm);
        return;
    }
    todo_rows(t);
    status_text_rows(t);
    if (t->search)
        search_row(t);
    else
        status_row(t);
}

/* ---- drawing ---- */

/* the row above the footer's first: its place at the bottom (newB), or in REGULAR mode right under the
 * transcript while the window is not yet full (pi's regular mode: the editor follows the content) */
static int foot_top(cl_tui *t, int newB)
{
    if (t->screen == TUI_SCREEN_REGULAR && t->tr - 1 < newB)
        return t->tr - 1 < 0 ? 0 : t->tr - 1;
    return newB;
}

/* the transcript region ends at newB: the footer grows (the transcript
 * scrolls up if it would be covered) or shrinks */
static void set_bottom(cl_tui *t, int newB, int newFb)
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
        if (newFb > newB)
            newFb = newB;
    }
    {
        /* what was the footer and is not any more (a smaller footer: the
         * rows the transcript gets back) is cleared with what is the
         * footer now */
        int from = newB < t->B ? newB : t->B;
        if (t->fb < from)
            from = t->fb;
        if (newFb < from)
            from = newFb;
        if (t->screen == TUI_SCREEN_REGULAR)    /* the footer follows the transcript: what is below its end is the footer's or blank */
            from = (t->tr < newFb + 1 ? t->tr : newFb + 1) - 1;
        from++;
        t->n_full++;
        t->B = newB;
        t->fb = newFb;
        region(t, newB);
        cup(t, from <= t->rows ? from : t->rows, 1);
        put(t, SGR0 "\033[J");
    }
    t->full = 1;
}

static void fs_draw(cl_tui *t);

void tui_frame(cl_tui *t)
{
    int i, newB, newFb, any = 0;
    if (!t->started)
        return;
    build(t);
    newB = t->rows - t->nwant;
    newFb = foot_top(t, newB);
    if (newB == t->B && newFb == t->fb && !t->full && !t->view_dirty && t->nwant == t->ndrawn && t->crow == t->lrow &&
        t->ccol == t->lcol) {
        for (i = 0; i < t->nwant; i++)
            if (!t->drawn[i] || strcmp(t->drawn[i], t->want[i]))
                break;
        if (i == t->nwant)
            return;                 /* nothing changed: nothing sent */
    }
    t->lrow = t->crow;
    t->lcol = t->ccol;
    put(t, "\033[?2026h\033[?25l");
    if (newB != t->B || newFb != t->fb || t->full) {
        set_bottom(t, newB, newFb);
        any = 1;
    }
    if (t->view_dirty) {
        fs_draw(t);
        t->full = 1;
        any = 1;
    }
    for (i = 0; i < t->nwant; i++) {
        if (!t->full && i < t->ndrawn && t->drawn[i] && !strcmp(t->drawn[i], t->want[i]))
            continue;
        cup(t, t->fb + 1 + i, 1);
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
        cup(t, t->fb + t->crow, t->ccol);
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

/* Ctrl+L's copy of what the transcript shows */
static void ring_add(cl_tui *t, const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (!d)
        return;
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    free(t->ring[t->ringat]);
    t->ring[t->ringat] = d;
    t->ringat = (t->ringat + 1) % TUI_RING;
    if (t->nring < TUI_RING)
        t->nring++;
}

void tui_log(cl_tui *t, const char *s, long n)
{
    if (t->log.n + n > TUI_LOG_MAX && t->log.n) {
        /* the oldest half goes, at a line's end */
        long cut = t->log.n / 2;
        const char *nl = (const char *)memchr(t->log.p + cut, '\n', (size_t)(t->log.n - cut));
        cut = nl ? (long)(nl + 1 - t->log.p) : t->log.n;
        memmove(t->log.p, t->log.p + cut, (size_t)(t->log.n - cut) + 1);
        t->log.n -= cut;
    }
    jw_raw(&t->log, s, n);
    if (n && s[n - 1] != '\n')
        jw_raw(&t->log, "\n", 1);
}

static void bar_draw(cl_tui *t, long total);
static long log_rows(cl_tui *t);

void tui_lines(cl_tui *t, const char *s, long n)
{
    long a = 0, i;
    int r0 = t->tr, rows = 0, first = 1;
    int hold = t->screen == TUI_SCREEN_FULL && t->view > 0;     /* scrolled back: the viewport stays where it is */
    int float_foot = t->screen == TUI_SCREEN_REGULAR && r0 <= t->B;
    if (!t->nolog)
        tui_log(t, s, n);
    if (!t->started) {
        t->io->write(t->io->u, s, n);
        return;
    }
    if (!hold) {
        put(t, "\033[?2026h\033[?25l");
        if (float_foot) {
            /* the footer follows the transcript (REGULAR): the lines go where it is, it is drawn again under them */
            cup(t, r0, 1);
            put(t, SGR0 "\033[J");
            t->full = 1;
        } else if (r0 > t->B) {
            cup(t, t->B, 1);
            put(t, "\n");
        } else {
            cup(t, r0, 1);
        }
    }
    for (i = 0; i <= n; i++) {
        if (i < n && s[i] != '\n')
            continue;
        if (i == n && a == n)
            break;
        {
            int w = tui_width(s + a, i - a), k = w ? (w + t->cols - 1) / t->cols : 1;
            if (!hold) {
                if (!first)
                    put(t, "\r\n");
                jw_raw(&t->o, s + a, i - a);
                put(t, SGR0);
            }
            ring_add(t, s + a, i - a);
            rows += k;
            first = 0;
            t->n_lines++;
        }
        a = i + 1;
    }
    if (hold) {
        t->view += rows;            /* the same text stays in view */
        t->view_dirty = 1;
        tui_frame(t);
        return;
    }
    if (r0 > t->B)
        t->tr = t->B + 1;
    else
        t->tr = r0 + rows > t->B + 1 ? t->B + 1 : r0 + rows;
    if (t->screen == TUI_SCREEN_FULL && t->scrollbar)
        bar_draw(t, log_rows(t));
    if (float_foot) {
        tui_frame(t);               /* closes the frame opened above */
        return;
    }
    /* the cursor back in the box */
    if (t->crow) {
        cup(t, t->fb + t->crow, t->ccol);
        put(t, "\033[?25h");
    }
    put(t, "\033[?2026l");
    flush(t);
}

static void ring_clear(cl_tui *t)
{
    int i;
    for (i = 0; i < TUI_RING; i++) {
        free(t->ring[i]);
        t->ring[i] = 0;
    }
    t->nring = t->ringat = 0;
}

void tui_clear(cl_tui *t)
{
    ring_clear(t);
    jw_reset(&t->log);
    if (!t->started)
        return;
    put(t, "\033[r\033[H\033[2J");
    t->B = t->fb = t->rows;
    t->tr = 1;
    t->view = 0;
    t->full = 1;
    tui_frame(t);
}

void tui_redraw(cl_tui *t)
{
    int k, used = 0, first = t->nring;
    if (!t->started)
        return;
    t->n_redraws++;
    if (t->screen == TUI_SCREEN_FULL) {     /* the viewport is drawn from the whole log at the next frame */
        t->view_dirty = 1;
        tui_frame(t);
        return;
    }
    put(t, "\033[?2026h\033[r\033[H\033[2J");
    /* the newest lines that fit the transcript's rows, oldest first */
    for (k = 0; k < t->nring; k++) {
        const char *l = t->ring[(t->ringat - 1 - k + TUI_RING * 2) % TUI_RING];
        int w = tui_width(l, (long)strlen(l)), rows = w ? (w + t->cols - 1) / t->cols : 1;
        if (used + rows > t->B)
            break;
        used += rows;
        first = t->nring - 1 - k;
    }
    for (k = first; k < t->nring; k++) {
        const char *l = t->ring[(t->ringat - t->nring + k + TUI_RING * 2) % TUI_RING];
        if (k > first)
            put(t, "\r\n");
        put(t, l);
        put(t, SGR0);
    }
    t->tr = used + 1;
    t->full = 1;
    tui_frame(t);
}

/* the last k lines of the log (FULL's viewport is drawn from it, so what is taken back leaves it too) */
static void log_pop(cl_tui *t, long k)
{
    long e = t->log.n;
    while (k > 0 && e > 0) {
        e--;                        /* the newline that ends the last line */
        while (e > 0 && t->log.p[e - 1] != '\n')
            e--;
        k--;
    }
    t->log.n = e;
    if (t->log.p)
        t->log.p[e] = 0;
}

int tui_takeback(cl_tui *t, long mark)
{
    long k = t->n_lines - mark;
    if (k < 0 || k > t->nring)
        return -1;
    if (t->screen == TUI_SCREEN_FULL && !t->nolog)
        log_pop(t, k);
    for (; k > 0; k--) {
        t->ringat = (t->ringat - 1 + TUI_RING) % TUI_RING;
        free(t->ring[t->ringat]);
        t->ring[t->ringat] = 0;
        t->nring--;
        t->n_lines--;               /* counted out again: a later mark still points into the copy */
    }
    tui_redraw(t);
    return 0;
}

int tui_replace_tail(cl_tui *t, long mark, const char *s, long n)
{
    long k = t->n_lines - mark;
    int rows_old = 0, j, onscreen;
    if (k < 0 || k > t->nring)
        return -1;
    for (j = 0; j < k; j++) {
        const char *l = t->ring[(t->ringat - 1 - j + TUI_RING * 2) % TUI_RING];
        int w = tui_width(l, (long)strlen(l));
        rows_old += w ? (w + t->cols - 1) / t->cols : 1;
    }
    /* the old lines out of the copy and the log; the new ones go in through tui_lines */
    if (!t->nolog)
        log_pop(t, k);
    for (; k > 0; k--) {
        t->ringat = (t->ringat - 1 + TUI_RING) % TUI_RING;
        free(t->ring[t->ringat]);
        t->ring[t->ringat] = 0;
        t->nring--;
        t->n_lines--;
    }
    onscreen = t->started && !(t->screen == TUI_SCREEN_FULL && t->view > 0) && rows_old < t->tr;
    if (onscreen) {
        /* they are the end of what the screen shows: the cursor goes to their first row and the rest is wiped
         * with the footer, which is drawn again */
        t->tr -= rows_old;
        put(t, "\033[?2026h");
        cup(t, t->tr, 1);
        put(t, SGR0 "\033[J");
        t->full = 1;
        put(t, "\033[?2026l");
        tui_lines(t, s, n);
        tui_frame(t);               /* the footer under them again */
        return 0;
    }
    if (t->started) {
        /* scrolled out of sight (or off the top): the screen is drawn again from the copy */
        long a = 0, i;
        if (!t->nolog)
            tui_log(t, s, n);
        for (i = 0; i <= n; i++) {
            if (i < n && s[i] != '\n')
                continue;
            if (i == n && a == n)
                break;
            ring_add(t, s + a, i - a);
            t->n_lines++;
            a = i + 1;
        }
        tui_redraw(t);
        return 0;
    }
    tui_lines(t, s, n);
    return 0;
}

/* ---- FULL: the transcript's viewport (pi's alternate screen) ---- */

static int line_rows(cl_tui *t, const char *l, long n)
{
    int w = tui_width(l, n);
    return w ? (w + t->cols - 1) / t->cols : 1;
}

/* the rows the whole log takes */
static long log_rows(cl_tui *t)
{
    long i, a = 0, r = 0;
    for (i = 0; i < t->log.n; i++)
        if (t->log.p[i] == '\n') {
            r += line_rows(t, t->log.p + a, i - a);
            a = i + 1;
        }
    return r;
}

/* pi's scrollbar (pi-tui layout.js getScrollbarGeometry): the track a "|" down the last column, the thumb a
 * "heavy |" at least two rows tall, its place as far down the track as the viewport is down the transcript */
static void bar_draw(cl_tui *t, long total)
{
    long track = t->B, thumb, maxscroll, top, i;
    char sg[48];
    if (track < 1 || total <= track || !t->pith)
        return;
    thumb = (track * track + total / 2) / total;
    if (thumb > track)
        thumb = track;
    if (thumb < (track < 2 ? track : 2))
        thumb = track < 2 ? track : 2;
    maxscroll = total - track;
    top = (((maxscroll - t->view) * (track - thumb)) * 2 + maxscroll) / (2 * maxscroll);
    for (i = 0; i < track; i++) {
        int on = i >= top && i < top + thumb;
        cup(t, (int)i + 1, t->cols);
        pitheme_sgr(t->pith, on ? PR_SCROLLBAR_THUMB : PR_SCROLLBAR_TRACK, sg);
        put(t, sg);
        put(t, on ? "\342\224\203" : "\342\224\202");
        put(t, SGR0);
    }
}

/* the viewport drawn from the log: the lines whose rows are the transcript's `view` rows back from its end */
static void fs_draw(cl_tui *t)
{
    long total = log_rows(t), maxv, top, i, a = 0, row = 0;
    int used = 0;
    t->view_dirty = 0;
    maxv = total > t->B ? total - t->B : 0;
    if (t->view > maxv)
        t->view = maxv;
    if (t->view < 0)
        t->view = 0;
    top = total - t->B - t->view;
    if (top < 0)
        top = 0;
    t->n_redraws++;
    put(t, "\033[H\033[2J");
    for (i = 0; i < t->log.n; i++) {
        int k;
        if (t->log.p[i] != '\n')
            continue;
        k = line_rows(t, t->log.p + a, i - a);
        if (row >= top) {
            if (used + k > t->B)
                break;
            if (used)
                put(t, "\r\n");
            jw_raw(&t->o, t->log.p + a, i - a);
            put(t, SGR0);
            used += k;
        }
        row += k;
        a = i + 1;
    }
    t->tr = used + 1;
    if (t->view > 0) {
        /* pi's indicator while the viewport is not at the end, centred on the last row */
        static const char jump[] = "\342\206\223 Jump to latest message \302\267 Ctrl+End";
        int w = tui_width(jump, (long)sizeof(jump) - 1);
        if (t->B >= 1 && t->cols > w) {
            cup(t, t->B, (t->cols - w) / 2 + 1);
            put(t, jump);
        }
    }
    if (t->scrollbar)
        bar_draw(t, total);
}

void tui_scroll(cl_tui *t, int act)
{
    long page, total, maxv;
    if (t->screen != TUI_SCREEN_FULL || !t->started)
        return;
    total = log_rows(t);
    maxv = total > t->B ? total - t->B : 0;
    page = t->B > 4 ? t->B - 4 : 1;         /* pi-tui tui-alt-screen.js PAGE_SCROLL_OVERLAP 4 */
    switch (act) {
    case KA_VIEW_PGUP: t->view += page; break;
    case KA_VIEW_PGDN: t->view -= page; break;
    case KA_VIEW_HALF_UP: t->view += t->B > 1 ? t->B / 2 : 1; break;
    case KA_VIEW_HALF_DN: t->view -= t->B > 1 ? t->B / 2 : 1; break;
    case KA_VIEW_LINE_UP: t->view += 1; break;
    case KA_VIEW_LINE_DN: t->view -= 1; break;
    case KA_VIEW_TOP: t->view = maxv; break;
    case KA_VIEW_BOTTOM: t->view = 0; break;
    default: return;
    }
    if (t->view > maxv)
        t->view = maxv;
    if (t->view < 0)
        t->view = 0;
    t->view_dirty = 1;
    tui_frame(t);
}

int tui_resized(cl_tui *t)
{
    int c, r;
    get_size(t, &c, &r);
    if (c == t->cols && r == t->rows)
        return 0;
    t->cols = c;
    t->rows = r;
    put(t, "\033[r");
    t->B = t->fb = r;
    if (t->tr > r)
        t->tr = r;
    if (t->screen == TUI_SCREEN_FULL)
        t->view_dirty = 1;          /* the lines wrap differently: the viewport is drawn again */
    t->full = 1;
    return 1;
}

static void check_size(cl_tui *t)
{
    tui_resized(t);
}

void tui_title(cl_tui *t, const char *s)
{
    if (t->look == TUI_LOOK_PI)
        s = t->pi_title;        /* pi's title is the program's text and does not follow the turn */
    if (!t->started || !s[0] || !strcmp(t->title, s))
        return;
    cl_copy(t->title, s, sizeof(t->title));
    put(t, "\033]2;");
    put(t, s);
    put(t, "\a");
    flush(t);
}

void tui_raw(cl_tui *t, const char *s, long n)
{
    if (!t->started || n <= 0)
        return;
    jw_raw(&t->o, s, n);
    flush(t);
}

void tui_notify(cl_tui *t, const char *msg)
{
    /* pi 1.0.4 sends nothing when a turn ends or a question is asked: no bell, no OSC 9 (its raw output stream,
     * tests/pi/tui/turn_raw.raw); only an extension can ring it */
    if (!t->started || t->look == TUI_LOOK_PI)
        return;
    /* the bell, then OSC 9 (UP-Term, iTerm2, kitty: a desktop notice) */
    put(t, "\a\033]9;");
    put(t, msg);
    put(t, "\a");
    flush(t);
    t->n_notify++;
}

/* bracketed paste, kitty's disambiguation, modifyOtherKeys, and (A4 gaps
 * 3) focus reports: away from the window is when a question may go on
 * without the user and a recap is made */
static const char modes_on[] = "\033[?2004h\033[>1u\033[>4;1m\033[?1004h";
static const char modes_off[] = "\033[?2004l\033[<u\033[>4;0m\033[?1004l";

int tui_start(cl_tui *t)
{
    unsigned long t0;
    if (t->io->raw && t->io->raw(t->io->u, 1))
        return -1;
    get_size(t, &t->cols, &t->rows);
    /* bracketed paste, kitty's disambiguation (Esc and Shift+Enter come
     * as CSI u), modifyOtherKeys 1 for terminals without it; where is the
     * cursor? */
    if (t->screen == TUI_SCREEN_FULL) {
        /* pi's fullscreen: the alternate screen, cleared, no cursor row to ask for. The alternate screen
         * first: kitty's flags are a stack per screen, and park_pi pops them there before ?1049l (pi
         * itself writes ?1049h before its CSI >7u). Pushed on the main screen they outlived the program:
         * a Ctrl key at the shell's prompt came as CSI 117;5u, text in the console's line editor */
        static const char start[] = "\033[?1049h\033[?2004h\033[>1u\033[>4;1m\033[?1004h\033[H\033[2J";
        t->io->write(t->io->u, start, (long)sizeof(start) - 1);
    } else {
        static const char start[] = "\033[?2004h\033[>1u\033[>4;1m\033[?1004h\033[6n";
        t->io->write(t->io->u, start, (long)sizeof(start) - 1);
    }
    t->tr = t->screen == TUI_SCREEN_FULL ? 1 : t->rows;
    t0 = t->io->ms ? t->io->ms(t->io->u) : 0;
    for (; t->screen != TUI_SCREEN_FULL;) {
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
    t->B = t->fb = t->rows;
    t->started = 1;
    t->full = 1;
    put(t, "\033[22;0t");       /* the window's title kept, put back at the end */
    tui_title(t, "Claude");
    tui_frame(t);
    return 0;
}

/* pi's way out: what it leaves on the screen is the footer (REGULAR), or the alternate screen is left and the
 * transcript and the footer written on the main screen (FULL, fullscreenExitOutput "transcript"), then the line
 * that tells how to come back */
static void park_pi(cl_tui *t)
{
    int i;
    put(t, "\033[r" SGR0 "\033[?25h");
    put(t, modes_off);
    if (t->screen == TUI_SCREEN_FULL) {
        long a = 0, k;
        put(t, "\033[?1049l");
        if (!t->exit_hint_only) {
            for (k = 0; k < t->log.n; k++)
                if (t->log.p[k] == '\n') {
                    jw_raw(&t->o, t->log.p + a, k - a);
                    put(t, SGR0 "\r\n");
                    a = k + 1;
                }
            for (i = 0; i < t->ndrawn; i++) {
                put(t, t->drawn[i] ? t->drawn[i] : "");
                put(t, SGR0 "\r\n");
            }
            put(t, "\r\n");
        }
    } else {
        cup(t, t->fb + (t->ndrawn > 0 ? t->ndrawn : 1), 1);
        put(t, "\r\n\r\n");
    }
    if (t->exit_hint[0]) {
        put(t, t->exit_hint);
        put(t, "\r\n");
    } else if (t->screen == TUI_SCREEN_REGULAR) {
        put(t, "\033[A");
    }
}

static void park(cl_tui *t)
{
    if (t->screen != TUI_SCREEN_INLINE) {
        park_pi(t);
        return;
    }
    put(t, "\033[r");
    cup(t, t->tr <= t->B ? t->tr : t->B + 1 <= t->rows ? t->B + 1 : t->rows, 1);
    if (t->tr > t->B && t->B >= t->rows)
        put(t, "\r\n");
    put(t, SGR0 "\033[J\033[?25h");
    put(t, modes_off);
}

void tui_stop(cl_tui *t)
{
    if (!t->started)
        return;
    park(t);
    put(t, "\033[23;0t");
    flush(t);
    t->started = 0;
    if (t->io->raw)
        t->io->raw(t->io->u, 0);
}

/* ---- keys ---- */

static unsigned long now(cl_tui *t)
{
    return t->io->ms ? t->io->ms(t->io->u) : 0;
}

static void cycle_mode(cl_tui *t)
{
    if (t->perm)
        t->perm->mode = perm_next(t->perm);
}

/* a key read: a focus report is kept here, not handed on (0: no key) */
static int got_key(cl_tui *t, cl_key *k)
{
    if (k->k == K_FOCUS) {
        t->focus = k->row;
        return 0;
    }
    t->key_ms = now(t);
    return 1;
}

static int next_key(cl_tui *t, cl_key *k, long wait)
{
    char b[256];
    long n;
    if (keys_next(&t->keys, k, 0))
        return got_key(t, k);
    n = t->io->read(t->io->u, b, sizeof(b), wait);
    if (n < 0)
        return -1;
    if (n > 0)
        keys_feed(&t->keys, b, n);
    /* a sequence's bytes come in one burst: what is there now is all */
    return keys_next(&t->keys, k, 1) ? got_key(t, k) : 0;
}

static void tick(cl_tui *t)
{
    unsigned long n = now(t);
    if (t->busy && n - t->tick >= 200) {
        t->tick = n;
        t->frame++;
    }
}

/* the slash menu's chosen command written into the box in place of the text before the cursor (pi's applyCompletion:
 * what follows the cursor stays); 1 when it took */
static int complete(cl_tui *t)
{
    int *idx = 0, n;
    long from, cur;
    const char *name;
    n = slash_matches_at(t, &idx, SLASH_MAX(t), &from);
    if (!n)
        return 0;
    name = t->cmds[idx[t->msel < n ? t->msel : 0]].name;
    cur = t->ed.cur;
    if (!t->slash_filter || (from == 0 && cur == t->ed.n)) {
        ed_set(&t->ed, name);
        return 1;
    }
    ed_cut(&t->ed, from, cur, 0);
    ed_insert(&t->ed, name, (long)strlen(name));
    return 1;
}

static char *dupn(const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (d) {
        memcpy(d, s, (size_t)n);
        d[n] = 0;
    }
    return d;
}

/* the box's text as the program gets it: "!cmd", "#note" or the prompt */
static char *box_line(cl_tui *t)
{
    char *d = (char *)malloc((size_t)t->ed.n + 2);
    if (!d)
        return 0;
    if (t->box == BOX_PROMPT || t->ed.b[0] == (t->box == BOX_BASH ? '!' : '#')) {
        /* (a recalled "!ls" in bash mode is not doubled) */
        memcpy(d, t->ed.b, (size_t)t->ed.n + 1);
    } else {
        d[0] = t->box == BOX_BASH ? '!' : '#';
        memcpy(d + 1, t->ed.b, (size_t)t->ed.n + 1);
    }
    return d;
}

static void box_reset(cl_tui *t)
{
    ed_clear(&t->ed);
    t->box = BOX_PROMPT;
    t->msel = 0;
    t->mclosed = 0;
    t->copen = 0;
    t->scroll = 0;
}

void tui_set_text(cl_tui *t, const char *s)
{
    box_reset(t);
    ed_set(&t->ed, s);
}

/* a submitted line goes into the history, in memory and on disk */
static void remember(cl_tui *t, const char *line)
{
    ed_remember(&t->ed, line);
    hist_add(&t->hist, t->sys, t->sys ? t->histfile : 0, line, t->project ? t->project : "");
}

/* ---- Ctrl+R ---- */

static void search_start(cl_tui *t)
{
    free(t->saved);
    t->saved = dupn(t->ed.b, t->ed.n);
    t->search = 1;
    t->sfail = 0;
    t->sq[0] = 0;
    t->sidx = t->hist.n;
}

static void search_find(cl_tui *t, int older)
{
    int i = hist_find(&t->hist, t->sq, older ? t->sidx : t->hist.n);
    if (i >= 0) {
        t->sidx = i;
        ed_set(&t->ed, t->hist.d[i]);
        t->sfail = 0;
    } else {
        t->sfail = 1;
    }
}

static void search_end(cl_tui *t, int keep)
{
    if (!keep && t->saved)
        ed_set(&t->ed, t->saved);
    free(t->saved);
    t->saved = 0;
    t->search = 0;
    t->sfail = 0;
    t->sq[0] = 0;
}

/* ---- @-completion ---- */

/* The path token that ends at the cursor: 1 with *start at its start and
 * *skip 1 when it is an "@path" of a prompt, 0 when it is a bash-mode
 * token holding a '/' (Claude Code's file list in shell mode). */
static int path_token(cl_tui *t, long *start, int *skip)
{
    static const char *const dir_cmds[] = { "/add-dir ", "/cd " };
    long s = t->ed.cur;
    int i;
    t->comp_dirs = 0;
    /* /add-dir and /cd (A4 gaps 3): the argument is a directory path,
     * spaces and all */
    for (i = 0; t->box == BOX_PROMPT && i < 2; i++) {
        long l = (long)strlen(dir_cmds[i]);
        if (t->ed.cur >= l && !strncmp(t->ed.b, dir_cmds[i], (size_t)l) &&
            !memchr(t->ed.b, '\n', (size_t)t->ed.n)) {
            while (l < t->ed.cur && t->ed.b[l] == ' ')
                l++;
            *start = l;
            *skip = 0;
            t->comp_dirs = 1;
            return 1;
        }
    }
    while (s > 0 && t->ed.b[s - 1] != ' ' && t->ed.b[s - 1] != '\n' && t->ed.b[s - 1] != '\t')
        s--;
    *start = s;
    if (s >= t->ed.cur)
        return 0;
    if (t->box == BOX_PROMPT && t->ed.b[s] == '@') {
        *skip = 1;
        return 1;
    }
    *skip = 0;
    return t->box == BOX_BASH && memchr(t->ed.b + s, '/', (size_t)(t->ed.cur - s)) != 0;
}

static void comp_apply(cl_tui *t, const char *rep, int final)
{
    long l = (long)strlen(rep);
    ed_cut(&t->ed, t->ctok + t->cskip, t->ed.cur, 0);
    ed_insert(&t->ed, rep, l);
    if (final && l && rep[l - 1] != '/' && rep[l - 1] != ':')
        ed_insert(&t->ed, " ", 1);
}

/* the paths the token at the cursor may become: their count (0 none),
 * -1 when there is no token; the list's state set for it */
static int comp_query(cl_tui *t, long *plen)
{
    long s, pl;
    int skip, n;
    char tok[128];
    if (!t->complete || !path_token(t, &s, &skip))
        return -1;
    pl = t->ed.cur - s - skip;
    if (!plen && t->comp_dirs && pl <= 0)
        return -1;                  /* a directory list opens once a path is begun (Tab: at once) */
    if (pl >= (long)sizeof(tok))
        return -1;
    memcpy(tok, t->ed.b + s + skip, (size_t)pl);
    tok[pl] = 0;
    t->ctok = s;
    t->cskip = skip;                /* (the completion looks: an @ token offers agents too) */
    n = t->complete(t->cu, tok, t->comp, TUI_COMP);
    t->ncomp = n > 0 ? n : 0;
    if (t->csel >= t->ncomp)
        t->csel = 0;
    if (plen)
        *plen = pl;
    return n > 0 ? n : 0;
}

/* After a key changed the text or moved the cursor: the list follows the
 * token at the cursor, narrowed to what is typed (each directory read
 * once a prompt: input.c's cache in t->dirs) */
static void comp_live(cl_tui *t)
{
    int n;
    if (t->modal || t->search || t->cclosed) {
        t->copen = 0;
        return;
    }
    n = comp_query(t, 0);
    t->copen = n > 0;
}

/* Tab in bash mode on a command without a path: the newest earlier !
 * command of this project that starts with what is typed */
static int bash_hist_tab(cl_tui *t)
{
    int i;
    for (i = t->hist.n - 1; i >= 0; i--) {
        const char *d = t->hist.d[i];
        if (d[0] != '!' || strncmp(d + 1, t->ed.b, (size_t)t->ed.n) || !d[1 + t->ed.n] ||
            strcmp(t->hist.p[i] ? t->hist.p[i] : "", t->project ? t->project : ""))
            continue;
        ed_set(&t->ed, d + 1);
        return 1;
    }
    cl_copy(t->hint, "No earlier command starts so", sizeof(t->hint));
    return 1;
}

/* Tab with the list closed: one match taken, several: their common start
 * and the list */
static int comp_tab(cl_tui *t)
{
    long pl;
    int n, i;
    n = comp_query(t, &pl);
    if (n < 0)
        return 0;
    t->copen = 0;
    if (n <= 0) {
        cl_copy(t->hint, "No matching files", sizeof(t->hint));
        return 1;
    }
    if (n == 1) {
        comp_apply(t, t->comp[0], 1);
        return 1;
    }
    /* several: their common start, then the list */
    {
        long c = (long)strlen(t->comp[0]);
        for (i = 1; i < n; i++) {
            long k = 0;
            while (k < c && t->comp[i][k] == t->comp[0][k])
                k++;
            c = k;
        }
        if (c > pl) {
            char cp[128];
            memcpy(cp, t->comp[0], (size_t)c);
            cp[c] = 0;
            comp_apply(t, cp, 0);
        }
    }
    t->ncomp = n;
    t->csel = 0;
    t->copen = 1;
    t->cclosed = 0;
    return 1;
}

/* the @ list's directory cache (input.c reads, these keep) */
tui_dir *tui_dir_get(cl_tui *t, const char *full)
{
    int i;
    for (i = 0; i < TUI_DIRS; i++)
        if (t->dirs[i].path[0] && t->dirs[i].epoch == t->epoch && !strcmp(t->dirs[i].path, full))
            return &t->dirs[i];
    return 0;
}

tui_dir *tui_dir_put(cl_tui *t, const char *full)
{
    tui_dir *d = 0;
    int i;
    for (i = 0; i < TUI_DIRS && !d; i++)
        if (!strcmp(t->dirs[i].path, full))
            d = &t->dirs[i];        /* read for an earlier prompt: again */
    if (!d) {
        d = &t->dirs[t->dir_next];
        t->dir_next = (t->dir_next + 1) % TUI_DIRS;
    }
    cl_copy(d->path, full, sizeof(d->path));
    jw_reset(&d->names);
    d->n = 0;
    d->epoch = t->epoch;
    t->n_lists++;
    return d;
}

/* ---- the queue ---- */

static int is_command(const char *s)
{
    return s[0] == '/' || s[0] == '!' || s[0] == '#';
}

static void enqueue(cl_tui *t, char *line)
{
    if (t->nq == TUI_QUEUE) {
        /* full: the last entry takes this one too */
        char *j = (char *)malloc(strlen(t->queue[t->nq - 1]) + strlen(line) + 2);
        if (j) {
            strcpy(j, t->queue[t->nq - 1]);
            strcat(j, "\n");
            strcat(j, line);
            free(t->queue[t->nq - 1]);
            t->queue[t->nq - 1] = j;
        }
        free(line);
        return;
    }
    t->queue[t->nq++] = line;
}

static void unqueue(cl_tui *t, int n)
{
    int i;
    for (i = 0; i < n; i++)
        free(t->queue[i]);
    memmove(t->queue, t->queue + n, sizeof(t->queue[0]) * (size_t)(t->nq - n));
    t->nq -= n;
}

char *tui_dequeue(cl_tui *t, int plain_only)
{
    jw w;
    int n = 0;
    char *d;
    if (!t->nq)
        return 0;
    if (is_command(t->queue[0])) {
        if (plain_only)
            return 0;
        d = t->queue[0];
        t->queue[0] = 0;
        unqueue(t, 1);
        return d;
    }
    jw_init(&w);
    while (n < t->nq && !is_command(t->queue[n])) {
        if (n)
            jw_raw(&w, "\n", 1);
        jw_rawz(&w, t->queue[n]);
        n++;
    }
    d = w.oom ? 0 : dupn(w.p, w.n);
    jw_free(&w);
    unqueue(t, n);
    return d;
}

/* Up on the first row: what was queued comes back into the box, one entry
 * a line, ahead of what is typed */
static void take_back(cl_tui *t)
{
    jw w;
    int i;
    jw_init(&w);
    for (i = 0; i < t->nq; i++) {
        if (i)
            jw_raw(&w, "\n", 1);
        jw_rawz(&w, t->queue[i]);
    }
    if (t->ed.n) {
        jw_raw(&w, "\n", 1);
        jw_raw(&w, t->ed.b, t->ed.n);
    }
    unqueue(t, t->nq);
    if (!w.oom) {
        box_reset(t);
        ed_set(&t->ed, w.p);
    }
    jw_free(&w);
}

/* ---- Ctrl+G: the external editor ---- */

static void external_edit(cl_tui *t)
{
    char *b = 0;
    long n = 0;
    int rc;
    if (!t->io->edit || !t->sys) {
        cl_copy(t->hint, "No external editor here", sizeof(t->hint));
        return;
    }
    if (t->sys->write(t->sys->u, t->edit_path, t->ed.b, t->ed.n)) {
        cl_copy(t->hint, "Cannot write the prompt's file", sizeof(t->hint));
        return;
    }
    /* the window as it was before Claude: the editor may be a console one */
    park(t);
    flush(t);
    if (t->io->raw)
        t->io->raw(t->io->u, 0);
    rc = t->io->edit(t->io->u, t->edit_path);
    if (t->io->raw)
        t->io->raw(t->io->u, 1);
    /* pi: ui.stop() leaves the alternate screen, ui.start() enters it again (tui-alt-screen.ts start: ?1049h,
     * then its modes, cleared) -- the modes after the screen, as in tui_start */
    if (t->screen == TUI_SCREEN_FULL)
        put(t, "\033[?1049h");
    put(t, modes_on);
    if (t->screen == TUI_SCREEN_FULL)
        put(t, "\033[H\033[2J");
    t->title[0] = 0;
    tui_title(t, t->busy ? "Claude - working" : "Claude");
    tui_redraw(t);
    if (rc) {
        cl_copy(t->hint, "The editor did not run", sizeof(t->hint));
        return;
    }
    if (t->sys->read(t->sys->u, t->edit_path, 1024L * 1024, &b, &n) == 0) {
        while (n > 0 && (b[n - 1] == '\n' || b[n - 1] == '\r'))
            n--;
        b[n] = 0;
        ed_set(&t->ed, b);
    }
    free(b);
}

/* ---- one key, the box's or the screen's ---- */

enum { H_GO, H_SUBMIT, H_QUIT, H_STOP };

/* the editor's key; a change of text reopens the slash menu */
static void edit(cl_tui *t, cl_key *k)
{
    long n0 = t->ed.n, c0 = t->ed.cur;
    t->ed.now_ms = now(t);          /* vim's remapped sequences are timed */
    ed_key(&t->ed, k);
    if (t->ed.n != n0) {
        t->suggest[0] = 0;          /* typing drops the suggestion */
        t->mclosed = 0;
        t->msel = 0;
        t->cclosed = 0;
    }
    if (t->slash_filter && (t->ed.n != n0 || t->ed.cur != c0)) {
        /* pi's menu is open or closed (editor.ts updateAutocomplete, cancelAutocomplete): typing opens it, a change that leaves
         * nothing to list closes it, and a cursor move alone does not open a closed one */
        int *idx = 0;
        if (!t->mclosed && !slash_matches(t, &idx, SLASH_MAX(t)))
            t->mclosed = 1;
    }
    if (t->ed.n != n0 || t->ed.cur != c0)
        comp_live(t);
}

/* the box's text taken as a submitted line (the history has it, the box
 * is empty again, the @ list's directories will be read anew): 0 when
 * there is nothing */
static char *take_box(cl_tui *t)
{
    char *line;
    complete(t);
    if (!t->ed.n)
        return 0;
    line = box_line(t);
    if (!line)
        return 0;
    remember(t, line);
    box_reset(t);
    t->hint[0] = 0;
    t->epoch++;
    return line;
}

/* Ctrl+Enter / Ctrl+X Ctrl+S while Claude works: what is queued goes now,
 * the draft queued behind it. A ! queued ahead of the messages stops the
 * turn; a command running in the foreground moves to the background (the
 * messages go into the same turn); anything else stops the turn and the
 * queue is sent next. In bash mode the key only queues. */
static int send_now(cl_tui *t)
{
    char *line = take_box(t);
    int i, bash = line && line[0] == '!';
    if (line)
        enqueue(t, line);
    if (bash || !t->nq)
        return H_GO;
    for (i = 0; i < t->nq && is_command(t->queue[i]); i++)
        if (t->queue[i][0] == '!')
            return H_STOP;
    if (t->bgable) {
        t->bg_req = 1;
        return H_GO;
    }
    return H_STOP;
}

/* Ctrl+S: the text put aside (with its cursor and mode), or back */
static void stash(cl_tui *t)
{
    if (t->ed.n) {
        free(t->stash);
        t->stash = dupn(t->ed.b, t->ed.n);
        t->stash_cur = t->ed.cur;
        t->stash_box = t->box;
        box_reset(t);
        cl_copy(t->hint, "Prompt stashed (ctrl+s on an empty prompt brings it back)", sizeof(t->hint));
        return;
    }
    if (!t->stash)
        return;
    box_reset(t);
    ed_set(&t->ed, t->stash);
    t->ed.cur = t->stash_cur <= t->ed.n ? t->stash_cur : t->ed.n;
    t->box = t->stash_box;
    free(t->stash);
    t->stash = 0;
}

/* Alt+T: extended thinking on or off for the session (from the next turn;
 * the models that always think say so) */
static void think_toggle(cl_tui *t)
{
    int caps = conv_caps(t->model ? t->model : "");
    if (!t->think_off)
        return;
    if (caps & CAP_THINK_ALWAYS)
        cl_copy(t->hint, "Thinking can't be turned off for this model", sizeof(t->hint));
    else if (!(caps & CAP_ADAPTIVE))
        cl_copy(t->hint, "This model runs without extended thinking here", sizeof(t->hint));
    else {
        *t->think_off = !*t->think_off;
        cl_copy(t->hint, *t->think_off ? "Thinking off" : "Thinking on", sizeof(t->hint));
    }
}

/* a key made up for the editor (an action's own key may be another) */
static void edit_as(cl_tui *t, int kind, unsigned long ch, int mods)
{
    cl_key s;
    memset(&s, 0, sizeof(s));
    s.k = kind;
    s.ch = ch;
    s.mods = mods;
    edit(t, &s);
}

/* the box's text submitted (Enter's chat:submit; queue: chat:queueSubmit,
 * which never takes the \ + Enter newline) */
/* TUI_LOOK_PI: the program's queued messages put in front of the box's text, a blank line between (pi's
 * restoreQueuedMessagesToEditor); how many came back */
static int restore_queue(cl_tui *t, int quiet)
{
    int n = 0;
    char *q = t->on_dequeue ? t->on_dequeue(t->qu, &n, quiet) : 0;
    if (q) {
        jw w;
        char *cur = box_line(t);
        jw_init(&w);
        jw_rawz(&w, q);
        if (cur && cur[0]) {
            jw_rawz(&w, "\n\n");
            jw_rawz(&w, cur);
        }
        jw_rawz(&w, "");
        tui_set_text(t, w.p);
        jw_free(&w);
        free(cur);
        free(q);
    }
    return n;
}

/* a line typed while the agent works, in pi's look: steering (Enter) or a follow-up (Alt+Enter) for the
 * program; a command or a ! line waits in the type-ahead queue. 1 when taken */
static int pi_queue_line(cl_tui *t, char *line, int follow)
{
    if (t->look != TUI_LOOK_PI || !t->on_queue || line[0] == '/' || line[0] == '!')
        return 0;
    if (!t->on_queue(t->qu, line, follow))
        return 0;
    free(line);
    return 1;
}

static int submit(cl_tui *t, int busy, char *buf, long cap, int queue)
{
    char *line;
    if (!queue && (t->ed.vim == VIM_OFF || t->ed.vim == VIM_INSERT) && t->ed.cur > 0 &&
        t->ed.b[t->ed.cur - 1] == '\\') {
        edit_as(t, K_BS, 0, 0);
        ed_insert(&t->ed, "\n", 1);
        return H_GO;
    }
    line = take_box(t);
    if (!line)
        return H_GO;
    if (busy && pi_queue_line(t, line, 0))
        return H_GO;
    if (busy || !buf) {
        enqueue(t, line);
        return H_GO;
    }
    cl_copy(buf, line, cap);
    free(line);
    return H_SUBMIT;
}

/* chat:cancel (Esc): vim's own first, then the slash menu, the box's
 * mode, a turn, a pending /loop wakeup on the empty box, and Esc Esc
 * (clear the draft, or the rewind menu) */
static int cancel(cl_tui *t, int busy)
{
    int *idx = 0;
    if (t->ed.vim == VIM_INSERT || t->ed.vim == VIM_VISUAL || t->ed.vim == VIM_VLINE ||
        (t->ed.vim == VIM_NORMAL && !vim_idle(&t->ed))) {
        edit_as(t, K_ESC, 0, 0);
        return H_GO;
    }
    if (slash_matches(t, &idx, SLASH_MAX(t))) {
        t->mclosed = 1;
        return H_GO;
    }
    if (t->box != BOX_PROMPT && !t->ed.n) {
        t->box = BOX_PROMPT;
        return H_GO;
    }
    if (busy) {
        if (t->look == TUI_LOOK_PI)
            restore_queue(t, 1);    /* pi: Esc takes the queued messages back to the editor, then aborts */
        return H_STOP;
    }
    if (!t->ed.n && t->esc_idle && t->esc_idle(t->iu))
        return H_GO;                /* A4 gaps 3: a pending /loop wakeup cancelled */
    if (t->esc_armed && now(t) - t->esc_ms <= 1000) {
        t->esc_armed = 0;
        t->hint[0] = 0;
        if (t->ed.n) {
            /* the draft goes, into the history: Up brings it back */
            char *line = box_line(t);
            if (line)
                ed_remember(&t->ed, line);
            free(line);
            box_reset(t);
        } else if (t->on_rewind) {
            t->on_rewind(t->ru);
        }
        return H_GO;
    }
    t->esc_armed = 1;
    t->esc_ms = now(t);
    if (t->ed.n)
        cl_copy(t->hint, "Esc again to clear", sizeof(t->hint));
    return H_GO;
}

/* history:previous / history:next (Up, Down): the slash menu's selection,
 * the queue taken back, else the editor (a row up, then the history) */
static int history_step(cl_tui *t, int up)
{
    int *idx = 0, n = slash_matches(t, &idx, SLASH_MAX(t));
    if (n) {
        t->msel = (t->msel + (up ? n - 1 : 1)) % n;
        return H_GO;
    }
    if (up && t->nq && ed_lstart(&t->ed, t->ed.cur) == 0) {
        take_back(t);
        return H_GO;
    }
    edit_as(t, up ? K_UP : K_DOWN, 0, 0);
    return H_GO;
}

/* an action of the Chat, Task, Help or Global context */
static int act_chat(cl_tui *t, int act, cl_key *k, int busy, char *buf, long cap)
{
    switch (act) {
    case KA_INTERRUPT:
        if (busy)
            return H_STOP;
        if (t->ed.n || t->box != BOX_PROMPT) {
            box_reset(t);
            return H_GO;
        }
        if (t->esc_idle && t->esc_idle(t->iu))
            return H_GO;            /* A4 gaps 3: a pending /loop wakeup cancelled */
        if (t->quit_armed == 'c')
            return H_QUIT;
        t->quit_armed = 'c';
        cl_copy(t->hint, "Press Ctrl+C again to exit", sizeof(t->hint));
        return H_GO;
    case KA_EXIT:
        if (t->ed.n) {
            edit(t, k);             /* text in the box: the character after the cursor goes */
            return H_GO;
        }
        if (t->look == TUI_LOOK_PI) {
            /* pi's app.exit on an empty editor: shutdown at once, the run stopped first when one is going */
            if (!busy)
                return H_QUIT;
            t->quit_after = 1;
            return H_STOP;
        }
        if (busy)
            return H_GO;
        if (t->quit_armed == 'd' && now(t) - t->quit_ms <= 800)
            return H_QUIT;
        t->quit_armed = 'd';
        t->quit_ms = now(t);
        cl_copy(t->hint, "Press Ctrl+D again to exit", sizeof(t->hint));
        return H_GO;
    case KA_REDRAW:
    case KA_CLEAR_INPUT:
    case KA_CLEAR_SCREEN:
        tui_redraw(t);
        return H_GO;
    case KA_TODOS:
        t->show_todos = !t->show_todos;
        return H_GO;
    case KA_TRANSCRIPT:
        tui_transcript(t);
        return H_GO;
    case KA_HIST_SEARCH:
        search_start(t);
        return H_GO;
    case KA_HIST_PREV:
    case KA_HIST_NEXT:
        return history_step(t, act == KA_HIST_PREV);
    case KA_CANCEL:
        return cancel(t, busy);
    case KA_KILL_AGENTS:
        cl_copy(t->hint, "No background agents run here: subagents run in the foreground", sizeof(t->hint));
        return H_GO;
    case KA_CYCLE_MODE:
        cycle_mode(t);
        return H_GO;
    case KA_MODEL_PICKER:
        /* the model picker, the draft left in the box */
        if (busy || !buf) {
            char *m = dupn("/model", 6);
            if (m)
                enqueue(t, m);
            return H_GO;
        }
        cl_copy(buf, "/model", cap);
        t->keycmd = 1;
        return H_SUBMIT;
    case KA_FAST_MODE:
        cl_copy(t->hint, "Fast mode is a claude.ai plan feature: not here", sizeof(t->hint));
        return H_GO;
    case KA_THINKING:
        think_toggle(t);
        return H_GO;
    case KA_SUBMIT:
    case KA_QUEUE_SUBMIT:
        return submit(t, busy, buf, cap, act == KA_QUEUE_SUBMIT);
    case KA_SEND_NOW:
        return busy ? send_now(t) : submit(t, busy, buf, cap, 1);
    case KA_FOLLOW_UP:
        if (busy) {
            char *line = take_box(t);
            if (line && !pi_queue_line(t, line, 1))
                enqueue(t, line);
            return H_GO;
        }
        return submit(t, busy, buf, cap, 0);        /* idle: it is a plain Enter */
    case KA_DEQUEUE:
        restore_queue(t, 0);
        return H_GO;
    case KA_MODEL_NEXT:
    case KA_MODEL_PREV:
        if (t->on_cycle)
            t->on_cycle(t->qu, 0, act == KA_MODEL_NEXT ? 1 : -1);
        return H_GO;
    case KA_THINK_CYCLE:
        if (t->on_cycle)
            t->on_cycle(t->qu, 1, 1);
        return H_GO;
    case KA_TOOLS_EXPAND:
        if (t->on_expand)
            t->on_expand(t->qu);
        return H_GO;
    case KA_CMD_NEW:
    case KA_CMD_TREE:
    case KA_CMD_FORK:
    case KA_CMD_RESUME: {
        const char *c = act == KA_CMD_NEW ? "/new" : act == KA_CMD_TREE ? "/tree" : act == KA_CMD_FORK ? "/fork" : "/resume";
        if (busy || !buf) {
            char *m = dupn(c, (long)strlen(c));
            if (m)
                enqueue(t, m);
            return H_GO;
        }
        cl_copy(buf, c, cap);
        t->keycmd = 1;
        return H_SUBMIT;
    }
    case KA_NEWLINE:
        edit_as(t, K_NEWLINE, 0, 0);
        return H_GO;
    case KA_UNDO:
        edit_as(t, K_CTRL, '_', KM_CTRL);
        return H_GO;
    case KA_EXT_EDITOR:
        external_edit(t);
        return H_GO;
    case KA_STASH:
        stash(t);
        return H_GO;
    case KA_IMAGE_PASTE:
        cl_copy(t->hint, "No image paste on the Amiga: mention the file with @ instead", sizeof(t->hint));
        return H_GO;
    case KA_TASK_BG:
        if (t->bgable)
            t->bg_req = 1;
        else
            cl_copy(t->hint, "Nothing to move to the background now", sizeof(t->hint));
        return H_GO;
    default:
        return H_GO;                /* help:dismiss (the panel is gone), the inert ones */
    }
}

/* Ctrl+R's search: its actions, and typing the query */
static int search_key(cl_tui *t, int act, cl_key *k)
{
    switch (act) {
    case KA_HS_NEXT:
        search_find(t, 1);
        return 1;
    case KA_HS_CANCEL:
        search_end(t, 0);
        return 1;
    case KA_HS_ACCEPT:
        search_end(t, 1);
        return 1;
    case KA_HS_SCOPE:
        return 1;                   /* fullscreen rendering's scopes: one history here */
    default:
        break;
    }
    if (k->k == K_CHAR && !act) {
        char u[8];
        long l = (long)strlen(t->sq);
        int ul = vw_put_utf8(u, k->ch);
        if (l + ul < (long)sizeof(t->sq)) {
            memcpy(t->sq + l, u, (size_t)ul);
            t->sq[l + ul] = 0;
            search_find(t, 0);
        }
        return 1;
    }
    if (k->k == K_BS && !act) {
        long l = (long)strlen(t->sq);
        if (!l) {
            search_end(t, 0);
            return 1;
        }
        do
            l--;
        while (l > 0 && ((unsigned char)t->sq[l] & 0xc0) == 0x80);
        t->sq[l] = 0;
        search_find(t, 0);
        return 1;
    }
    search_end(t, 1);               /* Enter sends the match; other keys edit it */
    return 0;
}

/* the contexts active in the box now, the most specific first */
static int box_contexts(cl_tui *t, int *ctx, int help, int busy)
{
    int n = 0;
    if (t->search) {
        ctx[n++] = KC_HSEARCH;
        ctx[n++] = KC_GLOBAL;
        return n;
    }
    if (t->copen)
        ctx[n++] = KC_AUTOCOMPLETE;
    if (help)
        ctx[n++] = KC_HELP;
    if (busy)
        ctx[n++] = KC_TASK;
    ctx[n++] = KC_CHAT;
    ctx[n++] = KC_GLOBAL;
    return n;
}

/* vim's NORMAL and VISUAL modes take the plain characters themselves
 * (Claude Code: vim keys are not remappable) */
static int vim_owns(cl_tui *t, cl_key *k)
{
    return (t->ed.vim == VIM_NORMAL || t->ed.vim == VIM_VISUAL || t->ed.vim == VIM_VLINE) && k->k == K_CHAR;
}

static int handle(cl_tui *t, cl_key *k, int busy, char *buf, long cap)
{
    int ctx[6], nc, act, help = t->show_help;
    if (k->k == K_CPR)
        return H_GO;
    if (k->k != K_ESC)
        t->esc_armed = 0;
    if (t->quit_armed && !(k->k == K_CTRL && (k->ch == 'c' || k->ch == 'd'))) {
        t->hint[0] = 0;
        t->quit_armed = 0;
    }
    if (t->show_help) {
        /* the panel goes with the next key; ? only closes it */
        t->show_help = 0;
        if (k->k == K_CHAR && k->ch == '?')
            return H_GO;
    }
    nc = box_contexts(t, ctx, help, busy);
    act = vim_owns(t, k) && !t->search ? KA_NONE : km_action(&t->km, ctx, nc, k, now(t));
    if (t->km.expired) {
        t->km.expired = 0;
        cl_copy(t->hint, "Chord cancelled: the second key came after 3 seconds", sizeof(t->hint));
    }
    if (act == KA_PENDING)
        return H_GO;
    if (act == KA_CHORD_MISS) {
        cl_copy(t->hint, "No shortcut for that key sequence", sizeof(t->hint));
        return H_GO;
    }
    if (t->search) {
        if (search_key(t, act, k))
            return H_GO;
        if (act == KA_HS_EXECUTE)
            act = KA_SUBMIT;        /* the match sent */
        else {
            /* the search ended with the match in the box: the key goes on there */
            nc = box_contexts(t, ctx, 0, busy);
            act = km_action(&t->km, ctx, nc, k, now(t));
            if (act == KA_PENDING || act == KA_CHORD_MISS)
                return H_GO;
        }
    }
    if (t->copen) {
        switch (act) {
        case KA_AC_PREV:
            t->csel = (t->csel + t->ncomp - 1) % t->ncomp;
            return H_GO;
        case KA_AC_NEXT:
            t->csel = (t->csel + 1) % t->ncomp;
            return H_GO;
        case KA_AC_ACCEPT:
            comp_apply(t, t->comp[t->csel], 1);
            t->copen = 0;
            return H_GO;
        case KA_AC_DISMISS:
            t->copen = 0;
            t->cclosed = 1;
            return H_GO;
        default:
            break;
        }
        if (act == KA_SUBMIT) {
            /* Enter: the row taken, unless it is what is typed: then it sends */
            long have = t->ed.cur - t->ctok - t->cskip;
            if ((long)strlen(t->comp[t->csel]) != have ||
                strncmp(t->comp[t->csel], t->ed.b + t->ctok + t->cskip, (size_t)have)) {
                comp_apply(t, t->comp[t->csel], 1);
                t->copen = 0;
                return H_GO;
            }
            t->copen = 0;
        }
        /* typing narrows it (edit -> comp_live) */
    }
    if (act >= KA_VIEW_PGUP && act <= KA_VIEW_BOTTOM) {
        /* pi's tui.altScreen.*: the viewport of the fullscreen transcript; elsewhere the key is the editor's */
        if (t->screen == TUI_SCREEN_FULL) {
            tui_scroll(t, act);
            return H_GO;
        }
        act = KA_NONE;
    }
    if (act > KA_NONE && act < KA_COUNT)
        return act_chat(t, act, k, busy, buf, cap);
    /* no binding: the box's own keys, then the editor's */
    switch (k->k) {
    case K_RIGHT:
    case K_TAB:
        if (!t->ed.n && t->suggest[0] && t->box == BOX_PROMPT) {
            ed_set(&t->ed, t->suggest);     /* the suggestion taken: Enter sends it */
            t->suggest[0] = 0;
            return H_GO;
        }
        if (k->k == K_RIGHT) {
            edit(t, k);
            return H_GO;
        }
        if (complete(t))
            ed_insert(&t->ed, " ", 1);
        else if (!comp_tab(t) && t->box == BOX_BASH && t->ed.n)
            bash_hist_tab(t);
        return H_GO;
    case K_BS:
        if (!t->ed.n && t->box != BOX_PROMPT) {
            t->box = BOX_PROMPT;
            return H_GO;
        }
        edit(t, k);
        return H_GO;
    case K_CHAR:
        if (!t->ed.n && t->box == BOX_PROMPT && (t->ed.vim == VIM_OFF || t->ed.vim == VIM_INSERT) &&
            (k->ch == '!' || k->ch == '#') && t->look != TUI_LOOK_PI) {
            t->box = k->ch == '!' ? BOX_BASH : BOX_MEMORY;
            return H_GO;
        }
        if (!t->ed.n && t->box == BOX_PROMPT && t->ed.vim != VIM_NORMAL && k->ch == '?') {
            t->show_help = 1;       /* the shortcuts panel (? again or any key closes it) */
            return H_GO;
        }
        if (t->ed.vim == VIM_NORMAL && vim_idle(&t->ed) && k->ch == '/') {
            search_start(t);
            return H_GO;
        }
        edit(t, k);
        return H_GO;
    case K_PASTE:
        if (!t->ed.n && t->box == BOX_PROMPT && k->n > 0 && k->text[0] == '!') {
            cl_key rest = *k;
            t->box = BOX_BASH;
            rest.text++;
            rest.n--;
            edit(t, &rest);
            return H_GO;
        }
        edit(t, k);
        return H_GO;
    case K_CTRL:
        if (k->ch == 'u' && !t->ed.n && t->box != BOX_PROMPT) {
            t->box = BOX_PROMPT;
            return H_GO;
        }
        edit(t, k);
        return H_GO;
    default:
        edit(t, k);
        return H_GO;
    }
}

long tui_read(cl_tui *t, char *buf, long cap)
{
    char *q;
    t->hint[0] = 0;
    t->keycmd = 0;
    if (t->quit_after) {
        t->quit_after = 0;
        return -1;              /* Ctrl+D while the run went (pi's look): the run has stopped, now the program */
    }
    q = tui_dequeue(t, 0);
    if (q) {
        cl_copy(buf, q, cap);
        free(q);
        tui_frame(t);
        return (long)strlen(buf);
    }
    for (;;) {
        cl_key k;
        int r, h;
        if (t->idle)
            t->idle(t->iu);         /* the status line's schedule */
        if (t->wake && !t->ed.n && (q = t->wake(t->iu)) != 0) { cl_copy(buf, q, cap); free(q); tui_frame(t); return (long)strlen(buf); }   /* A4 gaps 2: a scheduled turn */
        tui_frame(t);
        r = next_key(t, &k, 500);
        if (r < 0)
            return -1;
        if (!r) {
            check_size(t);
            continue;
        }
        h = handle(t, &k, 0, buf, cap);
        if (h == H_SUBMIT) {
            tui_frame(t);
            return (long)strlen(buf);
        }
        if (h == H_QUIT)
            return -1;
    }
}

int tui_poll(cl_tui *t)
{
    int stop = 0, r;
    cl_key k;
    while ((r = next_key(t, &k, 0)) > 0)
        if (handle(t, &k, 1, 0, 0) == H_STOP)
            stop = 1;
    if (r < 0)
        stop = 1;
    tui_tick(t);
    return stop;
}

int tui_wait(cl_tui *t, long ms)
{
    int res = TW_GO, r, any = 0;
    cl_key k;
    t->bgable = 1;
    t->bg_req = 0;
    /* the keys typed meanwhile (as tui_poll), then the rest of the time
     * asleep: the console keeps what is typed until the next look */
    while ((r = next_key(t, &k, 0)) > 0) {
        any = 1;
        if (handle(t, &k, 1, 0, 0) == H_STOP)
            res = TW_STOP;
    }
    if (r < 0)
        res = TW_STOP;
    else if (!any && res == TW_GO && !t->bg_req && t->io->sleep && t->io->sleep(t->io->u, ms))
        res = TW_STOP;              /* a Ctrl+C break signal while asleep */
    if (t->bg_req && res == TW_GO)
        res = TW_BACKGROUND;
    t->bgable = 0;
    t->bg_req = 0;
    tui_tick(t);
    return res;
}

void tui_tick(cl_tui *t)
{
    check_size(t);
    tick(t);
    tui_frame(t);
}

void tui_busy(cl_tui *t, int on)
{
    int was = t->busy;
    t->busy = on;
    if (on) {
        t->t0 = t->tick = now(t);
        t->busy_t0 = t->t0;
        t->tokens = 0;
        t->frame = 0;
        tui_title(t, "Claude - working");
    } else if (was) {
        tui_title(t, "Claude");
        if (t->notify_after_s >= 0 && now(t) - t->busy_t0 >= (unsigned long)t->notify_after_s * 1000UL)
            tui_notify(t, "Claude is waiting for your input");
    }
    tui_frame(t);
}

void tui_set_todos(cl_tui *t, const char *json, long n)
{
    jv v, arr, e, x;
    jit it;
    jw_reset(&t->todos);
    if (json_parse(json, n, &v) || !json_get(v, "todos", &arr))
        return;
    json_iter(arr, &it);
    while (json_next(&it, 0, &e)) {
        long l;
        char *c = json_get(e, "content", &x) ? json_strdup(x, &l) : 0;
        char *nl;
        if (!c)
            continue;
        for (nl = c; *nl; nl++)
            if (*nl == '\n' || *nl == '\r')
                *nl = ' ';
        if (json_get(e, "status", &x) && json_streq(x, "completed"))
            jw_raw(&t->todos, "c", 1);
        else if (json_get(e, "status", &x) && json_streq(x, "in_progress"))
            jw_raw(&t->todos, "p", 1);
        else
            jw_raw(&t->todos, "o", 1);
        jw_rawz(&t->todos, c);
        jw_raw(&t->todos, "\n", 1);
        free(c);
    }
}

int tui_menu(cl_tui *t, const char *title, const char *question, const char *const *opt, int n, int sel,
             int esc)
{
    int busy = t->busy, choice = -1;
    unsigned long idle0, start;
    t->modal = 1;
    t->m_title = title;
    t->m_q = question;
    t->m_opt = opt;
    t->m_n = n;
    t->m_sel = sel;
    t->busy = 0;
    t->m_noting = -1;
    t->m_note[0] = 0;
    t->m_left = -1;
    idle0 = start = now(t);
    for (;;) {
        cl_key k;
        int r;
        long wait = 500;
        if (t->m_timeout_ms >= 0) {
            /* an extension's dialog with a timeout (ExtensionUIDialogOptions.timeout): runs out whatever is typed */
            long left = t->m_timeout_ms - (long)(now(t) - start);
            long next;
            if (left <= 0) {
                choice = TUI_AWAY;
                break;
            }
            t->m_left = (int)((left + 999) / 1000);
            next = left % 1000 ? left % 1000 : 1000;      /* the next time the shown seconds change */
            if (next < wait)
                wait = next;
        } else if (t->m_afk_ms >= 0) {
            /* AskUserQuestion's auto-continue: idle time counts while the
             * window is not known to have the focus; a key starts it over */
            long left;
            if (t->focus == 1)
                idle0 = now(t);
            left = t->m_afk_ms - (long)(now(t) - idle0);
            if (left <= 0) {
                choice = TUI_AWAY;
                break;
            }
            t->m_left = left <= t->m_afk_count_ms ? (int)((left + 999) / 1000) : -1;
            if (left < wait)
                wait = left;
        }
        tui_frame(t);
        r = next_key(t, &k, wait);
        if (r < 0)
            break;
        if (!r) {
            check_size(t);
            continue;
        }
        idle0 = now(t);
        if (t->m_noting >= 0) {
            /* the comment field: typed text; Enter answers with it, Tab,
             * Shift+Tab or Esc close it */
            long l = (long)strlen(t->m_note);
            if (k.k == K_CHAR && k.ch >= 0x20) {
                char u[8];
                int ul = vw_put_utf8(u, k.ch);
                if (l + ul < (long)sizeof(t->m_note)) {
                    memcpy(t->m_note + l, u, (size_t)ul);
                    t->m_note[l + ul] = 0;
                }
                continue;
            }
            if (k.k == K_PASTE) {
                long i;
                for (i = 0; i < k.n && l + 1 < (long)sizeof(t->m_note); i++)
                    if ((unsigned char)k.text[i] >= 0x20)
                        t->m_note[l++] = k.text[i];
                t->m_note[l] = 0;
                continue;
            }
            if (k.k == K_BS) {
                while (l > 0 && ((unsigned char)t->m_note[l - 1] & 0xc0) == 0x80)
                    l--;
                if (l > 0)
                    l--;
                t->m_note[l] = 0;
                continue;
            }
            if (k.k == K_ENTER) {
                choice = t->m_noting;
                break;
            }
            if (k.k == K_TAB || k.k == K_BTAB || k.k == K_ESC || k.k == K_UP || k.k == K_DOWN) {
                t->m_noting = -1;
                t->m_note[0] = 0;
                if (k.k != K_UP && k.k != K_DOWN)
                    continue;
            } else {
                continue;
            }
        }
        {
            /* the dialog's bindings (A4 gaps 3): Confirmation for a
             * permission question, Select (with the rewind list's or the
             * theme picker's own context first) for the others */
            int mctx[2], mc = 0, a, done = 0;
            if (t->m_ctx == KC_CONFIRM || t->m_comment)     /* a permission question */
                mctx[mc++] = KC_CONFIRM;
            else {
                if (t->m_ctx == KC_MSGSEL || t->m_ctx == KC_THEME)
                    mctx[mc++] = t->m_ctx;
                mctx[mc++] = KC_SELECT;
            }
            a = km_action(&t->km, mctx, mc, &k, now(t));
            switch (a) {
            case KA_PENDING:
            case KA_CHORD_MISS:
            case KA_TOGGLE:         /* the options here are ticked with Enter */
            case KA_PERM_DEBUG:
            case KA_THEME_SYNTAX:   /* no syntax highlighting to toggle */
            case KA_INERT:
                continue;
            case KA_NEXT_FIELD:
                if (t->m_comment && (t->m_sel == 0 || t->m_sel == n - 1)) {
                    t->m_noting = t->m_sel;     /* a comment on Yes or No */
                    continue;
                }
                t->m_sel = (t->m_sel + 1) % n;
                continue;
            case KA_CONFIRM_CYCLE:
                if (t->m_btab >= 0 && t->m_btab < n) {
                    choice = t->m_btab;     /* a file permission: allowed for the session */
                    done = 1;
                }
                break;
            case KA_PREV:
            case KA_PREV_FIELD:
            case KA_SEL_PREV:
                t->m_sel = t->m_ext ? (t->m_sel > 0 ? t->m_sel - 1 : 0) : (t->m_sel + n - 1) % n;
                continue;
            case KA_NEXT:
            case KA_SEL_NEXT:
                t->m_sel = t->m_ext ? (t->m_sel + 1 < n ? t->m_sel + 1 : n - 1) : (t->m_sel + 1) % n;
                continue;
            case KA_SEL_FIRST:
            case KA_SEL_PGUP:
                t->m_sel = a == KA_SEL_FIRST || t->m_sel < 5 ? 0 : t->m_sel - 5;
                continue;
            case KA_SEL_LAST:
            case KA_SEL_PGDN:
                t->m_sel = a == KA_SEL_LAST || t->m_sel + 5 >= n ? n - 1 : t->m_sel + 5;
                continue;
            case KA_YES:
            case KA_SEL_ACCEPT:
                choice = t->m_sel;
                done = 1;
                break;
            case KA_NO:
            case KA_SEL_CANCEL:
                choice = esc;
                done = 1;
                break;
            default:
                /* no binding: Ctrl+C (reserved) declines, a digit picks,
                 * Tab and Shift+Tab move and allow as before */
                if (k.k == K_CTRL && k.ch == 'c') {
                    choice = esc;
                    done = 1;
                } else if (t->m_ext) {
                    /* pi's ExtensionSelectorComponent: j and k move like the arrows, nothing else is bound */
                    if (k.k == K_CHAR && k.ch == 'k')
                        t->m_sel = t->m_sel > 0 ? t->m_sel - 1 : 0;
                    else if (k.k == K_CHAR && k.ch == 'j')
                        t->m_sel = t->m_sel + 1 < n ? t->m_sel + 1 : n - 1;
                } else if (k.k == K_CHAR && k.ch >= '1' && k.ch < (unsigned long)('1' + n) && k.ch <= '9') {
                    choice = (int)(k.ch - '1');
                    done = 1;
                } else if (k.k == K_BTAB && t->m_btab >= 0 && t->m_btab < n) {
                    choice = t->m_btab;
                    done = 1;
                } else if (k.k == K_TAB)
                    t->m_sel = (t->m_sel + 1) % n;
                break;
            }
            if (done)
                break;
        }
    }
    t->modal = 0;
    t->busy = busy;
    t->m_btab = -1;
    t->m_comment = 0;
    t->m_ctx = KC_SELECT;
    t->m_afk_ms = -1;
    t->m_left = -1;
    t->m_ext = 0;
    t->m_timeout_ms = -1;
    if (choice < 0 || choice != t->m_noting)
        t->m_note[0] = 0;           /* a comment goes only with its own option */
    t->m_noting = -1;
    tui_frame(t);
    return choice;
}

int tui_key(cl_tui *t, cl_key *k, long wait)
{
    return next_key(t, k, wait);
}

int tui_pending(cl_tui *t)
{
    char b[256];
    long n = t->io->read(t->io->u, b, sizeof(b), 0);
    if (n > 0)
        keys_feed(&t->keys, b, n);
    if (n < 0)
        return 1;
    /* a focus report alone is no key */
    while (t->keys.n >= 3 && t->keys.b[0] == 0x1b && t->keys.b[1] == '[' &&
           (t->keys.b[2] == 'I' || t->keys.b[2] == 'O')) {
        t->focus = t->keys.b[2] == 'I';
        memmove(t->keys.b, t->keys.b + 3, (size_t)(t->keys.n - 3));
        t->keys.n -= 3;
    }
    return t->keys.n > 0 || t->keys.paste;
}

int tui_pick_run(cl_tui *t, tui_pick *p)
{
    int res = -1, ctx[1];
    ctx[0] = KC_TREE;
    t->modal = 2;
    t->pick = p;
    for (;;) {
        cl_key k;
        int r, act;
        tui_frame(t);
        r = next_key(t, &k, 500);
        if (r < 0)
            break;
        if (!r) {
            check_size(t);
            continue;
        }
        act = km_action(&t->km, ctx, 1, &k, now(t));
        if (act == KA_PENDING || act == KA_CHORD_MISS)
            continue;
        if (p->key && p->key(p->u, &k, act))
            continue;
        if (p->layout == TUI_PICK_AUTH && k.k == K_CTRL && k.ch == 'c')
            break;                  /* pi's tui.select.cancel: Esc or Ctrl+C */
        if (act == KA_SEL_NEXT || (act == KA_NONE && k.k == K_DOWN)) {
            if (p->n)
                p->sel = p->sel == p->n - 1 ? 0 : p->sel + 1;
        } else if (act == KA_SEL_PREV || (act == KA_NONE && k.k == K_UP)) {
            if (p->n)
                p->sel = p->sel == 0 ? p->n - 1 : p->sel - 1;
        } else if (act == KA_SEL_PGUP || (act == KA_NONE && k.k == K_LEFT)) {
            p->sel = p->sel - p->maxrows < 0 ? 0 : p->sel - p->maxrows;
        } else if (act == KA_SEL_PGDN || (act == KA_NONE && k.k == K_RIGHT)) {
            p->sel = p->sel + p->maxrows > p->n - 1 ? (p->n ? p->n - 1 : 0) : p->sel + p->maxrows;
        } else if (act == KA_SEL_ACCEPT) {
            if (p->n) {
                res = p->sel;
                break;
            }
        } else if (act == KA_SEL_CANCEL)
            break;
    }
    t->modal = 0;
    t->pick = 0;
    tui_frame(t);
    return res;
}

static int line_input(cl_tui *t, const char *title, const char *hint, char *out, long cap, int ext, long timeout_ms)
{
    int res = -1;
    unsigned long start = now(t);
    t->modal = 3;
    t->pi_in_hint = hint;
    t->pi_in_ext = ext;
    t->m_timeout_ms = timeout_ms;
    t->m_left = -1;
    t->pi_in_title = title;
    t->pi_in[0] = 0;
    for (;;) {
        cl_key k;
        int r;
        long l = (long)strlen(t->pi_in);
        long wait = 500;
        if (t->m_timeout_ms >= 0) {
            /* ExtensionUIDialogOptions.timeout: runs out whatever is typed; pi's CountdownTimer shows the seconds */
            long left = t->m_timeout_ms - (long)(now(t) - start);
            long next;
            if (left <= 0) {
                res = 0;
                break;
            }
            t->m_left = (int)((left + 999) / 1000);
            next = left % 1000 ? left % 1000 : 1000;
            if (next < wait)
                wait = next;
        }
        tui_frame(t);
        r = next_key(t, &k, wait);
        if (r < 0)
            break;
        if (!r) {
            check_size(t);
            continue;
        }
        if (k.k == K_ENTER) {
            cl_copy(out, t->pi_in, cap);
            res = 1;
            break;
        }
        if (k.k == K_ESC || (k.k == K_CTRL && k.ch == 'c')) {
            res = 0;
            break;
        }
        if (k.k == K_BS) {
            if (l) {
                do
                    l--;
                while (l > 0 && ((unsigned char)t->pi_in[l] & 0xc0) == 0x80);
                t->pi_in[l] = 0;
            }
        } else if (k.k == K_CHAR && k.ch >= 0x20 && l + 5 < (long)sizeof(t->pi_in)) {
            l += vw_put_utf8(t->pi_in + l, k.ch);
            t->pi_in[l] = 0;
        } else if (k.k == K_PASTE) {
            long i;
            for (i = 0; i < k.n && l + 1 < (long)sizeof(t->pi_in); i++)
                if ((unsigned char)k.text[i] >= 0x20)
                    t->pi_in[l++] = k.text[i];
            t->pi_in[l] = 0;
        }
    }
    t->modal = 0;
    t->pi_in_hint = 0;
    t->pi_in_ext = 0;
    t->m_timeout_ms = -1;
    t->m_left = -1;
    tui_frame(t);
    return res;
}

int tui_line_input(cl_tui *t, const char *title, char *out, long cap)
{
    return line_input(t, title, 0, out, cap, 0, -1);
}

int tui_line_input_hint(cl_tui *t, const char *title, const char *hint, char *out, long cap)
{
    return line_input(t, title, hint, out, cap, 0, -1);
}

int tui_ext_input(cl_tui *t, const char *title, char *out, long cap, long timeout_ms)
{
    return line_input(t, title, 0, out, cap, 1, timeout_ms > 0 ? timeout_ms : -1);
}

int tui_dialog_read(cl_tui *t, char *out, long cap)
{
    struct tui_dialog *d = t->dlg;
    int i, last = -1, r;
    if (!d)
        return -1;
    for (i = 0; i < d->n; i++)
        if (d->l[i].kind == TUI_DLG_INPUT)
            last = i;
    if (last < 0)
        return -1;
    r = line_input(t, "", 0, out, cap, 2, -1);
    if (r == 1) {
        char *copy = (char *)malloc(strlen(out) + 1);
        if (copy) {
            strcpy(copy, out);
            free(d->l[last].text);
            d->l[last].text = copy;
        }
    }
    tui_frame(t);
    return r;
}

int tui_dialog_cancelled(cl_tui *t)
{
    cl_key k;
    int stop = 0;
    while (next_key(t, &k, 1) > 0)
        if (k.k == K_ESC || (k.k == K_CTRL && k.ch == 'c'))
            stop = 1;
    return stop;
}

int tui_ext_menu(cl_tui *t, const char *title, const char *const *opt, int n, long timeout_ms)
{
    t->m_ext = 1;
    t->m_timeout_ms = timeout_ms > 0 ? timeout_ms : -1;
    return tui_menu(t, title, 0, opt, n, 0, -1);
}
