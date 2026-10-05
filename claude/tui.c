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
    t->B = t->rows;
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
    t->notify_after_s = 10;
    t->edit_path = "T:claude-prompt.txt";
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
    free(t->saved);
    t->saved = 0;
    free(t->stash);
    t->stash = 0;
    for (i = 0; i < TUI_DIRS; i++)
        jw_free(&t->dirs[i].names);
    hist_free(&t->hist);
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

/* the frame's colour: the box's mode's */
static const char *frame(cl_tui *t)
{
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

/* the slash commands matching the box's text: their indexes */
static int slash_matches(cl_tui *t, int *idx, int max)
{
    int i, n = 0;
    const char *s = t->ed.b;
    long len = t->ed.n;
    if (t->modal || t->search || t->box != BOX_PROMPT || !t->cmds || len < 1 || s[0] != '/' || t->mclosed ||
        memchr(s, ' ', (size_t)len) ||
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
    "ctrl+enter to send now",   "alt+p to switch model",   "ctrl+l to redraw"
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
    nm = slash_matches(t, idx, 8);
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

void tui_lines(cl_tui *t, const char *s, long n)
{
    long a = 0, i;
    int r0 = t->tr, rows = 0, first = 1;
    if (!t->nolog)
        tui_log(t, s, n);
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
            ring_add(t, s + a, i - a);
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
    t->B = t->rows;
    t->tr = 1;
    t->full = 1;
    tui_frame(t);
}

void tui_redraw(cl_tui *t)
{
    int k, used = 0, first = t->nring;
    if (!t->started)
        return;
    t->n_redraws++;
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

int tui_resized(cl_tui *t)
{
    int c, r;
    get_size(t, &c, &r);
    if (c == t->cols && r == t->rows)
        return 0;
    t->cols = c;
    t->rows = r;
    put(t, "\033[r");
    t->B = r;
    if (t->tr > r)
        t->tr = r;
    t->full = 1;
    return 1;
}

static void check_size(cl_tui *t)
{
    tui_resized(t);
}

void tui_title(cl_tui *t, const char *s)
{
    if (!t->started || !strcmp(t->title, s))
        return;
    cl_copy(t->title, s, sizeof(t->title));
    put(t, "\033]2;");
    put(t, s);
    put(t, "\a");
    flush(t);
}

void tui_notify(cl_tui *t, const char *msg)
{
    if (!t->started)
        return;
    /* the bell, then OSC 9 (UP-Term, iTerm2, kitty: a desktop notice) */
    put(t, "\a\033]9;");
    put(t, msg);
    put(t, "\a");
    flush(t);
    t->n_notify++;
}

static const char modes_on[] = "\033[?2004h\033[>1u\033[>4;1m";
static const char modes_off[] = "\033[?2004l\033[<u\033[>4;0m";

int tui_start(cl_tui *t)
{
    unsigned long t0;
    if (t->io->raw && t->io->raw(t->io->u, 1))
        return -1;
    get_size(t, &t->cols, &t->rows);
    /* bracketed paste, kitty's disambiguation (Esc and Shift+Enter come
     * as CSI u), modifyOtherKeys 1 for terminals without it; where is the
     * cursor? */
    {
        static const char start[] = "\033[?2004h\033[>1u\033[>4;1m\033[6n";
        t->io->write(t->io->u, start, (long)sizeof(start) - 1);
    }
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
    put(t, "\033[22;0t");       /* the window's title kept, put back at the end */
    tui_title(t, "Claude");
    tui_frame(t);
    return 0;
}

static void park(cl_tui *t)
{
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
    unsigned long n = now(t);
    if (t->busy && n - t->tick >= 200) {
        t->tick = n;
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
    long s = t->ed.cur;
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
    if (pl >= (long)sizeof(tok))
        return -1;
    memcpy(tok, t->ed.b + s + skip, (size_t)pl);
    tok[pl] = 0;
    n = t->complete(t->cu, tok, t->comp, TUI_COMP);
    t->ctok = s;
    t->cskip = skip;
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
    put(t, modes_on);
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
        t->mclosed = 0;
        t->msel = 0;
        t->cclosed = 0;
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

static int handle(cl_tui *t, cl_key *k, int busy, char *buf, long cap)
{
    int idx[8];
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
    if (t->ctrlx) {
        t->ctrlx = 0;
        if (k->k == K_CTRL && k->ch == 'e') {
            external_edit(t);
            return H_GO;
        }
        if (k->k == K_CTRL && k->ch == 's') {
            cl_key enter;
            if (busy)
                return send_now(t);
            memset(&enter, 0, sizeof(enter));
            enter.k = K_ENTER;
            k = &enter;
            return handle(t, k, busy, buf, cap);
        }
        if (k->k == K_CTRL)
            return H_GO;            /* Ctrl+X Ctrl+K: no background subagents here */
    }
    if (t->search) {
        switch (k->k) {
        case K_CHAR: {
            char u[8];
            long l = (long)strlen(t->sq);
            int ul = vw_put_utf8(u, k->ch);
            if (l + ul < (long)sizeof(t->sq)) {
                memcpy(t->sq + l, u, (size_t)ul);
                t->sq[l + ul] = 0;
                search_find(t, 0);
            }
            return H_GO;
        }
        case K_CTRL:
            if (k->ch == 'r') {
                search_find(t, 1);
                return H_GO;
            }
            if (k->ch == 'c') {
                search_end(t, 0);
                return H_GO;
            }
            search_end(t, 1);
            break;
        case K_BS: {
            long l = (long)strlen(t->sq);
            if (!l) {
                search_end(t, 0);
                return H_GO;
            }
            do
                l--;
            while (l > 0 && ((unsigned char)t->sq[l] & 0xc0) == 0x80);
            t->sq[l] = 0;
            search_find(t, 0);
            return H_GO;
        }
        case K_TAB:
        case K_ESC:
            search_end(t, 1);
            return H_GO;
        default:
            search_end(t, 1);       /* Enter sends the match; other keys edit it */
            break;
        }
    }
    if (t->copen) {
        switch (k->k) {
        case K_UP:
            t->csel = (t->csel + t->ncomp - 1) % t->ncomp;
            return H_GO;
        case K_DOWN:
            t->csel = (t->csel + 1) % t->ncomp;
            return H_GO;
        case K_TAB:
            comp_apply(t, t->comp[t->csel], 1);
            t->copen = 0;
            return H_GO;
        case K_ENTER: {
            /* the row taken, unless it is what is typed: then Enter sends */
            long have = t->ed.cur - t->ctok - t->cskip;
            if (k->mods & KM_CTRL || ((long)strlen(t->comp[t->csel]) == have &&
                                      !strncmp(t->comp[t->csel], t->ed.b + t->ctok + t->cskip, (size_t)have))) {
                t->copen = 0;
                break;
            }
            comp_apply(t, t->comp[t->csel], 1);
            t->copen = 0;
            return H_GO;
        }
        case K_ESC:
            t->copen = 0;
            t->cclosed = 1;
            return H_GO;
        default:
            break;                  /* typing narrows it (edit -> comp_live) */
        }
    }
    switch (k->k) {
    case K_ENTER: {
        char *line;
        if ((k->mods & KM_CTRL) && busy)
            return send_now(t);     /* Ctrl+Enter */
        if ((t->ed.vim == VIM_OFF || t->ed.vim == VIM_INSERT) && t->ed.cur > 0 && t->ed.b[t->ed.cur - 1] == '\\') {
            cl_key bs;
            memset(&bs, 0, sizeof(bs));
            bs.k = K_BS;
            ed_key(&t->ed, &bs);
            ed_insert(&t->ed, "\n", 1);
            return H_GO;
        }
        line = take_box(t);
        if (!line)
            return H_GO;
        if (busy || !buf) {
            enqueue(t, line);
            return H_GO;
        }
        cl_copy(buf, line, cap);
        free(line);
        return H_SUBMIT;
    }
    case K_TAB:
        if (complete(t))
            ed_insert(&t->ed, " ", 1);
        else if (!comp_tab(t) && t->box == BOX_BASH && t->ed.n)
            bash_hist_tab(t);
        return H_GO;
    case K_BTAB:
        cycle_mode(t);
        return H_GO;
    case K_UP:
    case K_DOWN: {
        int n = slash_matches(t, idx, 8);
        if (n) {
            t->msel = (t->msel + (k->k == K_UP ? n - 1 : 1)) % n;
            return H_GO;
        }
        if (k->k == K_UP && t->nq && ed_lstart(&t->ed, t->ed.cur) == 0) {
            take_back(t);
            return H_GO;
        }
        edit(t, k);
        return H_GO;
    }
    case K_ESC:
        if (t->ed.vim == VIM_INSERT || t->ed.vim == VIM_VISUAL || t->ed.vim == VIM_VLINE ||
            (t->ed.vim == VIM_NORMAL && !vim_idle(&t->ed))) {
            edit(t, k);
            return H_GO;
        }
        if (slash_matches(t, idx, 8)) {
            t->mclosed = 1;
            return H_GO;
        }
        if (t->box != BOX_PROMPT && !t->ed.n) {
            t->box = BOX_PROMPT;
            return H_GO;
        }
        if (busy)
            return H_STOP;
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
    case K_BS:
        if (!t->ed.n && t->box != BOX_PROMPT) {
            t->box = BOX_PROMPT;
            return H_GO;
        }
        edit(t, k);
        return H_GO;
    case K_CHAR:
        if (!t->ed.n && t->box == BOX_PROMPT && (t->ed.vim == VIM_OFF || t->ed.vim == VIM_INSERT) &&
            (k->ch == '!' || k->ch == '#')) {
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
        switch (k->ch) {
        case 'c':
            if (busy)
                return H_STOP;
            if (t->ed.n || t->box != BOX_PROMPT) {
                box_reset(t);
                return H_GO;
            }
            if (t->quit_armed == 'c')
                return H_QUIT;
            t->quit_armed = 'c';
            cl_copy(t->hint, "Press Ctrl+C again to exit", sizeof(t->hint));
            return H_GO;
        case 'd':
            if (t->ed.n) {
                edit(t, k);
                return H_GO;
            }
            if (busy)
                return H_GO;
            if (t->quit_armed == 'd' && now(t) - t->quit_ms <= 800)
                return H_QUIT;
            t->quit_armed = 'd';
            t->quit_ms = now(t);
            cl_copy(t->hint, "Press Ctrl+D again to exit", sizeof(t->hint));
            return H_GO;
        case 'o':
            tui_transcript(t);
            return H_GO;
        case 't':
            t->show_todos = !t->show_todos;
            return H_GO;
        case 'l':
            tui_redraw(t);
            return H_GO;
        case 'g':
            external_edit(t);
            return H_GO;
        case 'x':
            t->ctrlx = 1;
            return H_GO;
        case 'r':
            search_start(t);
            return H_GO;
        case 's':
            stash(t);
            return H_GO;
        case 'b':
            if (!busy) {
                edit(t, k);         /* nothing runs: back one character */
                return H_GO;
            }
            if (t->bgable)
                t->bg_req = 1;
            else
                cl_copy(t->hint, "Nothing to move to the background now", sizeof(t->hint));
            return H_GO;
        case 'u':
            if (!t->ed.n && t->box != BOX_PROMPT) {
                t->box = BOX_PROMPT;
                return H_GO;
            }
            edit(t, k);
            return H_GO;
        default:
            edit(t, k);
            return H_GO;
        }
    case K_ALT:
        if (k->ch == 'p') {
            /* Alt+P: the model picker, the draft left in the box */
            if (busy || !buf) {
                char *m = dupn("/model", 6);
                if (m)
                    enqueue(t, m);
                return H_GO;
            }
            cl_copy(buf, "/model", cap);
            t->keycmd = 1;
            return H_SUBMIT;
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
    t->modal = 1;
    t->m_title = title;
    t->m_q = question;
    t->m_opt = opt;
    t->m_n = n;
    t->m_sel = sel;
    t->busy = 0;
    t->m_noting = -1;
    t->m_note[0] = 0;
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
        if (k.k == K_TAB && t->m_comment && (t->m_sel == 0 || t->m_sel == n - 1)) {
            t->m_noting = t->m_sel;     /* a comment on Yes or No */
            continue;
        }
        if (k.k == K_BTAB && t->m_btab >= 0 && t->m_btab < n) {
            choice = t->m_btab;     /* a file permission: allowed for the session */
            break;
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
        } else if (k.k == K_CHAR && k.ch >= '1' && k.ch < (unsigned long)('1' + n) && k.ch <= '9') {
            choice = (int)(k.ch - '1');
            break;
        }
    }
    t->modal = 0;
    t->busy = busy;
    t->m_btab = -1;
    t->m_comment = 0;
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
