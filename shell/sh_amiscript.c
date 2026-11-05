/* vsh's AmigaDOS script mode: lexer and line runner. See sh_amiscript.h. */
#include <stdlib.h>
#include <string.h>
#include "sh_amiscript.h"
#include "sh_hits.h"

#define MAXTOK 256

typedef struct {
    char *v[MAXTOK];
    int n;
    int redir[MAXTOK];   /* 0 word, 1 > , 2 >> , 3 < : v[i] is then the redirection's target */
} line_toks;

typedef struct {
    char *s;
    size_t n, cap;
} buf;

static char *dup_n(const char *s, size_t n)
{
    char *r = (char *)malloc(n + 1);
    if (r) {
        memcpy(r, s, n);
        r[n] = 0;
    }
    return r;
}

static int buf_add(buf *b, char c)
{
    if (b->n + 2 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 32;
        char *t = (char *)realloc(b->s, nc);
        if (!t)
            return 0;
        b->s = t;
        b->cap = nc;
    }
    b->s[b->n++] = c;
    b->s[b->n] = 0;
    return 1;
}

static void buf_adds(buf *b, const char *s)
{
    for (; s && *s; s++)
        if (!buf_add(b, *s))
            return;
}

/* the value of $name: a shell variable, else ENV: */
static void var_value(sh_shell *sh, const char *name, buf *out)
{
    const char *v = sh_get(&sh->ctx, name);
    char tmp[512];
    if (!v && sh->os.getenv && sh->os.getenv(sh->os.data, name, tmp, sizeof(tmp)))
        v = tmp;
    buf_adds(out, v);
}

/* $name or ${name} at s (which points at the '$'): appends the value, returns the characters used (1 when
 * the dollar stands alone and is kept) */
static size_t dollar(sh_shell *sh, const char *s, buf *out)
{
    char name[128];
    size_t i = 0;
    if (s[1] == '{') {
        const char *e = strchr(s + 2, '}');
        if (!e || (size_t)(e - (s + 2)) >= sizeof(name)) {
            buf_add(out, '$');
            return 1;
        }
        memcpy(name, s + 2, (size_t)(e - (s + 2)));
        name[e - (s + 2)] = 0;
        var_value(sh, name, out);
        return (size_t)(e - s) + 1;
    }
    while (i < sizeof(name) - 1 && (s[1 + i] == '_' || (s[1 + i] >= '0' && s[1 + i] <= '9') ||
                                     (s[1 + i] >= 'a' && s[1 + i] <= 'z') || (s[1 + i] >= 'A' && s[1 + i] <= 'Z'))) {
        name[i] = s[1 + i];
        i++;
    }
    if (!i) {
        buf_add(out, '$');
        return 1;
    }
    name[i] = 0;
    var_value(sh, name, out);
    return i + 1;
}

/* the escapes after a '*': *" ** *N *E inside quotes, *$ anywhere; any other star is literal */
static int star_escape(const char *s, int in_quote, buf *out)
{
    char c = s[1];
    if (c == '$' || (in_quote && (c == '"' || c == '*'))) {
        buf_add(out, c);
        return 2;
    }
    if (in_quote && (c == 'N' || c == 'n')) {
        buf_add(out, '\n');
        return 2;
    }
    if (in_quote && (c == 'E' || c == 'e')) {
        buf_add(out, 27);
        return 2;
    }
    buf_add(out, '*');
    return 1;
}

static void toks_free(line_toks *t)
{
    int i;
    for (i = 0; i < t->n; i++)
        free(t->v[i]);
    t->n = 0;
}

/* Split one line into tokens. ';' outside quotes starts a comment. Returns 0 on success. */
static int lex_line(sh_shell *sh, const char *p, line_toks *t)
{
    t->n = 0;
    for (;;) {
        buf b;
        int redir = 0;
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p || *p == ';')
            return 0;
        memset(&b, 0, sizeof(b));
        if (*p == '>') {
            redir = 1;
            p++;
            if (*p == '>') {
                redir = 2;
                p++;
            }
        } else if (*p == '<') {
            redir = 3;
            p++;
        }
        if (redir)
            while (*p == ' ' || *p == '\t')
                p++;
        for (;;) {
            if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '*')
                        p += star_escape(p, 1, &b);
                    else if (*p == '$')
                        p += dollar(sh, p, &b);
                    else
                        buf_add(&b, *p++);
                }
                if (*p == '"')
                    p++;
            } else if (*p == '*' && p[1] == '$') {
                p += star_escape(p, 0, &b);
            } else if (*p == '$') {
                p += dollar(sh, p, &b);
            } else if (*p && *p != ' ' && *p != '\t' && *p != ';') {
                buf_add(&b, *p++);
            } else
                break;
        }
        if (t->n >= MAXTOK) {
            free(b.s);
            return 1;
        }
        t->v[t->n] = b.s ? b.s : dup_n("", 0);
        if (!t->v[t->n])
            return 1;
        t->redir[t->n] = redir;
        t->n++;
    }
}

/* a word the sh expander leaves alone without quotes: it stays as it is, which lets an alias or function
 * of that name match the command word */
static int plain_word(const char *w)
{
    if (!*w)
        return 0;
    for (; *w; w++) {
        char c = *w;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '/' || c == ':' || c == '-' || c == '+' || c == ',' || c == '@' || c == '%'))
            return 0;
    }
    return 1;
}

static char *quote_word(const char *w)
{
    buf b;
    memset(&b, 0, sizeof(b));
    if (plain_word(w)) {
        buf_adds(&b, w);
        return b.s;
    }
    buf_add(&b, '\'');
    for (; *w; w++) {
        if (*w == '\'')
            buf_adds(&b, "'\\''");
        else
            buf_add(&b, *w);
    }
    buf_add(&b, '\'');
    return b.s;
}

/* run one tokenized line as a literal SH_CMD */
static long run_tokens(sh_shell *sh, line_toks *t, const sh_io *io)
{
    sh_node c;
    sh_word *words = 0, **wt = &words;
    sh_redir *redirs = 0, **rt = &redirs;
    int i;
    long st = sh->ctx.status;
    for (i = 0; i < t->n; i++) {
        if (t->redir[i]) {
            sh_redir *r = (sh_redir *)calloc(1, sizeof(*r));
            if (!r)
                break;
            r->fd = t->redir[i] == 3 ? 0 : 1;
            r->kind = t->redir[i] == 1 ? SH_R_OUT : t->redir[i] == 2 ? SH_R_APPEND : SH_R_IN;
            r->target = quote_word(t->v[i]);
            *rt = r;
            rt = &r->next;
        } else {
            sh_word *w = (sh_word *)calloc(1, sizeof(*w));
            if (!w)
                break;
            w->text = quote_word(t->v[i]);
            *wt = w;
            wt = &w->next;
        }
    }
    memset(&c, 0, sizeof(c));
    c.kind = SH_CMD;
    c.words = words;
    c.redirs = redirs;
    if (words || redirs) {
        SH_HIT(AMISCRIPT_LINE);
        st = sh_exec(sh, &c, io);
    }
    while (words) {
        sh_word *n = words->next;
        free(words->text);
        free(words);
        words = n;
    }
    while (redirs) {
        sh_redir *n = redirs->next;
        free(redirs->target);
        free(redirs);
        redirs = n;
    }
    return st;
}

long sh_amiga_execute(sh_shell *sh, const char *text, const char *file, int argc, char **argv, const sh_io *io)
{
    const char *p = text;
    long st = sh->ctx.status;
    (void)file;
    (void)argc;
    (void)argv;
    while (*p && !sh->intr && !sh->exiting) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *ln = dup_n(p, len);
        line_toks t;
        if (!ln)
            break;
        if (len && ln[len - 1] == '\r')
            ln[len - 1] = 0;
        memset(&t, 0, sizeof(t));
        if (!lex_line(sh, ln, &t) && t.n)
            st = run_tokens(sh, &t, io);
        toks_free(&t);
        free(ln);
        p += len + (e ? 1 : 0);
    }
    sh->ctx.status = st;
    return st;
}
