/* vsh's AmigaDOS script mode: lexer and line runner. See sh_amiscript.h. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

static int word_is(const char *a, const char *b);

/* the value of $name: RC and ? are the last return code, else a shell variable, else ENV: */
static void var_value(sh_shell *sh, const char *name, buf *out)
{
    const char *v;
    char tmp[512];
    if (!strcmp(name, "?") || word_is(name, "rc")) {
        long n = sh->ctx.status;
        int i = (int)sizeof(tmp) - 1;
        unsigned long u = n < 0 ? 0UL - (unsigned long)n : (unsigned long)n;
        tmp[i] = 0;
        do {
            tmp[--i] = (char)('0' + (int)(u % 10));
            u /= 10;
        } while (u);
        if (n < 0)
            tmp[--i] = '-';
        buf_adds(out, tmp + i);
        return;
    }
    v = sh_get(&sh->ctx, name);
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
    if (s[1] == '?') {
        var_value(sh, "?", out);
        return 2;
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

/* AmigaDOS names ignore case */
static int word_is(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
    return !*a && !*b;
}

static void say_io(sh_shell *sh, sh_fh fh, const char *s)
{
    sh->os.write(sh->os.data, fh, s, (long)strlen(s));
}

static void say_num(sh_shell *sh, sh_fh fh, long n)
{
    char num[32];
    int i = (int)sizeof(num) - 1;
    unsigned long u = n < 0 ? 0UL - (unsigned long)n : (unsigned long)n;
    num[i] = 0;
    num[--i] = '\n';
    do {
        num[--i] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while (u);
    if (n < 0)
        num[--i] = '-';
    say_io(sh, fh, num + i);
}

/* The interpreter's own words. Returns 1 when t's first token is one and sets *st to its result code;
 * *failat is this call's limit and *quit is set by Quit (the script then ends with *st). */
static int script_word(sh_shell *sh, line_toks *t, const sh_io *io, long *st, long *failat, int *quit)
{
    const char *w = t->v[0];
    if (t->redir[0])
        return 0;
    if (word_is(w, "quit")) {
        *st = t->n > 1 ? strtol(t->v[1], 0, 10) : 0;
        *quit = 1;
        return 1;
    }
    if (word_is(w, "failat")) {
        if (t->n > 1)
            *failat = strtol(t->v[1], 0, 10);
        else
            say_num(sh, io->out, *failat);
        *st = 0;
        return 1;
    }
    /* D1: inside a script Set, Unset and Get are AmigaDOS local variables, not the bash builtins */
    if (word_is(w, "set")) {
        *st = 0;
        if (t->n > 1) {
            buf v;
            int i;
            memset(&v, 0, sizeof(v));
            for (i = 2; i < t->n; i++) {
                if (i > 2)
                    buf_add(&v, ' ');
                buf_adds(&v, t->v[i]);
            }
            if (sh_set(&sh->ctx, t->v[1], v.s ? v.s : ""))
                *st = 20;
            free(v.s);
        }
        return 1;
    }
    if (word_is(w, "unset")) {
        *st = t->n > 1 && sh_unset(&sh->ctx, t->v[1]) ? 20 : 0;
        return 1;
    }
    if (word_is(w, "get")) {
        const char *v = t->n > 1 ? sh_get(&sh->ctx, t->v[1]) : 0;
        if (v) {
            say_io(sh, io->out, v);
            say_io(sh, io->out, "\n");
        }
        *st = 0;
        return 1;
    }
    return 0;
}

/* the first word of the line at p, lower-cased into w (comment and quotes not interpreted) */
static void first_word(const char *p, char *w, size_t cap)
{
    size_t i = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    while (*p && *p != ' ' && *p != '\t' && *p != ';' && *p != '\n' && *p != '\r' && i + 1 < cap)
        w[i++] = (char)tolower((unsigned char)*p++);
    w[i] = 0;
}

/* Textual skip over an If block, from the start of the line at p: the pointer after the EndIf that
 * matches (or, with stop_at_else, after an Else at the same depth); 0 at the end of the text. No If
 * stack is kept; only the first word of each line counts, so `echo If` does not. */
static const char *skip_if(const char *p, int stop_at_else)
{
    int depth = 0;
    while (*p) {
        char w[16];
        const char *e = strchr(p, '\n');
        first_word(p, w, sizeof(w));
        p = e ? e + 1 : p + strlen(p);
        if (!strcmp(w, "if"))
            depth++;
        else if (!strcmp(w, "endif")) {
            if (!depth)
                return p;
            depth--;
        } else if (!strcmp(w, "else") && !depth && stop_at_else)
            return p;
    }
    return 0;
}

static int cmp_word(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return tolower((unsigned char)*a) - tolower((unsigned char)*b);
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* If's condition: NOT, WARN, ERROR, FAIL (the last rc), a EQ|GT|GE b [VAL], EXISTS name; several are
 * ANDed and NOT inverts the whole (the design's choice, UNVERIFIED against C:If) */
static int if_true(sh_shell *sh, line_toks *t)
{
    int i, neg = 0, val = 0, ok = 1;
    long rc = sh->ctx.status;
    for (i = 1; i < t->n; i++)
        if (word_is(t->v[i], "val"))
            val = 1;
    for (i = 1; i < t->n; i++) {
        const char *w = t->v[i];
        if (word_is(w, "not"))
            neg = !neg;
        else if (word_is(w, "val"))
            continue;
        else if (word_is(w, "warn"))
            ok = ok && rc >= 5;
        else if (word_is(w, "error"))
            ok = ok && rc >= 10;
        else if (word_is(w, "fail"))
            ok = ok && rc >= 20;
        else if (word_is(w, "exists") && i + 1 < t->n) {
            sh_stat sb;
            ok = ok && sh->os.stat && sh->os.stat(sh->os.data, t->v[++i], &sb, 0) == 0;
        } else if (i + 2 < t->n && (word_is(t->v[i + 1], "eq") || word_is(t->v[i + 1], "gt") ||
                                    word_is(t->v[i + 1], "ge"))) {
            int c;
            char op = (char)tolower((unsigned char)t->v[i + 1][1]);
            if (val) {
                long a = strtol(w, 0, 10), b = strtol(t->v[i + 2], 0, 10);
                c = a < b ? -1 : a > b;
            } else
                c = cmp_word(w, t->v[i + 2]);
            ok = ok && (op == 'q' ? c == 0 : op == 't' ? c > 0 : c >= 0);
            i += 2;
        } else
            ok = 0;
    }
    return neg ? !ok : ok;
}

long sh_amiga_execute(sh_shell *sh, const char *text, const char *file, int argc, char **argv, const sh_io *io)
{
    const char *p = text;
    long st = sh->ctx.status;
    long failat = 10;   /* this call's limit: a nested Execute starts at 10 and this one survives it */
    int quit = 0;
    (void)file;
    (void)argc;
    (void)argv;
    while (*p && !sh->intr && !sh->exiting) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        const char *next = p + len + (e ? 1 : 0);
        char *ln = dup_n(p, len);
        line_toks t;
        if (!ln)
            break;
        if (len && ln[len - 1] == '\r')
            ln[len - 1] = 0;
        memset(&t, 0, sizeof(t));
        sh->ctx.status = st;   /* $RC and If see the last return code */
        if (lex_line(sh, ln, &t)) {
            /* an unlexable line is skipped */
        } else if (t.n && !t.redir[0] && word_is(t.v[0], "if")) {
            if (!if_true(sh, &t)) {
                next = skip_if(next, 1);
                if (!next)
                    quit = 1;
            }
        } else if (t.n && !t.redir[0] && word_is(t.v[0], "else")) {
            next = skip_if(next, 0);
            if (!next)
                quit = 1;
        } else if (t.n && !t.redir[0] && word_is(t.v[0], "endif")) {
            /* the end of a block that ran: nothing to do */
        } else if (t.n && !script_word(sh, &t, io, &st, &failat, &quit)) {
            st = run_tokens(sh, &t, io);
            if (st >= failat) {
                say_io(sh, io->err, t.v[0]);
                say_io(sh, io->err, " failed returncode ");
                say_num(sh, io->err, st);
                quit = 1;
            }
        }
        toks_free(&t);
        free(ln);
        if (quit)
            break;
        p = next;
    }
    sh->ctx.status = st;
    return st;
}
