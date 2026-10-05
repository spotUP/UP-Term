/* show -- see show.h. */
#include <stdlib.h>
#include <string.h>
#include "show.h"
#include "tools.h"
#include "util.h"

#define G_BULLET "\342\217\272"     /* U+23FA */
#define G_CORNER "\342\216\277"     /* U+23BF, drawn as the light corner */
#define G_STAR   "\342\234\273"     /* U+273B */
#define G_CHECK  "\342\234\224"     /* U+2714 */
#define G_SQUARE "\342\226\240"     /* U+25A0 */
#define G_CIRCLE "\342\227\213"     /* U+25CB */
#define G_TL "\342\225\255"
#define G_TR "\342\225\256"
#define G_BL "\342\225\260"
#define G_BR "\342\225\257"
#define G_H  "\342\224\200"
#define G_V  "\342\224\202"

#define SGR0  "\033[0m"
#define DIM   "\033[2m"
#define BOLD  "\033[1m"
#define TH    (s->t->th)           /* the colours (theme.h) */

const char *show_name(int tool)
{
    return tools_title(tool);
}

/* ---- output helpers ---- */

static void think_flush(cl_show *s);

static void out(cl_show *s)
{
    if (s->batch.n) {
        tui_lines(s->t, s->batch.p, s->batch.n);
        jw_reset(&s->batch);
    }
}

/* the batch to the screen only (a folded result: the viewer gets it whole) */
static void out_screen(cl_show *s)
{
    s->t->nolog = 1;
    out(s);
    s->t->nolog = 0;
}

/* the batch to the transcript viewer only */
static void out_log(cl_show *s)
{
    if (s->batch.n)
        tui_log(s->t, s->batch.p, s->batch.n);
    jw_reset(&s->batch);
}

static void raw(cl_show *s, const char *z)
{
    jw_rawz(&s->batch, z);
}

/* text for the screen: controls as '?', tabs as spaces to 4, cut at max
 * columns (max < 0: no cut); *w the columns used */
static long text(jw *o, const char *p, long n, int max, int *w)
{
    long i = 0;
    while (i < n) {
        unsigned long cp;
        int l = vw_char(p + i, n - i, &cp), cw;
        char u[8];
        if (cp == '\t') {
            int k = 4 - (*w % 4);
            if (max >= 0 && *w + k > max)
                break;
            while (k--) {
                jw_raw(o, " ", 1);
                (*w)++;
            }
            i += l;
            continue;
        }
        if (cp < 0x20 || cp == 0x7f || (cp >= 0x80 && cp < 0xa0))
            cp = '?';
        cw = vw_cp_width(cp);
        if (max >= 0 && *w + cw > max)
            break;
        jw_raw(o, u, vw_put_utf8(u, cp));
        *w += cw;
        i += l;
    }
    return i;
}

/* text word-wrapped into lines of width columns, each prefixed (first
 * line: first, the others: rest) */
static void wrapped(cl_show *s, const char *first, const char *rest, const char *style, const char *p, long n,
                    int width)
{
    long a = 0;
    int line = 0;
    if (width < 8)
        width = 8;
    while (n > 0 && p[n - 1] == '\n')
        n--;
    while (a <= n) {
        long e = a, brk = -1, i;
        int w = 0;
        /* one source line */
        while (e < n && p[e] != '\n')
            e++;
        if (a == e) {
            raw(s, line++ ? rest : first);
            raw(s, "\n");
        }
        for (i = a; i < e;) {
            long k = i;
            int cw = 0;
            brk = -1;
            w = 0;
            while (k < e) {
                unsigned long cp;
                int l = vw_char(p + k, e - k, &cp), x = cp == '\t' ? 4 : vw_cp_width(cp);
                if (w + x > width)
                    break;
                if (cp == ' ')
                    brk = k;
                w += x;
                k += l;
            }
            if (k < e && brk > i)
                k = brk;
            if (k == i) {
                unsigned long cp;
                k = i + vw_char(p + i, e - i, &cp);
            }
            raw(s, line++ ? rest : first);
            raw(s, style);
            text(&s->batch, p + i, k - i, -1, &cw);
            raw(s, SGR0 "\n");
            i = k;
            while (i < e && p[i] == ' ')
                i++;
        }
        if (e >= n)
            break;
        a = e + 1;
    }
}

/* ---- the answer ---- */

static void md_sink(void *u, const char *p, long n)
{
    cl_show *s = (cl_show *)u;
    long a = 0, i;
    for (i = 0; i < n; i++) {
        if (p[i] != '\n')
            continue;
        jw_raw(&s->line, p + a, i - a);
        if (s->first) {
            /* the bullet goes on the answer's first line that has text */
            if (!tui_width(s->line.p ? s->line.p : "", s->line.n)) {
                jw_reset(&s->line);
                a = i + 1;
                continue;
            }
            raw(s, "\n" SGR0 G_BULLET " ");
            s->first = 0;
            s->n_blocks++;
        } else {
            raw(s, "  ");
        }
        jw_raw(&s->batch, s->line.p ? s->line.p : "", s->line.n);
        raw(s, SGR0 "\n");
        jw_reset(&s->line);
        a = i + 1;
    }
    jw_raw(&s->line, p + a, n - a);
}

static void md_start(cl_show *s)
{
    md_opts mo;
    if (s->md)
        return;
    hl_theme_builtin(&s->th, TH->hl);
    vo_init(&s->vo, md_sink, s, VW_UTF8, 16, &s->th);
    mo.width = s->t->cols - 3;
    mo.urls = 1;
    s->md = md_open(&mo, &s->vo);
    s->first = 1;
    jw_reset(&s->line);
}

static void r_text(void *u, const char *p, long n)
{
    cl_show *s = (cl_show *)u;
    s->n_text++;
    think_flush(s);
    md_start(s);
    if (s->md)
        md_feed(s->md, p, n);
    vo_flush(&s->vo);
    out(s);
}

static void r_end(void *u)
{
    cl_show *s = (cl_show *)u;
    think_flush(s);
    if (!s->md)
        return;
    md_close(s->md);
    s->md = 0;
    vo_flush(&s->vo);
    if (s->line.n)
        md_sink(s, "\n", 1);
    out(s);
}

void show_render(cl_show *s, cl_render *r)
{
    r->u = s;
    r->text = r_text;
    r->end = r_end;
}

void show_init(cl_show *s, cl_tui *t)
{
    memset(s, 0, sizeof(*s));
    s->t = t;
    hl_theme_builtin(&s->th, "ansi");
    jw_init(&s->line);
    jw_init(&s->batch);
    jw_init(&s->head);
    jw_init(&s->think);
    s->tool = -1;
}

void show_free(cl_show *s)
{
    if (s->md)
        md_close(s->md);
    s->md = 0;
    jw_free(&s->line);
    jw_free(&s->batch);
    jw_free(&s->head);
    jw_free(&s->think);
}

/* ---- the program's own lines ---- */

void show_welcome(cl_show *s, const char *model, const char *root)
{
    int w = s->t->cols - 1 < 58 ? s->t->cols - 1 : 58, i, k;
    const char *rows[5];
    char cwd[300], mod[120];
    cl_copy(cwd, "  cwd: ", sizeof(cwd));
    cl_cat(cwd, root && *root ? root : "(the current directory)", sizeof(cwd));
    cl_copy(mod, "  model: ", sizeof(mod));
    cl_cat(mod, model, sizeof(mod));
    rows[0] = 0;
    rows[1] = "";
    rows[2] = "  /help for help, Shift+Tab for the permission mode";
    rows[3] = cwd;
    rows[4] = mod;
    raw(s, SGR0);
    raw(s, TH->accent);
    raw(s, G_TL);
    for (i = 2; i < w; i++)
        raw(s, G_H);
    raw(s, G_TR SGR0 "\n");
    for (k = 0; k < 5; k++) {
        int cw = 0;
        raw(s, TH->accent);
        raw(s, G_V SGR0 " ");
        if (!rows[k]) {
            raw(s, TH->accent);
            raw(s, G_STAR SGR0 " Welcome to " BOLD "Claude" SGR0 " on the Amiga!");
            /* the star is one column; the words' widths counted, not guessed */
            cw = 1 + (int)(sizeof(" Welcome to ") - 1 + sizeof("Claude") - 1 + sizeof(" on the Amiga!") - 1);
        } else {
            text(&s->batch, rows[k], (long)strlen(rows[k]), w - 4, &cw);
        }
        while (cw < w - 3) {
            raw(s, " ");
            cw++;
        }
        raw(s, TH->accent);
        raw(s, G_V SGR0 "\n");
    }
    raw(s, TH->accent);
    raw(s, G_BL);
    for (i = 2; i < w; i++)
        raw(s, G_H);
    raw(s, G_BR SGR0 "\n");
    out(s);
}

void show_user(cl_show *s, const char *p)
{
    think_flush(s);
    raw(s, "\n");
    if ((p[0] == '!' || p[0] == '#') && p[1]) {
        /* a ! command or a # memory, in its mode's colour */
        char first[32];
        cl_copy(first, p[0] == '!' ? TH->bash : TH->memory, sizeof(first));
        cl_cat(first, p[0] == '!' ? "! " : "# ", sizeof(first));
        cl_cat(first, SGR0, sizeof(first));
        wrapped(s, first, "  ", "", p + 1, (long)strlen(p + 1), s->t->cols - 3);
    } else {
        wrapped(s, DIM "> " SGR0, "  ", DIM, p, (long)strlen(p), s->t->cols - 3);
    }
    out(s);
}

void show_note(cl_show *s, const char *p)
{
    think_flush(s);
    wrapped(s, "  " DIM G_CORNER SGR0 "  ", "     ", "", p, (long)strlen(p), s->t->cols - 6);
    out(s);
}

/* ---- tool calls ---- */

static char *input_str(const char *in, long n, const char *key, long *len)
{
    jv v, x;
    if (json_parse(in, n, &v) || !json_get(v, key, &x))
        return 0;
    return json_strdup(x, len);
}

void show_tool(cl_show *s, int tool, const char *in, long inn, const char *what)
{
    char *args = tools_args(tool, in, inn), *path;
    long l;
    int w = 0;
    think_flush(s);
    jw_reset(&s->head);
    s->tool = tool;
    s->head_out = 0;
    s->adds = s->dels = 0;
    s->path[0] = 0;
    s->n_tools++;
    path = input_str(in, inn, "file_path", &l);
    if (path) {
        cl_copy(s->path, path, sizeof(s->path));
        free(path);
    }
    jw_rawz(&s->head, BOLD);
    if (s->name_sgr)
        jw_rawz(&s->head, s->name_sgr);    /* a subagent's color (its frontmatter) */
    jw_rawz(&s->head, show_name(tool));
    jw_rawz(&s->head, SGR0 "(");
    if (args && *args)
        text(&s->head, args, (long)strlen(args), 60, &w);
    else if (what && *what && (tool < 0 || tool >= T_COUNT))
        text(&s->head, what, (long)strlen(what), 60, &w);
    free(args);
    jw_rawz(&s->head, ")");
}

static void head_out(cl_show *s, const char *color)
{
    if (s->head_out || s->tool < 0)
        return;
    raw(s, "\n");
    raw(s, color);
    raw(s, G_BULLET SGR0 " ");
    jw_raw(&s->batch, s->head.p ? s->head.p : "", s->head.n);
    raw(s, SGR0 "\n");
    s->head_out = 1;
}

void show_head(cl_show *s)
{
    head_out(s, "");
    out(s);
}

/* the lines of a text: starts and lengths (the last line without '\n'
 * counts, an empty text has none) */
static long *line_index(const char *p, long n, long *count)
{
    long i, k = 0, c = 0, *ix;
    for (i = 0; i < n; i++)
        c += p[i] == '\n';
    if (n && p[n - 1] != '\n')
        c++;
    ix = (long *)malloc(sizeof(long) * (size_t)(c + 1) * 2);
    if (!ix) {
        *count = 0;
        return 0;
    }
    if (n) {
        long a = 0;
        for (i = 0; i <= n; i++) {
            if (i == n ? a < n : p[i] == '\n') {
                ix[k * 2] = a;
                ix[k * 2 + 1] = i - a;
                k++;
                a = i + 1;
            }
        }
    }
    *count = k;
    return ix;
}

static int digits(long v)
{
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

static void diff_line(cl_show *s, long no, int nw, int sign, const char *p, long n)
{
    char num[16];
    int w = 0, width = s->t->cols - 5, d = digits(no);
    raw(s, "     ");
    raw(s, sign == '-' ? TH->del : sign == '+' ? TH->add : DIM);
    while (d++ < nw) {
        raw(s, " ");
        w++;
    }
    cl_ltoa(no, num);
    raw(s, num);
    w += (int)strlen(num);
    raw(s, sign == '-' ? " - " : sign == '+' ? " + " : "   ");
    w += 3;
    if (sign == ' ')
        raw(s, SGR0);
    text(&s->batch, p, n, width, &w);
    if (sign != ' ')
        while (w < width) {
            raw(s, " ");
            w++;
        }
    raw(s, SGR0 "\n");
}

void show_preview(cl_show *s, int tool, const char *path, const char *before, long bn, const char *after,
                  long an)
{
    long nb, na, *bi, *ai, p = 0, q = 0, k, shown, limit;
    int nw, pass;
    (void)path;
    (void)tool;
    bi = line_index(before ? before : "", before ? bn : 0, &nb);
    ai = line_index(after, an, &na);
    if (!bi || !ai) {
        free(bi);
        free(ai);
        return;
    }
    while (p < nb && p < na && bi[p * 2 + 1] == ai[p * 2 + 1] &&
           !memcmp(before + bi[p * 2], after + ai[p * 2], (size_t)bi[p * 2 + 1]))
        p++;
    while (q < nb - p && q < na - p && bi[(nb - 1 - q) * 2 + 1] == ai[(na - 1 - q) * 2 + 1] &&
           !memcmp(before + bi[(nb - 1 - q) * 2], after + ai[(na - 1 - q) * 2], (size_t)bi[(nb - 1 - q) * 2 + 1]))
        q++;
    s->dels = (int)(nb - p - q);
    s->adds = (int)(na - p - q);
    s->n_diffs++;
    head_out(s, "");
    out(s);
    nw = digits(na > nb ? na : nb);
    /* folded for the screen, whole (to 400 lines) for the viewer */
    for (pass = 0; pass < 2; pass++) {
        limit = pass || (s->verbose && *s->verbose) ? 400 : before ? 24 : 12;
        shown = 0;
        for (k = p - 3 < 0 ? 0 : p - 3; k < p; k++)
            diff_line(s, k + 1, nw, ' ', before + bi[k * 2], bi[k * 2 + 1]);
        for (k = p; k < nb - q && shown < limit; k++, shown++)
            diff_line(s, k + 1, nw, '-', before + bi[k * 2], bi[k * 2 + 1]);
        for (k = p; k < na - q && shown < limit; k++, shown++)
            diff_line(s, k + 1, nw, '+', after + ai[k * 2], ai[k * 2 + 1]);
        if (s->dels + s->adds > shown) {
            char m[80], num[16];
            cl_copy(m, "     " DIM "... ", sizeof(m));
            cl_ltoa(s->dels + s->adds - shown, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, pass ? " more lines" SGR0 "\n" : " more lines (ctrl+o to see them)" SGR0 "\n", sizeof(m));
            raw(s, m);
        }
        for (k = 0; k < 3 && na - q + k < na; k++)
            diff_line(s, na - q + k + 1, nw, ' ', after + ai[(na - q + k) * 2], ai[(na - q + k) * 2 + 1]);
        if (pass)
            out_log(s);
        else
            out_screen(s);
    }
    free(bi);
    free(ai);
}

static long count_lines(const char *p, long n)
{
    long i, c = 0;
    for (i = 0; i < n; i++)
        c += p[i] == '\n';
    return c + (n && p[n - 1] != '\n');
}

/* "  <corner>  " and the summary */
static void summary(cl_show *s, const char *style, const char *m)
{
    wrapped(s, "  " DIM G_CORNER SGR0 "  ", "     ", style, m, (long)strlen(m), s->t->cols - 6);
}

/* the body of a result: folded to SHOW_FOLD lines on the screen, whole
 * (to 2000 lines) in the transcript viewer */
static void body(cl_show *s, const char *p, long n, int first_corner)
{
    long nl = count_lines(p, n);
    int pass;
    out(s);
    for (pass = 0; pass < 2; pass++) {
        long k = 0, a = 0, limit = pass || (s->verbose && *s->verbose) ? 2000 : SHOW_FOLD;
        while (a < n && k < limit) {
            long e = a;
            int w = 0;
            while (e < n && p[e] != '\n')
                e++;
            raw(s, first_corner && k == 0 ? "  " DIM G_CORNER SGR0 "  " : "     ");
            text(&s->batch, p + a, e - a, pass ? -1 : s->t->cols - 6, &w);
            raw(s, SGR0 "\n");
            k++;
            a = e + 1;
        }
        if (nl > k) {
            char m[96], num[16];
            cl_copy(m, "     " DIM "... +", sizeof(m));
            cl_ltoa(nl - k, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, pass ? " lines" SGR0 "\n" : " lines (ctrl+o to expand)" SGR0 "\n", sizeof(m));
            raw(s, m);
        }
        if (pass)
            out_log(s);
        else
            out_screen(s);
    }
}

static void todos(cl_show *s, const char *in, long inn)
{
    jv v, arr, e, x;
    jit it;
    int k = 0;
    raw(s, "\n");
    raw(s, TH->ok);
    raw(s, G_BULLET SGR0 " " BOLD "Update Todos" SGR0 "\n");
    tui_set_todos(s->t, in, inn);
    if (json_parse(in, inn, &v) || !json_get(v, "todos", &arr))
        return;
    json_iter(arr, &it);
    while (json_next(&it, 0, &e)) {
        long l;
        char *c = json_get(e, "content", &x) ? json_strdup(x, &l) : 0;
        int w = 0;
        raw(s, k++ ? "     " : "  " DIM G_CORNER SGR0 "  ");
        if (json_get(e, "status", &x) && json_streq(x, "completed")) {
            raw(s, TH->ok);
            raw(s, G_CHECK SGR0 " " DIM);
        } else if (json_get(e, "status", &x) && json_streq(x, "in_progress")) {
            raw(s, TH->accent);
            raw(s, G_SQUARE SGR0 " " BOLD);
        } else {
            raw(s, G_CIRCLE " ");
        }
        if (c)
            text(&s->batch, c, l, s->t->cols - 8, &w);
        raw(s, SGR0 "\n");
        free(c);
    }
}

void show_result(cl_show *s, int tool, const char *in, long inn, int is_error, const char *p, long n)
{
    char m[400], num[16];
    if (tool == T_TODO_WRITE && !is_error) {
        todos(s, in, inn);
        out(s);
        return;
    }
    head_out(s, is_error ? TH->err : TH->ok);
    if (is_error) {
        if (n >= 17 && !memcmp(p, "the user declined", 17))
            summary(s, DIM, "Declined.");
        else if (n >= 16 && !memcmp(p, "the user stopped", 16))
            summary(s, DIM, "Stopped. Tell Claude what to do instead.");
        else if (n >= 7 && !memcmp(p, "not run", 7))
            summary(s, DIM, "Not run.");
        else {
            cl_copy(m, "Error: ", sizeof(m));
            {
                long k = n < 300 ? n : 300;
                long l = (long)strlen(m);
                memcpy(m + l, p, (size_t)k);
                m[l + k] = 0;
            }
            if (tool == T_BASH || tool == T_TASK || tool == T_WEB_FETCH)
                body(s, p, n, 1);
            else
                summary(s, TH->err, m);
        }
        s->tool = -1;
        out(s);
        return;
    }
    if (s->brief && s->brief[0])
        tool = -1;                  /* the tool said what to show (WebFetch's "Received ...") */
    switch (tool) {
    case -1:
        summary(s, "", s->brief);
        break;
    case T_WRITE:
        cl_copy(m, "Wrote ", sizeof(m));
        cl_ltoa(s->adds, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, s->adds == 1 ? " line to " : " lines to ", sizeof(m));
        cl_cat(m, s->path, sizeof(m));
        summary(s, "", m);
        break;
    case T_EDIT:
    case T_MULTIEDIT:
        cl_copy(m, "Updated ", sizeof(m));
        cl_cat(m, s->path, sizeof(m));
        cl_cat(m, " with ", sizeof(m));
        cl_ltoa(s->adds, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, s->adds == 1 ? " addition and " : " additions and ", sizeof(m));
        cl_ltoa(s->dels, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, s->dels == 1 ? " removal" : " removals", sizeof(m));
        summary(s, "", m);
        break;
    case T_BASH: {
        /* "Return code N.\n" and the output */
        const char *o = (const char *)memchr(p, '\n', (size_t)n);
        long on = o ? n - (o + 1 - p) : 0;
        long rc = n > 12 && !memcmp(p, "Return code ", 12) ? atol(p + 12) : 0;
        if (o && on > 0)
            body(s, o + 1, on, 1);
        else
            summary(s, DIM, "(No output)");
        if (rc) {
            cl_copy(m, "Return code ", sizeof(m));
            cl_ltoa(rc, num);
            cl_cat(m, num, sizeof(m));
            raw(s, "     ");
            raw(s, TH->err);
            raw(s, m);
            raw(s, SGR0 "\n");
        }
        break;
    }
    default:
        if (tools_summary(tool, in, inn, p, n, m, sizeof(m)))
            summary(s, "", m);
        else
            body(s, p, n, 1);
        break;
    }
    s->tool = -1;
    out(s);
}

/* ---- thinking ---- */

void show_think(cl_show *s, const char *p, long n)
{
    if (!s->think.n && s->t->io->ms)
        s->think_t0 = s->t->io->ms(s->t->io->u);
    jw_raw(&s->think, p, n);
}

/* a thinking block is over: one dim line on the screen, its text in the
 * transcript viewer (Claude Code's "Thought for Ns (ctrl+o ...)") */
static void think_flush(cl_show *s)
{
    char m[120], num[16];
    long secs;
    if (!s->think.n)
        return;
    secs = s->t->io->ms ? (long)((s->t->io->ms(s->t->io->u) - s->think_t0) / 1000) : 0;
    cl_copy(m, "Thought for ", sizeof(m));
    cl_ltoa(secs, num);
    cl_cat(m, num, sizeof(m));
    cl_cat(m, "s", sizeof(m));
    raw(s, "\n" DIM G_STAR " ");
    raw(s, m);
    raw(s, " (ctrl+o to show thinking)" SGR0 "\n");
    out_screen(s);
    raw(s, "\n" DIM G_STAR " Thinking" SGR0 "\n");
    wrapped(s, "  ", "  ", DIM, s->think.p, s->think.n, s->t->cols - 3);
    out_log(s);
    jw_reset(&s->think);
    s->n_think++;
}

void show_server(cl_show *s, int call, const char *line)
{
    think_flush(s);
    if (call) {
        raw(s, "\n");
        raw(s, TH->ok); /* the theme's success colour, as the other tool bullets */
        raw(s, G_BULLET SGR0 " " BOLD);
        {
            int w = 0;
            text(&s->batch, line, (long)strlen(line), s->t->cols - 4, &w);
        }
        raw(s, SGR0 "\n");
    } else
        summary(s, "", line);
    out(s);
}
