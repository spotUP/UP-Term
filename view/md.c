/* md -- the Markdown renderer (md.h).
 *
 * Blocks: a line is matched against the open containers (block quotes, list
 * items) the CommonMark way, new containers open, then the line goes to the
 * open leaf (paragraph, fenced or indented code, HTML, table) or starts one.
 * A leaf is drawn when it closes. Inlines: the text becomes items (text,
 * code spans, delimiter runs, brackets), brackets resolve into links when
 * they close, emphasis is matched by the delimiter-run rules, and the
 * result is a flat run of bytes with a style word per byte, which is
 * word-wrapped to the width and written with SGR and OSC 8. */
#include <stdlib.h>
#include <string.h>
#include "md.h"
#include "hl_lex.h"

#define MAXC 24                 /* nested containers */
#define TCOLS 32                /* table columns */

enum { C_QUOTE, C_ITEM };
enum { L_NONE, L_PARA, L_FENCE, L_ICODE, L_HTML, L_TABLE };
enum { I_NONE, I_TEXT, I_CODE, I_DELIM, I_BOPEN, I_SOFT, I_HARD, I_LINKEND };
enum { A_LEFT, A_CENTER, A_RIGHT };

typedef struct cont {
    int type;
    int indent;                 /* item: its content's column */
    int ordered, marker, task, level;
    long num;
    char bullet;
} cont;

typedef struct buf {
    char *p;
    long n, cap;
} buf;

typedef struct ref {
    char *label, *url;
} ref;

typedef struct item {
    int kind, cls, attr;
    int count, orig, can_open, can_close, active, image, show;
    const char *s;
    long n;
    long link;                  /* urls offset + 1, 0 none */
    long src;                   /* a bracket: where its text starts */
} item;

typedef struct lspan {
    long a, b;
} lspan;

typedef struct listmem {
    int valid, ordered;
    char bullet;
    long num;
} listmem;

typedef struct md {
    const md_opts *o;
    vw_out *out;
    int utf8, oom;
    cont c[MAXC];
    int nc;
    int leaf;
    buf text;
    int nlines;
    int fchar, flen, findent;
    char lang[32];
    int talign[TCOLS], tcols;
    int any, blank, force_blank;
    int kept;                   /* containers open since the last block */
    listmem last[MAXC];
    ref *refs;
    int nrefs, crefs;
    item *it;
    long nit, cit;
    long *stk;
    long nstk, cstk;
    buf flat;
    unsigned long *fst;
    long cfst;
    buf urls;
    lspan *ln;
    long nln, cln;
    buf line;
} md;

/* ---- small things -------------------------------------------------------- */

static int grow(md *m, void **p, long *cap, long need, long size)
{
    long nc;
    void *np;
    if (need <= *cap)
        return 1;
    nc = *cap ? *cap : 64;
    while (nc < need)
        nc *= 2;
    np = realloc(*p, (size_t)(nc * size));
    if (!np) {
        m->oom = 1;
        return 0;
    }
    *p = np;
    *cap = nc;
    return 1;
}

static void badd(md *m, buf *b, const char *s, long n)
{
    if (!grow(m, (void **)&b->p, &b->cap, b->n + n + 1, 1))
        return;
    if (n)
        memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}

static int is_sp(int c)
{
    return c == ' ' || c == '\t';
}

static int is_ws(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0;
}

static int is_punct(int c)
{
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
}

static int is_alpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static long indent(const char *s, long n)
{
    long i = 0;
    while (i < n && s[i] == ' ')
        i++;
    return i;
}

static int is_blank(const char *s, long n)
{
    long i;
    for (i = 0; i < n; i++)
        if (!is_sp((unsigned char)s[i]) && s[i] != '\r')
            return 0;
    return 1;
}

static long run(const char *s, long n, int c)
{
    long i = 0;
    while (i < n && s[i] == (char)c)
        i++;
    return i;
}

/* ***, - - -, ___ */
static int is_hr(const char *s, long n)
{
    long i, k = 0;
    int c = n ? (unsigned char)s[0] : 0;
    if (c != '-' && c != '*' && c != '_')
        return 0;
    for (i = 0; i < n; i++) {
        if (s[i] == (char)c)
            k++;
        else if (!is_sp((unsigned char)s[i]))
            return 0;
    }
    return k >= 3;
}

/* a list marker at s: its length (with the '.' or ')'), 0 none */
static int list_marker(const char *s, long n, int *ordered, long *num, char *bullet)
{
    long i = 0, v = 0;
    if (n >= 1 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && (n == 1 || is_sp((unsigned char)s[1]))) {
        *ordered = 0;
        *num = 0;
        *bullet = s[0];
        return 1;
    }
    while (i < n && i < 9 && is_digit((unsigned char)s[i]))
        v = v * 10 + (s[i++] - '0');
    if (i > 0 && i < n && (s[i] == '.' || s[i] == ')') && (i + 1 == n || is_sp((unsigned char)s[i + 1]))) {
        *ordered = 1;
        *num = v;
        *bullet = s[i];
        return (int)i + 1;
    }
    return 0;
}

static int starts_block(const char *s, long n)
{
    long p = indent(s, n), r;
    int o;
    long num;
    char b;
    if (p >= 4)
        return 0;
    if (p >= n)
        return 1;
    s += p;
    n -= p;
    if (s[0] == '>')
        return 1;
    if (s[0] == '#' && (r = run(s, n, '#')) <= 6 && (r == n || is_sp((unsigned char)s[r])))
        return 1;
    if ((s[0] == '`' || s[0] == '~') && run(s, n, s[0]) >= 3)
        return 1;
    if (is_hr(s, n))
        return 1;
    if (list_marker(s, n, &o, &num, &b) > 0)
        return 1;
    return 0;
}

/* ---- glyphs -------------------------------------------------------------- */

enum { G_QUOTE, G_B1, G_B2, G_B3, G_H, G_HH, G_V, G_TL, G_TM, G_TR, G_ML, G_MM, G_MR, G_BL, G_BM, G_BR };

static const char *glyph(md *m, int g)
{
    static const char *const u[] = { "\342\224\202", "\342\200\242", "\342\227\213", "-", "\342\224\200",
                                     "\342\225\220", "\342\224\202", "\342\224\214", "\342\224\254",
                                     "\342\224\220", "\342\224\234", "\342\224\274", "\342\224\244",
                                     "\342\224\224", "\342\224\264", "\342\224\230" };
    static const char *const a[] = { "|", "*", "o", "-", "-", "=", "|", "+", "+", "+", "+", "+", "+",
                                     "+", "+", "+" };
    return m->utf8 ? u[g] : a[g];
}

/* ---- the frame of a line: quote bars and list markers -------------------- */

static int num_width(long v)
{
    int w = 1;
    while (v >= 10) {
        v /= 10;
        w++;
    }
    return w;
}

static int marker_width(const cont *c)
{
    if (c->type == C_QUOTE)
        return 2;
    return (c->ordered ? num_width(c->num) + 2 : 2) + (c->task ? 4 : 0);
}

static int prefix_width(md *m)
{
    int k, w = 0;
    for (k = 0; k < m->nc; k++)
        w += marker_width(&m->c[k]);
    return w;
}

static int avail(md *m)
{
    int w = m->o->width - prefix_width(m);
    return w < 8 ? 8 : w;
}

static void prefix(md *m, int blank)
{
    vw_out *o = m->out;
    int k, last = -1;
    if (blank)
        for (k = 0; k < m->nc && k < m->kept; k++)
            if (m->c[k].type == C_QUOTE)
                last = k;
    for (k = 0; k < m->nc; k++) {
        cont *c = &m->c[k];
        if (blank && k > last)
            break;
        if (c->type == C_QUOTE) {
            vo_class(o, MD_QUOTE);
            vo_rawz(o, glyph(m, G_QUOTE));
            if (!(blank && k == last))
                vo_raw(o, " ", 1);
            continue;
        }
        if (c->marker && !blank) {
            vo_class(o, MD_BULLET);
            if (c->ordered) {
                char t[16];
                int i = num_width(c->num);
                long v = c->num;
                t[i] = c->bullet;
                t[i + 1] = ' ';
                while (i > 0) {
                    t[--i] = (char)('0' + v % 10);
                    v /= 10;
                }
                vo_raw(o, t, num_width(c->num) + 2);
            } else {
                vo_rawz(o, glyph(m, c->level % 3 == 0 ? G_B1 : c->level % 3 == 1 ? G_B2 : G_B3));
                vo_raw(o, " ", 1);
            }
            if (c->task) {
                vo_class(o, MD_TASK);
                vo_rawz(o, c->task == 2 ? "[x] " : "[ ] ");
            }
            c->marker = 0;
        } else {
            vo_reset(o);
            vo_spaces(o, marker_width(c));
        }
    }
    vo_reset(o);
}

static void endline(md *m)
{
    vo_link(m->out, 0);
    vo_reset(m->out);
    vo_raw(m->out, "\n", 1);
}

static int items_open(md *m)
{
    int k, d = 0;
    for (k = 0; k < m->nc; k++)
        d += m->c[k].type == C_ITEM;
    return d;
}

/* the blank line before a block: always, except between the blocks of a
 * tight list (no blank line in the source) */
static void gap(md *m)
{
    int d = items_open(m), k;
    if (m->any && (m->force_blank || m->blank || d == 0)) {
        prefix(m, 1);
        endline(m);
    }
    m->any = 1;
    m->blank = 0;
    m->force_blank = 0;
    m->kept = m->nc;
    for (k = d; k < MAXC; k++)
        m->last[k].valid = 0;
}

/* ---- the flat run: bytes and a style word each --------------------------- */

#define WORD(cls, attr, link) ((unsigned long)(cls) | ((unsigned long)(attr) << 8) | ((unsigned long)(link) << 16))

static void fput(md *m, const char *s, long n, unsigned long w)
{
    long i;
    if (!grow(m, (void **)&m->fst, &m->cfst, m->flat.n + n + 1, sizeof(unsigned long)))
        return;
    for (i = 0; i < n; i++)
        m->fst[m->flat.n + i] = w;
    badd(m, &m->flat, s, n);
}

/* text with its blanks folded into one (a paragraph's spacing), in one
 * pass straight into the run */
static void ftext(md *m, const char *s, long n, unsigned long w)
{
    long i, k;
    char *d;
    unsigned long *ds;
    int last;
    if (!grow(m, (void **)&m->fst, &m->cfst, m->flat.n + n + 1, sizeof(unsigned long)) ||
        !grow(m, (void **)&m->flat.p, &m->flat.cap, m->flat.n + n + 1, 1))
        return;
    k = m->flat.n;
    d = m->flat.p;
    ds = m->fst;
    last = k ? (unsigned char)d[k - 1] : ' ';
    for (i = 0; i < n; i++) {
        int c = (unsigned char)s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (last == ' ' || last == '\n')
                continue;
            c = ' ';
        }
        d[k] = (char)c;
        ds[k++] = w;
        last = c;
    }
    d[k] = 0;
    m->flat.n = k;
}

static void style_of(md *m, unsigned long w, hl_sty *s)
{
    *s = m->out->theme->s[w & 0xFF];
    s->attr |= (unsigned)((w >> 8) & 0xFF);
}

/* flat[a..b) to the output in its styles */
static void emit(md *m, long a, long b)
{
    vw_out *o = m->out;
    while (a < b) {
        long e = a;
        unsigned long w = m->fst[a];
        hl_sty st;
        long link;
        if (m->flat.p[a] == '\n') {
            a++;
            continue;
        }
        while (e < b && m->fst[e] == w && m->flat.p[e] != '\n')
            e++;
        style_of(m, w, &st);
        vo_style(o, &st);
        link = (long)(w >> 16);
        vo_link(o, link ? m->urls.p + link - 1 : 0);
        vo_text(o, m->flat.p + a, e - a);
        a = e;
    }
}

static void add_line(md *m, long a, long b)
{
    if (!grow(m, (void **)&m->ln, &m->cln, m->nln + 1, sizeof(lspan)))
        return;
    m->ln[m->nln].a = a;
    m->ln[m->nln].b = b;
    m->nln++;
}

/* flat[a..b) in lines of at most w columns (m->ln): words move whole, a
 * word longer than a line is cut, '\n' ends a line */
static void wrap(md *m, long a, long b, int w)
{
    const char *p = m->flat.p;
    long i = a, ls, le;
    int lw = 0;
    m->nln = 0;
    if (w < 1)
        w = 1;
    while (i < b && p[i] == ' ')
        i++;
    ls = le = i;
    while (i < b) {
        long j;
        int ww, gw;
        if (p[i] == '\n') {
            add_line(m, ls, le);
            i++;
            while (i < b && p[i] == ' ')
                i++;
            ls = le = i;
            lw = 0;
            continue;
        }
        if (p[i] == ' ') {
            i++;
            continue;
        }
        j = i;
        while (j < b && p[j] != ' ' && p[j] != '\n')
            j++;
        ww = vw_width(p + i, j - i);
        if (lw == 0) {
            if (ww <= w) {
                ls = i;
                lw = ww;
                le = j;
                i = j;
                continue;
            }
            {
                long k = i + vw_fit(p + i, j - i, w);
                if (k == i) {
                    unsigned long cp;
                    k = i + vw_char(p + i, j - i, &cp);
                }
                add_line(m, i, k);
                i = k;
                ls = le = i;
                continue;
            }
        }
        gw = vw_width(p + le, i - le);
        if (lw + gw + ww <= w) {
            lw += gw + ww;
            le = j;
            i = j;
            continue;
        }
        add_line(m, ls, le);
        lw = 0;
        ls = le = i;
    }
    if (lw > 0 || m->nln == 0)
        add_line(m, ls, le);
}

/* ---- inlines ------------------------------------------------------------- */

static item *add_item(md *m, int kind, const char *s, long n)
{
    item *t;
    if (!grow(m, (void **)&m->it, &m->cit, m->nit + 1, sizeof(item)))
        return 0;
    t = &m->it[m->nit++];
    memset(t, 0, sizeof(*t));
    t->kind = kind;
    t->cls = -1;
    t->s = s;
    t->n = n;
    t->active = 1;
    return t;
}

static void text_item(md *m, const char *s, long a, long b)
{
    if (b > a)
        add_item(m, I_TEXT, s + a, b - a);
}

/* a label as references compare them: lower case, blanks folded */
static void norm_label(const char *s, long n, char *out, int max)
{
    long i;
    int k = 0, sp = 0;
    for (i = 0; i < n && k < max - 1; i++) {
        int c = (unsigned char)s[i];
        if (is_ws(c)) {
            sp = k > 0;
            continue;
        }
        if (sp && k < max - 2)
            out[k++] = ' ';
        sp = 0;
        out[k++] = (char)lower(c);
    }
    out[k] = 0;
}

static const char *find_ref(md *m, const char *s, long n)
{
    char l[128];
    int i;
    norm_label(s, n, l, sizeof(l));
    if (!l[0])
        return 0;
    for (i = 0; i < m->nrefs; i++)
        if (!strcmp(m->refs[i].label, l))
            return m->refs[i].url;
    return 0;
}

/* a URL kept for the line writer: its offset + 1 */
static long keep_url(md *m, const char *s, long n, int unescape)
{
    long at = m->urls.n, i;
    for (i = 0; i < n; i++) {
        if (unescape && s[i] == '\\' && i + 1 < n && is_punct((unsigned char)s[i + 1]))
            i++;
        badd(m, &m->urls, s + i, 1);
    }
    badd(m, &m->urls, "", 1);
    return m->oom ? 0 : at + 1;
}

static void process_emphasis(md *m, long lo, long hi)
{
    long ci, oi, k;
    for (ci = lo; ci < hi; ci++) {
        item *c = &m->it[ci];
        if (c->kind != I_DELIM || !c->can_close || !c->active)
            continue;
        while (c->count > 0) {
            item *o = 0;
            int use, attr;
            for (oi = ci - 1; oi >= lo; oi--) {
                item *t = &m->it[oi];
                if (t->kind != I_DELIM || t->s[0] != c->s[0] || !t->can_open || !t->active || t->count <= 0)
                    continue;
                if (c->s[0] != '~' && (t->can_close || c->can_open) && (t->orig + c->orig) % 3 == 0 &&
                    !(t->orig % 3 == 0 && c->orig % 3 == 0))
                    continue;
                if (c->s[0] == '~' && t->count != c->count)
                    continue;
                o = t;
                break;
            }
            if (!o)
                break;
            use = c->count >= 2 && o->count >= 2 ? 2 : 1;
            attr = c->s[0] == '~' ? HL_A_STRIKE : use == 2 ? HL_A_BOLD : HL_A_ITALIC;
            for (k = oi + 1; k < ci; k++) {
                m->it[k].attr |= attr;
                if (m->it[k].kind == I_DELIM)
                    m->it[k].active = 0;
            }
            o->count -= use;
            c->count -= use;
        }
        if (c->count > 0 && !c->can_open)
            c->active = 0;
    }
}

/* ']' at *pi: a link or image when what follows makes one */
static void close_bracket(md *m, const char *s, long n, long *pi)
{
    long i = *pi, oi = -1, j, k, e = -1, ua = 0, ub = 0, link, t;
    const char *url = 0;
    item *op, *end;
    int unesc = 1;
    if (m->nstk > 0) {
        oi = m->stk[m->nstk - 1];
        if (!m->it[oi].active) {
            /* a bracket inside a link's text: no link of its own */
            m->it[oi].kind = I_TEXT;
            m->nstk--;
            oi = -1;
        }
    }
    if (oi < 0) {
        add_item(m, I_TEXT, s + i, 1);
        *pi = i + 1;
        return;
    }
    op = &m->it[oi];
    j = i + 1;
    if (j < n && s[j] == '(') {
        k = j + 1;
        while (k < n && is_ws((unsigned char)s[k]))
            k++;
        if (k < n && s[k] == '<') {
            ua = k + 1;
            while (k < n && s[k] != '>' && s[k] != '\n')
                k++;
            ub = k;
            if (k < n && s[k] == '>')
                k++;
            else
                k = n;
        } else {
            int depth = 0;
            ua = k;
            while (k < n && !is_ws((unsigned char)s[k])) {
                if (s[k] == '\\' && k + 1 < n) {
                    k += 2;
                    continue;
                }
                if (s[k] == '(')
                    depth++;
                else if (s[k] == ')' && depth-- == 0)
                    break;
                k++;
            }
            ub = k;
        }
        while (k < n && is_ws((unsigned char)s[k]))
            k++;
        if (k < n && (s[k] == '"' || s[k] == '\'' || s[k] == '(')) {
            int q = s[k] == '(' ? ')' : s[k];
            k++;
            while (k < n && s[k] != (char)q) {
                if (s[k] == '\\')
                    k++;
                k++;
            }
            k++;
            while (k < n && is_ws((unsigned char)s[k]))
                k++;
        }
        if (k < n && s[k] == ')') {
            e = k + 1;
            url = s + ua;
        }
    }
    if (!url && j < n && s[j] == '[') {
        k = j + 1;
        while (k < n && s[k] != ']' && s[k] != '[')
            k++;
        if (k < n && s[k] == ']') {
            url = k > j + 1 ? find_ref(m, s + j + 1, k - j - 1) : find_ref(m, s + op->src, i - op->src);
            if (url) {
                e = k + 1;
                unesc = 0;
            }
        }
    }
    if (!url) {
        url = find_ref(m, s + op->src, i - op->src);
        if (url) {
            e = i + 1;
            unesc = 0;
        }
    }
    if (!url) {
        op->kind = I_TEXT;
        m->nstk--;
        add_item(m, I_TEXT, s + i, 1);
        *pi = i + 1;
        return;
    }
    if (unesc)
        link = keep_url(m, url, ub - ua, 1);
    else
        link = keep_url(m, url, (long)strlen(url), 0);
    for (t = oi + 1; t < m->nit; t++) {
        m->it[t].link = link;
        if (m->it[t].kind != I_CODE && m->it[t].cls < 0)
            m->it[t].cls = op->image ? MD_IMAGE : HL_LINK;
    }
    process_emphasis(m, oi + 1, m->nit);
    for (t = oi + 1; t < m->nit; t++)
        if (m->it[t].kind == I_DELIM)
            m->it[t].active = 0;
    if (op->image && oi + 1 == m->nit) {
        item *a = add_item(m, I_TEXT, "image", 5);
        if (a) {
            a->cls = MD_IMAGE;
            a->link = link;
        }
        op = &m->it[oi];
    }
    op->kind = I_NONE;
    end = add_item(m, I_LINKEND, 0, 0);
    op = &m->it[oi];
    if (end && link) {
        const char *u = m->urls.p + link - 1;
        long ul = (long)strlen(u);
        end->link = link;
        end->show = m->o->urls && !op->image && u[0] != '#' && u[0] &&
                    !(ul == i - op->src && !memcmp(u, s + op->src, ul));
    }
    m->nstk--;
    if (!op->image)
        for (t = 0; t < m->nstk; t++)
            if (!m->it[m->stk[t]].image)
                m->it[m->stk[t]].active = 0;
    *pi = e;
}

static const struct {
    const char *name;
    unsigned long cp;
} entities[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", 0xA0 },
    { "copy", 0xA9 }, { "reg", 0xAE }, { "trade", 0x2122 }, { "hellip", 0x2026 }, { "mdash", 0x2014 },
    { "ndash", 0x2013 }, { "laquo", 0xAB }, { "raquo", 0xBB }, { "middot", 0xB7 }, { "bull", 0x2022 },
    { "rarr", 0x2192 }, { "larr", 0x2190 }, { "times", 0xD7 }, { "deg", 0xB0 }, { "euro", 0x20AC },
    { "lsquo", 0x2018 }, { "rsquo", 0x2019 }, { "ldquo", 0x201C }, { "rdquo", 0x201D }, { "check", 0x2713 },
    { 0, 0 }
};

/* &name; &#123; &#x1F; at s: its length, *cp the character; 0 none */
static long entity(const char *s, long n, unsigned long *cp)
{
    long i = 1, v = 0;
    int k;
    if (n < 3)
        return 0;
    if (s[1] == '#') {
        int hex = n > 2 && (s[2] == 'x' || s[2] == 'X');
        i = hex ? 3 : 2;
        while (i < n && i < 10) {
            int c = (unsigned char)s[i], d;
            if (is_digit(c))
                d = c - '0';
            else if (hex && lower(c) >= 'a' && lower(c) <= 'f')
                d = lower(c) - 'a' + 10;
            else
                break;
            v = v * (hex ? 16 : 10) + d;
            i++;
        }
        if (i < n && s[i] == ';' && i > (hex ? 3 : 2)) {
            *cp = v > 0 && v <= 0x10FFFF ? (unsigned long)v : 0xFFFD;
            return i + 1;
        }
        return 0;
    }
    while (i < n && i < 10 && (is_alpha((unsigned char)s[i]) || is_digit((unsigned char)s[i])))
        i++;
    if (i >= n || s[i] != ';')
        return 0;
    for (k = 0; entities[k].name; k++)
        if ((long)strlen(entities[k].name) == i - 1 && !memcmp(entities[k].name, s + 1, i - 1)) {
            *cp = entities[k].cp;
            return i + 1;
        }
    return 0;
}

/* an attribute's value in a tag s[0..n): its start, *len */
static const char *attr_value(const char *s, long n, const char *name, long *len)
{
    long k = (long)strlen(name), i;
    for (i = 1; i + k < n; i++) {
        if (is_ws((unsigned char)s[i - 1]) && !memcmp(s + i, name, k)) {
            long j = i + k;
            while (j < n && is_ws((unsigned char)s[j]))
                j++;
            if (j < n && s[j] == '=') {
                int q;
                j++;
                while (j < n && is_ws((unsigned char)s[j]))
                    j++;
                q = j < n && (s[j] == '"' || s[j] == '\'') ? s[j] : 0;
                if (q) {
                    long e = ++j;
                    while (e < n && s[e] != (char)q)
                        e++;
                    *len = e - j;
                    return s + j;
                } else {
                    long e = j;
                    while (e < n && !is_ws((unsigned char)s[e]) && s[e] != '>')
                        e++;
                    *len = e - j;
                    return s + j;
                }
            }
        }
    }
    return 0;
}

/* a URL with no brackets around it: where it ends */
static long bare_url_end(const char *s, long n, long i)
{
    long e = i, open = 0, k;
    while (e < n && !is_ws((unsigned char)s[e]) && s[e] != '<')
        e++;
    for (;;) {
        int c = e > i ? s[e - 1] : 0;
        if (c && strchr("?!.,:*_~'\";", c)) {
            e--;
            continue;
        }
        if (c == ')') {
            open = 0;
            for (k = i; k < e; k++)
                open += s[k] == '(' ? 1 : s[k] == ')' ? -1 : 0;
            if (open < 0) {
                e--;
                continue;
            }
        }
        break;
    }
    return e;
}

/* The inlines of s[0..n) into the flat run, in class cls. */
static void inlines(md *m, const char *s, long n, int cls)
{
    long i = 0, t = 0, k;
    static unsigned char special[256];
    int c;
    if (!special['\\']) {
        const char *p = "\\`*_~[!]<&\nhw";
        while (*p)
            special[(unsigned char)*p++] = 1;
    }
    m->nit = 0;
    m->nstk = 0;
    while (i < n) {
        /* most text is no markup: past it in one step */
        while (i < n && !special[(unsigned char)s[i]])
            i++;
        if (i >= n)
            break;
        c = (unsigned char)s[i];
        if (c == '\\' && i + 1 < n && (s[i + 1] == '\n' || is_punct((unsigned char)s[i + 1]))) {
            text_item(m, s, t, i);
            if (s[i + 1] == '\n')
                add_item(m, I_HARD, 0, 0);
            else
                add_item(m, I_TEXT, s + i + 1, 1);
            i += 2;
            t = i;
            continue;
        }
        if (c == '`') {
            long r = run(s + i, n - i, '`'), j = i + r;
            while (j < n) {
                if (s[j] == '`') {
                    long r2 = run(s + j, n - j, '`');
                    if (r2 == r)
                        break;
                    j += r2;
                } else {
                    j++;
                }
            }
            if (j < n) {
                long a = i + r, b = j;
                if (b - a >= 2 && (s[a] == ' ' || s[a] == '\n') && (s[b - 1] == ' ' || s[b - 1] == '\n') &&
                    !is_blank(s + a, b - a)) {
                    a++;
                    b--;
                }
                text_item(m, s, t, i);
                add_item(m, I_CODE, s + a, b - a);
                i = j + r;
                t = i;
            } else {
                i += r;
            }
            continue;
        }
        if (c == '*' || c == '_' || c == '~') {
            long r = run(s + i, n - i, c);
            int before = i > 0 ? (unsigned char)s[i - 1] : '\n', after = i + r < n ? (unsigned char)s[i + r] : '\n';
            int left = !is_ws(after) && (!is_punct(after) || is_ws(before) || is_punct(before));
            int right = !is_ws(before) && (!is_punct(before) || is_ws(after) || is_punct(after));
            item *d;
            if (c == '~' && r > 2) {
                i += r;
                continue;
            }
            text_item(m, s, t, i);
            d = add_item(m, I_DELIM, s + i, r);
            if (d) {
                d->count = d->orig = (int)r;
                if (c == '_') {
                    d->can_open = left && (!right || is_punct(before));
                    d->can_close = right && (!left || is_punct(after));
                } else {
                    d->can_open = left;
                    d->can_close = right;
                }
            }
            i += r;
            t = i;
            continue;
        }
        if (c == '[' || (c == '!' && i + 1 < n && s[i + 1] == '[')) {
            item *b;
            long w = c == '!' ? 2 : 1;
            text_item(m, s, t, i);
            b = add_item(m, I_BOPEN, s + i, w);
            if (b) {
                b->image = c == '!';
                b->src = i + w;
                if (grow(m, (void **)&m->stk, &m->cstk, m->nstk + 1, sizeof(long)))
                    m->stk[m->nstk++] = m->nit - 1;
            }
            i += w;
            t = i;
            continue;
        }
        if (c == ']') {
            text_item(m, s, t, i);
            close_bracket(m, s, n, &i);
            t = i;
            continue;
        }
        if (c == '<') {
            long j = i + 1;
            /* an autolink: <scheme:...> or <user@host> */
            while (j < n && !is_ws((unsigned char)s[j]) && s[j] != '<' && s[j] != '>')
                j++;
            if (j < n && s[j] == '>' && j > i + 1) {
                long colon = i + 1, at = 0;
                while (colon < j && (is_alpha((unsigned char)s[colon]) || is_digit((unsigned char)s[colon]) ||
                                     s[colon] == '+' || s[colon] == '.' || s[colon] == '-'))
                    colon++;
                for (k = i + 1; k < j; k++)
                    if (s[k] == '@')
                        at = k;
                if ((colon < j && s[colon] == ':' && colon - i - 1 >= 2 && is_alpha((unsigned char)s[i + 1])) ||
                    (at > i + 1 && at < j - 1 && colon == j)) {
                    item *a;
                    long link;
                    text_item(m, s, t, i);
                    if (at && colon == j) {
                        long u0 = m->urls.n;
                        badd(m, &m->urls, "mailto:", 7);
                        badd(m, &m->urls, s + i + 1, j - i - 1);
                        badd(m, &m->urls, "", 1);
                        link = m->oom ? 0 : u0 + 1;
                    } else {
                        link = keep_url(m, s + i + 1, j - i - 1, 0);
                    }
                    a = add_item(m, I_TEXT, s + i + 1, j - i - 1);
                    if (a)
                        a->link = link, a->cls = HL_LINK;
                    i = j + 1;
                    t = i;
                    continue;
                }
            }
            /* inline HTML: <br> breaks, <img> shows its alt, the rest goes */
            if (i + 1 < n && (is_alpha((unsigned char)s[i + 1]) || s[i + 1] == '/' || s[i + 1] == '!')) {
                j = i + 1;
                if (n - i >= 4 && !memcmp(s + i, "<!--", 4)) {
                    while (j + 2 < n && memcmp(s + j, "-->", 3))
                        j++;
                    j = j + 2 < n ? j + 2 : n;
                } else {
                    while (j < n && s[j] != '>' && s[j] != '<')
                        j++;
                }
                if (j < n && s[j] == '>') {
                    long tl = 0;
                    text_item(m, s, t, i);
                    while (i + 1 + tl < j && is_alpha((unsigned char)s[i + 1 + tl]))
                        tl++;
                    if (tl == 2 && lower(s[i + 1]) == 'b' && lower(s[i + 2]) == 'r') {
                        add_item(m, I_HARD, 0, 0);
                    } else if (tl == 3 && !memcmp(s + i + 1, "img", 3)) {
                        long al = 0, sl = 0;
                        const char *alt = attr_value(s + i, j - i + 1, "alt", &al);
                        const char *src = attr_value(s + i, j - i + 1, "src", &sl);
                        item *a = add_item(m, I_TEXT, alt && al ? alt : "image", alt && al ? al : 5);
                        if (a) {
                            a->cls = MD_IMAGE;
                            if (src && sl)
                                a->link = keep_url(m, src, sl, 0);
                        }
                    }
                    i = j + 1;
                    t = i;
                    continue;
                }
            }
            i++;
            continue;
        }
        if (c == '&') {
            unsigned long cp;
            long l = entity(s + i, n - i, &cp);
            if (l) {
                char u[4];
                item *a;
                int ul = vw_put_utf8(u, cp);
                text_item(m, s, t, i);
                /* the character lives in the urls buffer, which stays put
                 * while the items are drawn */
                {
                    long at = m->urls.n;
                    badd(m, &m->urls, u, ul);
                    badd(m, &m->urls, "", 1);
                    a = m->oom ? 0 : add_item(m, I_TEXT, 0, ul);
                    if (a) {
                        a->src = at;
                        a->kind = I_TEXT;
                        a->s = 0;
                    }
                }
                i += l;
                t = i;
                continue;
            }
        }
        if (c == '\n') {
            long e = i;
            while (e > t && s[e - 1] == ' ')
                e--;
            text_item(m, s, t, e);
            add_item(m, i - e >= 2 ? I_HARD : I_SOFT, 0, 0);
            i++;
            while (i < n && s[i] == ' ')
                i++;
            t = i;
            continue;
        }
        if ((c == 'h' || c == 'w') && (i == 0 || is_ws((unsigned char)s[i - 1]) || (s[i - 1] && strchr("(*_~\"'", s[i - 1]))) &&
            ((n - i > 8 && !memcmp(s + i, "https://", 8)) || (n - i > 7 && !memcmp(s + i, "http://", 7)) ||
             (n - i > 4 && !memcmp(s + i, "www.", 4)))) {
            long e = bare_url_end(s, n, i);
            long pre = s[i] == 'w' ? 4 : s[i + 4] == 's' ? 8 : 7;
            if (e > i + pre && (s[i] != 'w' || memchr(s + i + 4, '.', e - i - 4))) {
                item *a;
                long link;
                text_item(m, s, t, i);
                if (s[i] == 'w') {
                    long u0 = m->urls.n;
                    badd(m, &m->urls, "http://", 7);
                    badd(m, &m->urls, s + i, e - i);
                    badd(m, &m->urls, "", 1);
                    link = m->oom ? 0 : u0 + 1;
                } else {
                    link = keep_url(m, s + i, e - i, 0);
                }
                a = add_item(m, I_TEXT, s + i, e - i);
                if (a)
                    a->link = link, a->cls = HL_LINK;
                i = e;
                t = i;
                continue;
            }
        }
        i++;
    }
    text_item(m, s, t, n);
    process_emphasis(m, 0, m->nit);
    for (k = 0; k < m->nit; k++) {
        item *a = &m->it[k];
        int ac = a->cls >= 0 ? a->cls : cls;
        unsigned long w = WORD(ac, a->attr, a->link);
        switch (a->kind) {
        case I_TEXT:
        case I_BOPEN:
            if (!a->s)
                ftext(m, m->urls.p + a->src, a->n, w);
            else
                ftext(m, a->s, a->n, w);
            break;
        case I_DELIM:
            if (a->count > 0)
                ftext(m, a->s, a->count, w);
            break;
        case I_CODE: {
            long j;
            w = WORD(MD_CODESPAN, a->attr, a->link);
            for (j = 0; j < a->n; j++)
                fput(m, a->s[j] == '\n' ? " " : a->s + j, 1, w);
            break;
        }
        case I_SOFT:
            ftext(m, " ", 1, w);
            break;
        case I_HARD:
            while (m->flat.n && m->flat.p[m->flat.n - 1] == ' ')
                m->flat.n--;
            fput(m, "\n", 1, w);
            break;
        case I_LINKEND:
            if (a->show) {
                const char *u = m->urls.p + a->link - 1;
                fput(m, " ", 1, WORD(cls, 0, 0));
                fput(m, u, (long)strlen(u), WORD(MD_URL, 0, a->link));
            }
            break;
        default:
            break;
        }
    }
}

/* ---- blocks -------------------------------------------------------------- */

static int visible(md *m)
{
    long i;
    for (i = 0; i < m->flat.n; i++)
        if (m->flat.p[i] != ' ' && m->flat.p[i] != '\n')
            return 1;
    return 0;
}

static void lines_out(md *m, int indent_cols)
{
    long k;
    for (k = 0; k < m->nln; k++) {
        prefix(m, 0);
        vo_spaces(m->out, indent_cols);
        emit(m, m->ln[k].a, m->ln[k].b);
        endline(m);
    }
}

static void render_para(md *m, const char *s, long n, int html)
{
    cont *c = m->nc ? &m->c[m->nc - 1] : 0;
    m->flat.n = 0;
    m->urls.n = 0;
    if (!html && c && c->type == C_ITEM && c->marker && n >= 4 && s[0] == '[' &&
        (s[1] == ' ' || s[1] == 'x' || s[1] == 'X') && s[2] == ']' && is_sp((unsigned char)s[3])) {
        c->task = s[1] == ' ' ? 1 : 2;
        s += 4;
        n -= 4;
    }
    inlines(m, s, n, HL_PLAIN);
    if (!visible(m))
        return;
    gap(m);
    wrap(m, 0, m->flat.n, avail(m));
    lines_out(m, 0);
}

static void render_heading(md *m, int level, const char *s, long n)
{
    int cls = MD_H1 + level - 1, w, k;
    m->flat.n = 0;
    m->urls.n = 0;
    if (m->out->depth <= 0 && level >= 3) {
        /* without colours the level shows as the source wrote it */
        fput(m, "######", level, WORD(cls, 0, 0));
        fput(m, " ", 1, WORD(cls, 0, 0));
    }
    inlines(m, s, n, cls);
    gap(m);
    w = avail(m);
    wrap(m, 0, m->flat.n, w);
    lines_out(m, 0);
    if (level <= 2) {
        hl_sty st = m->out->theme->s[cls];
        st.attr = 0;
        prefix(m, 0);
        vo_style(m->out, &st);
        for (k = 0; k < w; k++)
            vo_rawz(m->out, glyph(m, level == 1 ? G_HH : G_H));
        endline(m);
    }
}

static void render_rule(md *m)
{
    int w, k;
    gap(m);
    w = avail(m);
    prefix(m, 0);
    vo_class(m->out, MD_RULE);
    for (k = 0; k < w; k++)
        vo_rawz(m->out, glyph(m, G_H));
    endline(m);
}

static void code_token(void *u, int cls, const char *s, long n)
{
    md *m = (md *)u;
    fput(m, s, n, WORD(cls == HL_PLAIN ? MD_CODEBLOCK : cls, 0, 0));
}

static void render_code(md *m)
{
    const char *p = m->text.p;
    long n = m->text.n, a = 0;
    const hl_lang *L = m->lang[0] ? hl_find(m->lang, (long)strlen(m->lang)) : 0;
    hl_state st;
    int w;
    /* no trailing blank lines */
    while (n > 0 && p[n - 1] == '\n') {
        long b = n - 1;
        while (b > 0 && p[b - 1] != '\n')
            b--;
        if (!is_blank(p + b, n - 1 - b))
            break;
        n = b;
    }
    gap(m);
    w = avail(m) - 2;
    if (w < 4)
        w = 4;
    hl_begin(&st, L);
    while (a < n) {
        long e = a, k = 0;
        while (e < n && p[e] != '\n')
            e++;
        m->flat.n = 0;
        hl_line(&st, p + a, e - a, code_token, m);
        do {
            long b = k + vw_fit(m->flat.p + k, m->flat.n - k, w);
            if (b == k && k < m->flat.n) {
                unsigned long cp;
                b = k + vw_char(m->flat.p + k, m->flat.n - k, &cp);
            }
            prefix(m, 0);
            if (b > k)
                vo_spaces(m->out, 2);
            emit(m, k, b);
            endline(m);
            k = b;
        } while (k < m->flat.n);
        a = e + 1;
    }
}

/* a table row's cells: up to max (start, end) pairs, trimmed */
static int split_row(const char *s, long n, lspan *cells, int max)
{
    long i = 0, st, e;
    int k = 0, code = 0;
    while (i < n && is_sp((unsigned char)s[i]))
        i++;
    while (n > i && is_sp((unsigned char)s[n - 1]))
        n--;
    if (i < n && s[i] == '|')
        i++;
    if (n > i && s[n - 1] == '|' && !(n >= 2 && s[n - 2] == '\\'))
        n--;
    st = i;
    for (; i <= n; i++) {
        if (i < n && s[i] == '\\') {
            i++;
            continue;
        }
        if (i < n && s[i] == '`')
            code = !code;
        if (i == n || (s[i] == '|' && !code)) {
            long a = st;
            e = i;
            while (a < e && is_sp((unsigned char)s[a]))
                a++;
            while (e > a && is_sp((unsigned char)s[e - 1]))
                e--;
            if (k < max) {
                cells[k].a = a;
                cells[k].b = e;
                k++;
            }
            st = i + 1;
        }
    }
    return k;
}

/* the |---|:--:| row: the columns and their alignment, 0 when not one */
static int delim_row(const char *s, long n, int *align)
{
    lspan c[TCOLS];
    int k = split_row(s, n, c, TCOLS), j;
    if (!memchr(s, '|', n) && k < 2)
        return 0;
    for (j = 0; j < k; j++) {
        long a = c[j].a, b = c[j].b, d;
        int l = 0, r = 0;
        if (a < b && s[a] == ':') {
            l = 1;
            a++;
        }
        if (b > a && s[b - 1] == ':') {
            r = 1;
            b--;
        }
        if (b <= a)
            return 0;
        for (d = a; d < b; d++)
            if (s[d] != '-')
                return 0;
        align[j] = l && r ? A_CENTER : r ? A_RIGHT : A_LEFT;
    }
    return k;
}

void md_fit_columns(const int *natural, int ncols, int room, int *width)
{
    int k, sum = 0, lo = 1, hi = 1, cap, left;
    for (k = 0; k < ncols; k++) {
        width[k] = natural[k] < 1 ? 1 : natural[k];
        sum += width[k];
        if (width[k] > hi)
            hi = width[k];
    }
    if (sum <= room)
        return;
    if (room < ncols)
        room = ncols;
    /* the largest cap with sum(min(natural, cap)) <= room */
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2, s = 0;
        for (k = 0; k < ncols; k++)
            s += width[k] < mid ? width[k] : mid;
        if (s <= room)
            lo = mid;
        else
            hi = mid - 1;
    }
    cap = lo;
    left = room;
    for (k = 0; k < ncols; k++) {
        if (width[k] > cap)
            width[k] = cap;
        left -= width[k];
    }
    for (k = 0; k < ncols && left > 0; k++)
        if ((natural[k] < 1 ? 1 : natural[k]) > width[k]) {
            width[k]++;
            left--;
        }
}

static void border(md *m, int *w, int nc, int l, int mid, int r)
{
    int k, j;
    prefix(m, 0);
    vo_class(m->out, MD_TABLE);
    vo_rawz(m->out, glyph(m, l));
    for (k = 0; k < nc; k++) {
        for (j = 0; j < w[k] + 2; j++)
            vo_rawz(m->out, glyph(m, G_H));
        vo_rawz(m->out, glyph(m, k + 1 < nc ? mid : r));
    }
    endline(m);
}

static void render_table(md *m)
{
    const char *p = m->text.p;
    long n = m->text.n, a = 0, nrows = 0, r;
    int nc = m->tcols, k, nat[TCOLS], w[TCOLS], room;
    lspan *cells = 0, src[TCOLS];
    long ccap = 0;
    /* every cell rendered into the flat run, one after the other */
    m->flat.n = 0;
    m->urls.n = 0;
    while (a < n) {
        long e = a;
        int got;
        while (e < n && p[e] != '\n')
            e++;
        got = split_row(p + a, e - a, src, TCOLS);
        if (!grow(m, (void **)&cells, &ccap, (nrows + 1) * nc, sizeof(lspan)))
            break;
        for (k = 0; k < nc; k++) {
            long f0 = m->flat.n;
            if (k < got) {
                /* inlines() starts its own items; the flat run goes on */
                inlines(m, p + a + src[k].a, src[k].b - src[k].a, nrows == 0 ? MD_TH : HL_PLAIN);
            }
            while (m->flat.n > f0 && m->flat.p[m->flat.n - 1] == ' ')
                m->flat.n--;
            cells[nrows * nc + k].a = f0;
            cells[nrows * nc + k].b = m->flat.n;
            /* a separator, so the next cell's first blank folds away */
            fput(m, "\n", 1, 0);
        }
        nrows++;
        a = e + 1;
    }
    if (m->oom || !nrows) {
        free(cells);
        return;
    }
    for (k = 0; k < nc; k++) {
        nat[k] = 1;
        for (r = 0; r < nrows; r++) {
            lspan c = cells[r * nc + k];
            int cw = vw_width(m->flat.p + c.a, c.b - c.a);
            if (cw > nat[k])
                nat[k] = cw;
        }
    }
    room = m->o->width - prefix_width(m) - (3 * nc + 1);
    md_fit_columns(nat, nc, room, w);
    gap(m);
    border(m, w, nc, G_TL, G_TM, G_TR);
    for (r = 0; r < nrows; r++) {
        lspan *ls = 0;
        long lcap = 0, cnt[TCOLS], off[TCOLS], total = 0, h = 0, line;
        for (k = 0; k < nc; k++) {
            lspan c = cells[r * nc + k];
            long j;
            wrap(m, c.a, c.b, w[k]);
            if (!grow(m, (void **)&ls, &lcap, total + m->nln, sizeof(lspan)))
                break;
            off[k] = total;
            cnt[k] = m->nln;
            for (j = 0; j < m->nln; j++)
                ls[total + j] = m->ln[j];
            total += m->nln;
            if (m->nln > h)
                h = m->nln;
        }
        if (m->oom) {
            free(ls);
            break;
        }
        for (line = 0; line < h; line++) {
            prefix(m, 0);
            vo_class(m->out, MD_TABLE);
            vo_rawz(m->out, glyph(m, G_V));
            for (k = 0; k < nc; k++) {
                int cw = 0, pad, lp;
                lspan c;
                c.a = c.b = 0;
                if (line < cnt[k] && (c = ls[off[k] + line], c.b > c.a))
                    cw = vw_width(m->flat.p + c.a, c.b - c.a);
                pad = w[k] - cw;
                if (pad < 0)
                    pad = 0;
                lp = m->talign[k] == A_RIGHT ? pad : m->talign[k] == A_CENTER ? pad / 2 : 0;
                vo_reset(m->out);
                vo_spaces(m->out, 1 + lp);
                if (cw)
                    emit(m, c.a, c.b);
                vo_link(m->out, 0);
                vo_reset(m->out);
                vo_spaces(m->out, pad - lp + 1);
                vo_class(m->out, MD_TABLE);
                vo_rawz(m->out, glyph(m, G_V));
            }
            endline(m);
        }
        free(ls);
        if (r == 0)
            border(m, w, nc, G_ML, G_MM, G_MR);
    }
    border(m, w, nc, G_BL, G_BM, G_BR);
    free(cells);
}

static void close_leaf(md *m)
{
    int leaf = m->leaf;
    m->leaf = L_NONE;
    switch (leaf) {
    case L_PARA:
    case L_HTML:
        if (m->text.n)
            render_para(m, m->text.p, m->text.n - 1, leaf == L_HTML);
        break;
    case L_FENCE:
    case L_ICODE:
        render_code(m);
        break;
    case L_TABLE:
        render_table(m);
        break;
    default:
        break;
    }
    m->text.n = 0;
    m->nlines = 0;
}

static void text_line(md *m, const char *s, long n)
{
    badd(m, &m->text, s, n);
    badd(m, &m->text, "\n", 1);
    m->nlines++;
}

static void push(md *m, int type)
{
    cont *c;
    if (m->nc >= MAXC)
        return;
    c = &m->c[m->nc++];
    memset(c, 0, sizeof(*c));
    c->type = type;
}

static void pop(md *m)
{
    cont *c = &m->c[m->nc - 1];
    if (c->type == C_ITEM) {
        int d = items_open(m) - 1;
        if (c->marker) {
            /* an empty item still shows its marker */
            gap(m);
            prefix(m, 0);
            endline(m);
        }
        if (d >= 0 && d < MAXC) {
            m->last[d].valid = 1;
            m->last[d].ordered = c->ordered;
            m->last[d].bullet = c->bullet;
            m->last[d].num = c->num;
        }
    }
    m->nc--;
    if (m->kept > m->nc)
        m->kept = m->nc;
}

static void open_item(md *m, int ordered, long num, char bullet, int indent)
{
    int d = items_open(m);
    listmem *l = d < MAXC ? &m->last[d] : 0;
    cont *c;
    int sibling = l && l->valid && l->ordered == ordered && l->bullet == bullet;
    push(m, C_ITEM);
    c = &m->c[m->nc - 1];
    c->ordered = ordered;
    c->bullet = bullet;
    c->num = sibling && ordered ? l->num + 1 : num;
    c->indent = indent;
    c->marker = 1;
    c->level = d;
    if (!sibling && d == 0)
        m->force_blank = 1;
    if (l)
        l->valid = 0;
}

static void leaf_line(md *m, const char *s, long n)
{
    long ind = indent(s, n), p, r;
    int c;
    if (m->leaf == L_FENCE) {
        if (ind < 4 && ind < n && s[ind] == (char)m->fchar && (r = run(s + ind, n - ind, m->fchar)) >= m->flen &&
            is_blank(s + ind + r, n - ind - r)) {
            close_leaf(m);
            return;
        }
        p = ind < m->findent ? ind : m->findent;
        text_line(m, s + p, n - p);
        return;
    }
    if (is_blank(s, n)) {
        if (m->leaf == L_ICODE) {
            text_line(m, "", 0);
            return;
        }
        close_leaf(m);
        m->blank = 1;
        return;
    }
    if (m->leaf == L_HTML) {
        text_line(m, s, n);
        return;
    }
    if (m->leaf == L_ICODE) {
        if (ind >= 4) {
            text_line(m, s + 4, n - 4);
            return;
        }
        close_leaf(m);
    }
    if (ind >= 4) {
        if (m->leaf == L_PARA) {
            text_line(m, s + ind, n - ind);
        } else {
            close_leaf(m);
            m->leaf = L_ICODE;
            text_line(m, s + 4, n - 4);
        }
        return;
    }
    p = ind;
    c = (unsigned char)s[p];
    if ((c == '`' || c == '~') && (r = run(s + p, n - p, c)) >= 3 && !(c == '`' && memchr(s + p + r, '`', n - p - r))) {
        long a = p + r, k = 0;
        close_leaf(m);
        m->leaf = L_FENCE;
        m->fchar = c;
        m->flen = (int)r;
        m->findent = (int)ind;
        while (a < n && is_sp((unsigned char)s[a]))
            a++;
        while (a < n && !is_sp((unsigned char)s[a]) && s[a] != '{' && s[a] != ',' && k < (long)sizeof(m->lang) - 1)
            m->lang[k++] = (char)lower((unsigned char)s[a++]);
        m->lang[k] = 0;
        return;
    }
    if (c == '#' && (r = run(s + p, n - p, '#')) <= 6 && (p + r == n || is_sp((unsigned char)s[p + r]))) {
        long a = p + r, e = n;
        close_leaf(m);
        while (a < e && is_sp((unsigned char)s[a]))
            a++;
        while (e > a && is_sp((unsigned char)s[e - 1]))
            e--;
        {
            long h = e;
            while (h > a && s[h - 1] == '#')
                h--;
            if (h == a || is_sp((unsigned char)s[h - 1])) {
                e = h;
                while (e > a && is_sp((unsigned char)s[e - 1]))
                    e--;
            }
        }
        render_heading(m, (int)r, s + a, e - a);
        return;
    }
    if (m->leaf == L_PARA && (c == '=' || c == '-')) {
        long k = p;
        while (k < n && s[k] == (char)c)
            k++;
        if (is_blank(s + k, n - k)) {
            m->leaf = L_NONE;
            render_heading(m, c == '=' ? 1 : 2, m->text.p, m->text.n - 1);
            m->text.n = 0;
            m->nlines = 0;
            return;
        }
    }
    if (is_hr(s + p, n - p)) {
        close_leaf(m);
        render_rule(m);
        return;
    }
    if (m->leaf == L_PARA && m->nlines == 1) {
        int al[TCOLS], k = delim_row(s + p, n - p, al);
        lspan hc[TCOLS];
        if (k > 0 && k == split_row(m->text.p, m->text.n - 1, hc, TCOLS) &&
            (memchr(s, '|', n) || memchr(m->text.p, '|', m->text.n))) {
            m->leaf = L_TABLE;
            m->tcols = k;
            memcpy(m->talign, al, sizeof(al));
            return;
        }
    }
    if (m->leaf == L_TABLE) {
        if (memchr(s, '|', n)) {
            text_line(m, s, n);
            return;
        }
        close_leaf(m);
    }
    if (m->leaf != L_PARA && c == '<' && p + 1 < n &&
        (is_alpha((unsigned char)s[p + 1]) || s[p + 1] == '/' || s[p + 1] == '!')) {
        close_leaf(m);
        m->leaf = L_HTML;
        text_line(m, s + p, n - p);
        return;
    }
    if (m->leaf != L_PARA) {
        close_leaf(m);
        m->leaf = L_PARA;
    }
    text_line(m, s + p, n - p);
}

static void process_line(md *m, const char *s, long n)
{
    long pos = 0;
    int matched = 0, k, opened = 0;
    for (k = 0; k < m->nc; k++) {
        cont *c = &m->c[k];
        if (c->type == C_QUOTE) {
            long p = pos + indent(s + pos, n - pos);
            if (p - pos < 4 && p < n && s[p] == '>') {
                pos = p + 1;
                if (pos < n && s[pos] == ' ')
                    pos++;
                matched++;
                continue;
            }
            break;
        }
        if (is_blank(s + pos, n - pos)) {
            matched++;
            continue;
        }
        if (indent(s + pos, n - pos) >= c->indent) {
            pos += c->indent;
            matched++;
            continue;
        }
        break;
    }
    if (matched < m->nc) {
        if (m->leaf == L_PARA && !is_blank(s + pos, n - pos) && !starts_block(s + pos, n - pos)) {
            text_line(m, s + pos + indent(s + pos, n - pos), n - pos - indent(s + pos, n - pos));
            return;
        }
        close_leaf(m);
        while (m->nc > matched)
            pop(m);
    }
    while (m->leaf != L_FENCE) {
        long ind = indent(s + pos, n - pos), p = pos + ind;
        int mk, ord;
        long num;
        char bul;
        if (ind >= 4 || p >= n)
            break;
        if (s[p] == '>') {
            close_leaf(m);
            push(m, C_QUOTE);
            opened = 1;
            pos = p + 1;
            if (pos < n && s[pos] == ' ')
                pos++;
            continue;
        }
        mk = list_marker(s + p, n - p, &ord, &num, &bul);
        if (mk && !is_hr(s + p, n - p)) {
            long after = p + mk, sp = 0;
            int empty;
            while (after + sp < n && s[after + sp] == ' ')
                sp++;
            empty = after + sp >= n;
            if (m->leaf == L_PARA && (empty || (ord && num != 1)))
                break;
            if (empty || sp > 4)
                sp = 1;
            close_leaf(m);
            open_item(m, ord, num, bul, (int)(p - pos + mk + sp));
            opened = 1;
            pos = after + sp;
            if (pos > n)
                pos = n;
            continue;
        }
        break;
    }
    if (opened && is_blank(s + pos, n - pos))
        return; /* "-" or ">" alone: the container, no blank line */
    leaf_line(m, s + pos, n - pos);
}

/* ---- reference definitions: collected before anything is drawn ---------- */

static int ref_def(md *m, const char *s, long n)
{
    long p = indent(s, n), q, a, b;
    char l[128];
    int i;
    if (p >= 4 || p + 1 >= n || s[p] != '[' || s[p + 1] == '^')
        return 0;
    q = p + 1;
    while (q < n && s[q] != ']' && s[q] != '[')
        q++;
    if (q >= n || s[q] != ']' || q + 1 >= n || s[q + 1] != ':' || q == p + 1)
        return 0;
    a = q + 2;
    while (a < n && is_sp((unsigned char)s[a]))
        a++;
    if (a >= n)
        return 0;
    if (s[a] == '<') {
        b = ++a;
        while (b < n && s[b] != '>')
            b++;
        if (b >= n)
            return 0;
    } else {
        b = a;
        while (b < n && !is_sp((unsigned char)s[b]))
            b++;
    }
    {
        long t = b + (b < n && s[b] == '>');
        while (t < n && is_sp((unsigned char)s[t]))
            t++;
        if (t < n && s[t] != '"' && s[t] != '\'' && s[t] != '(')
            return 0;
    }
    norm_label(s + p + 1, q - p - 1, l, sizeof(l));
    for (i = 0; i < m->nrefs; i++)
        if (!strcmp(m->refs[i].label, l))
            return 1; /* the first definition wins */
    if (m->nrefs >= m->crefs) {
        int nc = m->crefs ? m->crefs * 2 : 16;
        ref *nr = (ref *)realloc(m->refs, sizeof(ref) * nc);
        if (!nr) {
            m->oom = 1;
            return 1;
        }
        m->refs = nr;
        m->crefs = nc;
    }
    m->refs[m->nrefs].label = (char *)malloc(strlen(l) + 1);
    m->refs[m->nrefs].url = (char *)malloc((size_t)(b - a + 1));
    if (!m->refs[m->nrefs].label || !m->refs[m->nrefs].url) {
        free(m->refs[m->nrefs].label);
        free(m->refs[m->nrefs].url);
        m->oom = 1;
        return 1;
    }
    strcpy(m->refs[m->nrefs].label, l);
    memcpy(m->refs[m->nrefs].url, s + a, b - a);
    m->refs[m->nrefs].url[b - a] = 0;
    m->nrefs++;
    return 1;
}

/* the line from doc at *pos, tabs expanded to 4 columns, into m->line */
static int next_line(md *m, const char *doc, long n, long *pos)
{
    long i = *pos, col = 0, e;
    const char *nl;
    char *d;
    if (i >= n)
        return 0;
    nl = (const char *)memchr(doc + i, '\n', n - i);
    e = nl ? (long)(nl - doc) : n;
    m->line.n = 0;
    /* room for every byte a tab (4 blanks) */
    if (!grow(m, (void **)&m->line.p, &m->line.cap, (e - i) * 4 + 1, 1)) {
        *pos = e + 1;
        return 1;
    }
    d = m->line.p;
    for (; i < e; i++) {
        if (doc[i] == '\t') {
            int k = 4 - (int)(col % 4);
            col += k;
            while (k--)
                *d++ = ' ';
        } else if (doc[i] != '\r') {
            *d++ = doc[i];
            col++;
        }
    }
    *d = 0;
    m->line.n = (long)(d - m->line.p);
    *pos = e + 1;
    return 1;
}

int md_render(const char *doc, long n, const md_opts *opt, vw_out *out)
{
    md *m = (md *)calloc(1, sizeof(md));
    long pos = 0, idx = 0, nl = 0;
    char *defs = 0;
    int i, fence = 0, fc = 0, prev_text = 0, rc;
    if (!m)
        return -1;
    m->o = opt;
    m->out = out;
    m->utf8 = out->cs == VW_UTF8;
    /* pass 1: reference definitions (not in fences, not inside a paragraph) */
    {
        long k;
        for (k = 0; k < n; k++)
            nl += doc[k] == '\n';
        defs = (char *)calloc((size_t)nl + 2, 1);
    }
    if (!defs) {
        free(m);
        return -1;
    }
    while (next_line(m, doc, n, &pos)) {
        const char *s = m->line.p;
        long ln = m->line.n, p = indent(s, ln);
        if (p < 4 && p < ln && (s[p] == '`' || s[p] == '~') && run(s + p, ln - p, s[p]) >= 3) {
            if (!fence) {
                fence = 1;
                fc = s[p];
            } else if (s[p] == fc) {
                fence = 0;
            }
            prev_text = 0;
        } else if (!fence) {
            if (!prev_text && ref_def(m, s, ln)) {
                defs[idx] = 1;
            } else {
                /* a heading or an HTML line ("<!-- Links -->") is no paragraph */
                prev_text = !is_blank(s, ln) && !(p < ln && (s[p] == '#' || s[p] == '<'));
            }
        }
        idx++;
    }
    /* pass 2: the blocks */
    pos = 0;
    idx = 0;
    while (next_line(m, doc, n, &pos)) {
        if (!defs[idx++])
            process_line(m, m->line.p, m->line.n);
    }
    close_leaf(m);
    while (m->nc > 0)
        pop(m);
    vo_link(out, 0);
    vo_reset(out);
    rc = m->oom ? -1 : 0;
    for (i = 0; i < m->nrefs; i++) {
        free(m->refs[i].label);
        free(m->refs[i].url);
    }
    free(m->refs);
    free(m->it);
    free(m->stk);
    free(m->flat.p);
    free(m->fst);
    free(m->urls.p);
    free(m->ln);
    free(m->line.p);
    free(m->text.p);
    free(defs);
    free(m);
    return rc;
}
