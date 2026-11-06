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
static int script_word(sh_shell *sh, line_toks *t, const sh_io *io, long *st, long *failat, int *quit, int *failat_set)
{
    const char *w = t->v[0];
    if (t->redir[0])
        return 0;
    if (word_is(w, "quit")) {
        const char *a = t->n > 1 ? t->v[1] : "0";
        const char *d = (*a == '-' || *a == '+') ? a + 1 : a;
        *quit = 1;
        if (!*d || strspn(d, "0123456789") != strlen(d)) {   /* S7b-5 R19 */
            say_io(sh, io->err, "bad number\n");
            *st = 10;
        } else
            *st = strtol(a, 0, 10);
        if (*st >= *failat) {   /* S7b-5 R9: the failing command is named */
            say_io(sh, io->err, "Quit failed returncode ");
            say_num(sh, io->err, *st);
            sh->amiscr_reported = 1;
        }
        return 1;
    }
    if (word_is(w, "failat")) {
        if (t->n > 1) {
            *failat = strtol(t->v[1], 0, 10);
            *failat_set = 1;
        } else {
            say_io(sh, io->out, "Fail limit: ");   /* S7b-5 R16 */
            say_num(sh, io->out, *failat);
        }
        *st = 0;
        return 1;
    }
    /* D1: inside a script Set, Unset and Get are AmigaDOS local variables, not the bash builtins */
    if (word_is(w, "set")) {
        *st = 0;
        if (t->n > 1) {
            buf v;
            int i, have = 0;
            char *eq = strchr(t->v[1], '=');
            memset(&v, 0, sizeof(v));
            /* Set name=value is Set name value: the text after the first '=' leads the value */
            if (eq) {
                *eq = 0;
                buf_adds(&v, eq + 1);
                have = *(eq + 1) != 0;
            }
            for (i = 2; i < t->n; i++) {
                if (i > 2 || have)
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
    if (word_is(w, "cd") && t->n == 1) {
        /* AmigaDOS CD with no argument prints the current directory (3.1/3.2 Shell); bash's cd goes home (S7c-5 X2) */
        char *d = sh->os.cwd ? sh->os.cwd(sh->os.data) : 0;
        if (d) {
            say_io(sh, io->out, d);
            say_io(sh, io->out, "\n");
            free(d);
        }
        *st = 0;
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

/* Skip: from p, the pointer after the first `Lab <label>` (any Lab when label is empty) or EndSkip,
 * 0 when there is none. *endskip says it was an EndSkip. Only the first words of a line count. */
static const char *skip_find(const char *p, const char *label, int *endskip)
{
    while (*p) {
        char w[16];
        const char *e = strchr(p, '\n');
        const char *q = p;
        first_word(p, w, sizeof(w));
        if (!strcmp(w, "endskip")) {
            *endskip = 1;
            return e ? e + 1 : p + strlen(p);
        }
        if (!strcmp(w, "lab")) {
            size_t n = 0;
            char a[64];
            while (*q == ' ' || *q == '\t')
                q++;
            while (*q && *q != ' ' && *q != '\t' && *q != '\n')
                q++;
            while (*q == ' ' || *q == '\t')
                q++;
            while (*q && *q != ' ' && *q != '\t' && *q != ';' && *q != '\n' && *q != '\r' && n + 1 < sizeof(a))
                a[n++] = *q++;
            a[n] = 0;
            if (!*label || !cmp_word(a, label)) {
                *endskip = 0;
                return e ? e + 1 : p + strlen(p);
            }
        }
        p = e ? e + 1 : p + strlen(p);
    }
    return 0;
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
            if (i + 1 < t->n && word_is(t->v[i + 1], "val"))   /* S7b-5 R11: EXISTS x VAL 1 takes the 1 */
                i += i + 2 < t->n ? 2 : 1;
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

/* ---- .key templates and the dot directives (design section 3, loading) ------------------------------- */

#define MAXITEM 32
#define MAXDEF 32

typedef struct {
    char *name, *alias, *raw;   /* raw: the item as written in .key, for the error text */
    int a, k, s, n, m, f;   /* the /A /K /S /N /M /F flags */
    int set;
    buf val;
} key_item;

typedef struct {
    key_item it[MAXITEM];
    int n;
    char *keytxt;           /* the .key line's template text, asked back by `Execute file ?` */
    int have;               /* a .key line was seen: only then are <...> replaced */
    char bra, ket, dot, dollar;
    char *defname[MAXDEF], *defval[MAXDEF];
    int ndef;
} tmpl;

static char lc(char c)
{
    return (char)tolower((unsigned char)c);
}

static int ieq_n(const char *a, size_t n, const char *b)
{
    size_t i;
    if (strlen(b) != n)
        return 0;
    for (i = 0; i < n; i++)
        if (lc(a[i]) != lc(b[i]))
            return 0;
    return 1;
}

static void tmpl_free(tmpl *t)
{
    int i;
    for (i = 0; i < t->n; i++) {
        free(t->it[i].name);
        free(t->it[i].alias);
        free(t->it[i].raw);
        free(t->it[i].val.s);
    }
    free(t->keytxt);
    for (i = 0; i < t->ndef; i++) {
        free(t->defname[i]);
        free(t->defval[i]);
    }
}

/* TEMPLATE: comma separated NAME[=ALIAS][/A][/K][/S][/N][/M][/F] items */
static void tmpl_parse(tmpl *t, const char *s)
{
    while (*s && t->n < MAXITEM) {
        const char *e = strchr(s, ',');
        size_t len = e ? (size_t)(e - s) : strlen(s);
        key_item *k = &t->it[t->n];
        char *item = dup_n(s, len), *sl, *eq, *p;
        if (!item)
            return;
        memset(k, 0, sizeof(*k));
        k->raw = dup_n(item, strlen(item));
        sl = strchr(item, '/');
        for (p = sl; p && *p; p++) {
            switch (lc(*p)) {
            case 'a': k->a = 1; break;
            case 'k': k->k = 1; break;
            case 's': k->s = 1; break;
            case 'n': k->n = 1; break;
            case 'm': k->m = 1; break;
            case 'f': k->f = 1; break;
            default: break;
            }
        }
        if (sl)
            *sl = 0;
        eq = strchr(item, '=');
        if (eq) {
            *eq = 0;
            k->alias = dup_n(eq + 1, strlen(eq + 1));
        }
        k->name = dup_n(item, strlen(item));
        free(item);
        t->n++;
        s += len + (e ? 1 : 0);
    }
}

static key_item *tmpl_find(tmpl *t, const char *w, size_t n)
{
    int i;
    for (i = 0; i < t->n; i++)
        if (ieq_n(w, n, t->it[i].name) || (t->it[i].alias && ieq_n(w, n, t->it[i].alias)))
            return &t->it[i];
    return 0;
}

/* store one value (a /M item appends); 0 when a /N item gets something that is no number */
static int item_put(key_item *k, const char *v, size_t n)
{
    size_t i;
    if (k->n) {
        i = (n && (*v == '-' || *v == '+')) ? 1 : 0;
        if (i >= n)
            return 0;
        for (; i < n; i++)
            if (!isdigit((unsigned char)v[i]))
                return 0;
    }
    if (k->m && k->set)
        buf_add(&k->val, ' ');
    for (i = 0; i < n; i++)
        buf_add(&k->val, v[i]);
    k->set = 1;
    return 1;
}

/* a /F item takes first and every argument after it, joined with spaces */
static int item_put_rest(key_item *k, char **a, int i, int na, const char *first)
{
    buf j;
    int ok;
    memset(&j, 0, sizeof(j));
    buf_adds(&j, first);
    for (i++; i < na; i++) {
        buf_add(&j, ' ');
        buf_adds(&j, a[i]);
    }
    ok = item_put(k, j.s ? j.s : "", j.n);
    free(j.s);
    return ok;
}

/* Bind the Execute arguments against the template (mini ReadArgs). Returns 0, or 1 after saying why not. */
static int tmpl_bind(sh_shell *sh, tmpl *t, char **a, int na, const sh_io *io)
{
    int i, j;
    for (i = 0; i < na; i++) {
        const char *eq = strchr(a[i], '=');
        key_item *k = tmpl_find(t, a[i], eq ? (size_t)(eq - a[i]) : strlen(a[i]));
        if (k && k->s) {
            k->set = 1;
        } else if (k) {
            const char *v;
            if (eq)
                v = eq + 1;
            else if (i + 1 < na)
                v = a[++i];
            else
                goto bad;
            if (k->f ? !item_put_rest(k, a, i, na, v) : !item_put(k, v, strlen(v)))
                goto bad;
            if (k->f)
                break;
        } else {
            key_item *slot = 0;
            for (j = 0; j < t->n && !slot; j++) {
                key_item *c = &t->it[j];
                if (!c->k && !c->s && (!c->set || c->m))
                    slot = c;
            }
            if (!slot)
                goto bad;
            if (slot->f ? !item_put_rest(slot, a, i, na, a[i]) : !item_put(slot, a[i], strlen(a[i])))
                goto bad;
            if (slot->f)
                break;
        }
    }
    for (j = 0; j < t->n; j++)
        if (t->it[j].a && !t->it[j].set) {
            say_io(sh, io->err, "EXECUTE: Parameters unsuitable for key \"");   /* S7b-5 R3 */
            say_io(sh, io->err, t->it[j].raw ? t->it[j].raw : t->it[j].name);
            say_io(sh, io->err, "\"\nrequired argument missing\nExecute failed returncode 10\n");
            sh->amiscr_load_rc = 10;
            return 1;
        }
    return 0;
bad:
    say_io(sh, io->err, "execute: Bad arguments\n");
    return 1;
}

/* One line starting with the dot character (p, len without the line end). Returns 1 when it is a directive
 * or a comment (the line is dropped), 0 for an ordinary line such as ./prog. */
static int directive(tmpl *t, const char *p, size_t len, int first)
{
    const char *w = p + 1, *end = p + len, *arg, *ne, *v;
    size_t wl;
    if (w >= end || *w == ' ' || *w == '\t')
        return 1;   /* a dot alone or followed by a space: a comment */
    for (wl = 0; w + wl < end && w[wl] != ' ' && w[wl] != '\t'; wl++)
        ;
    arg = w + wl;
    while (arg < end && (*arg == ' ' || *arg == '\t'))
        arg++;
    if (ieq_n(w, wl, "key") || ieq_n(w, wl, "k")) {
        /* real 3.2 honours .KEY on line 1 only (S7b-6 R2); on a later line the line is dropped, nothing bound */
        char *s = first ? dup_n(arg, (size_t)(end - arg)) : 0;
        if (s) {
            tmpl_parse(t, s);
            free(s);
        }
        if (first) {
            t->have = 1;
            t->keytxt = dup_n(arg, (size_t)(end - arg));
        }
    } else if (ieq_n(w, wl, "bra")) {
        if (arg < end)
            t->bra = *arg;
    } else if (ieq_n(w, wl, "ket")) {
        if (arg < end)
            t->ket = *arg;
    } else if (ieq_n(w, wl, "dot")) {
        if (arg < end)
            t->dot = *arg;
    } else if (ieq_n(w, wl, "dollar") || ieq_n(w, wl, "dol")) {
        if (arg < end)
            t->dollar = *arg;
    } else if (ieq_n(w, wl, "default") || ieq_n(w, wl, "def")) {
        for (ne = arg; ne < end && *ne != ' ' && *ne != '\t'; ne++)
            ;
        for (v = ne; v < end && (*v == ' ' || *v == '\t'); v++)
            ;
        if (ne > arg && t->ndef < MAXDEF) {
            t->defname[t->ndef] = dup_n(arg, (size_t)(ne - arg));
            t->defval[t->ndef] = dup_n(v, (size_t)(end - v));
            t->ndef++;
        }
    } else {
        return 0;
    }
    return 1;
}

/* the replacement for <in>: in/n is the text between bra and ket */
static void tmpl_subst(sh_shell *sh, tmpl *t, const char *in, size_t n, buf *out)
{
    size_t d, j;
    int i;
    for (d = 0; d < n && in[d] != t->dollar; d++)
        ;
    if (n == 2 && in[0] == t->dollar && in[1] == t->dollar) {   /* <$$>: the CLI number */
        char num[32];
        long v = sh->ctx.top_pid ? sh->ctx.top_pid : sh->ctx.pid;
        int p = (int)sizeof(num) - 1;
        unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
        num[p] = 0;
        do {
            num[--p] = (char)('0' + (int)(u % 10));
            u /= 10;
        } while (u);
        if (v < 0)
            num[--p] = '-';
        buf_adds(out, num + p);
        return;
    }
    for (i = 0; i < t->n; i++) {
        key_item *k = &t->it[i];
        if (k->set && (ieq_n(in, d, k->name) || (k->alias && ieq_n(in, d, k->alias)))) {
            buf_adds(out, k->s ? k->name : k->val.s);
            return;
        }
    }
    if (d < n) {   /* <name$default> */
        for (j = d + 1; j < n; j++)
            buf_add(out, in[j]);
        return;
    }
    for (i = 0; i < t->ndef; i++)
        if (ieq_n(in, n, t->defname[i])) {
            buf_adds(out, t->defval[i]);
            return;
        }
}

/* Directives are collected over the whole file and dropped; with a .key line the arguments are bound and
 * every <name>, <name$default> and <$$> is replaced. Returns the new text, or 0 after saying why (rc 20). */
static char *template_load(sh_shell *sh, const char *text, int argc, char **argv, const sh_io *io)
{
    tmpl t;
    buf out;
    const char *p;
    size_t nl = 1, li;
    char *isdir;
    memset(&t, 0, sizeof(t));
    memset(&out, 0, sizeof(out));
    t.bra = '<';
    t.ket = '>';
    t.dot = '.';
    t.dollar = '$';
    for (p = text; *p; p++)
        if (*p == '\n')
            nl++;
    isdir = (char *)calloc(nl + 1, 1);
    if (!isdir)
        return 0;
    for (p = text, li = 0; *p; li++) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p), dl = len;
        if (dl && p[dl - 1] == '\r')
            dl--;
        if (*p == t.dot && directive(&t, p, dl, li == 0))
            isdir[li] = 1;
        p += len + (e ? 1 : 0);
    }
    if (t.have && argc == 2 && !strcmp(argv[1], "?") && !sh->amiscr_q1) {
        /* `Execute file ?` on 3.2 (S7b-6 R6): the template is printed, one reply line is read from the input and
         * bound like command-line arguments. No reply (input ended) binds nothing. */
        char line[512];
        long n;
        line_toks rt;
        int bad, eof;
        memset(&rt, 0, sizeof(rt));
        say_io(sh, io->out, t.keytxt ? t.keytxt : "");
        say_io(sh, io->out, ": ");
        n = sh->os.read_line(sh->os.data, io->in, line, (long)sizeof(line));
        eof = n <= 0;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            n--;
        line[n < 0 ? 0 : n] = 0;
        /* Input ended before any reply (NIL:): Execute runs with nothing bound and does not fail /A (S7c-7 a). A blank line is a reply. */
        bad = eof ? 0 : lex_line(sh, line, &rt) || tmpl_bind(sh, &t, rt.v, rt.n, io);
        toks_free(&rt);
        if (bad) {
            free(isdir);
            tmpl_free(&t);
            return 0;
        }
    } else if (t.have && tmpl_bind(sh, &t, argv + 1, argc - 1, io)) {
        free(isdir);
        tmpl_free(&t);
        return 0;
    }
    for (p = text, li = 0; *p; li++) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p), i;
        if (!isdir[li]) {
            for (i = 0; i < len; i++) {
                if (t.have && p[i] == t.bra) {
                    size_t j = i + 1;
                    while (j < len && p[j] != t.ket && p[j] != t.bra && p[j] != ' ' && p[j] != '\t' && p[j] != ';')
                        j++;
                    if (j < len && p[j] == t.ket) {
                        tmpl_subst(sh, &t, p + i + 1, j - i - 1, &out);
                        i = j;
                        continue;
                    }
                }
                buf_add(&out, p[i]);
            }
            buf_add(&out, '\n');
        }
        p += len + (e ? 1 : 0);
    }
    if (!out.s)
        buf_add(&out, '\n');
    free(isdir);
    tmpl_free(&t);
    return out.s;
}

/* `cmd` in a script line: run cmd through the shell's own $( ) capture (ctx.subst) and put its output in the
 * line, trailing newlines dropped, inner newlines turned into spaces. Done before the line is lexed, so it
 * works inside double quotes too.
 * UNVERIFIED against the real Shell (S7 R-checks): (1) substitution inside double quotes, (2) an unclosed
 * backtick stays literal, (3) inner newlines become single spaces. Returns the new line (malloc). */
static char *line_backticks(sh_shell *sh, const char *ln)
{
    buf out;
    size_t i, n = strlen(ln);
    memset(&out, 0, sizeof(out));
    for (i = 0; i < n; i++) {
        const char *close = ln[i] == '`' ? (const char *)memchr(ln + i + 1, '`', n - i - 1) : 0;
        if (close && sh->ctx.subst) {
            char *cmd = dup_n(ln + i + 1, (size_t)(close - ln) - i - 1);
            char *res = cmd ? sh->ctx.subst(&sh->ctx, cmd) : 0;
            size_t l = res ? strlen(res) : 0, k;
            free(cmd);
            while (l && res[l - 1] == '\n')
                l--;
            for (k = 0; k < l; k++)
                buf_add(&out, res[k] == '\n' ? ' ' : res[k]);
            free(res);
            i = (size_t)(close - ln);
            continue;
        }
        buf_add(&out, ln[i]);
    }
    if (!out.s)
        return dup_n("", 0);
    return out.s;
}

long sh_amiga_execute(sh_shell *sh, const char *text0, const char *file, int argc, char **argv, const sh_io *io)
{
    char *loaded;
    const char *text;
    const char *p;
    long st = sh->ctx.status;
    long failat = sh->amiscr_depth ? sh->amiscr_failat : 10;   /* S7c-3 R17: the fail limit belongs to the CLI, so a nested Execute starts at the caller's current limit */
    int quit = 0, failat_set = 0;
    (void)file;
    sh->failat_leak = 0;
    sh->amiscr_load_rc = 20;
    loaded = template_load(sh, text0, argc, argv, io);
    text = loaded;
    p = text;
    if (!loaded) {
        sh->ctx.status = sh->amiscr_load_rc;   /* bad arguments against the .key template: nothing ran */
        return sh->amiscr_load_rc;
    }
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
        if (strchr(ln, '`')) {
            char *sub = line_backticks(sh, ln);
            free(ln);
            ln = sub;
            if (!ln)
                break;
        }
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
        } else if (t.n && !t.redir[0] && (word_is(t.v[0], "endif") || word_is(t.v[0], "lab") || word_is(t.v[0], "endskip"))) {
            /* the end of a block that ran, a label, or an EndSkip reached in normal flow: nothing to do */
        } else if (t.n && !t.redir[0] && word_is(t.v[0], "skip")) {
            const char *label = "";
            int back = 0, endskip = 0, i;
            const char *to;
            for (i = 1; i < t.n; i++) {
                if (word_is(t.v[i], "back"))
                    back = 1;
                else
                    label = t.v[i];
            }
            to = skip_find(back ? text : next, label, &endskip);
            if (to) {
                next = to;
                st = endskip ? 5 : 0;   /* S7c-3 R15: a Skip that finds its label succeeds, so $RC reads 0 */
            } else {
                say_io(sh, io->err, "object not found\nSkip failed returncode 10\n");   /* S7b-5 R13/R14 */
                sh->amiscr_reported = 1;
                st = 10;
                quit = 1;
            }
        } else if (t.n && !script_word(sh, &t, io, &st, &failat, &quit, &failat_set)) {
            sh->amiscr_reported = 0;
            sh->amiscr_failat = failat;
            sh->amiscr_depth++;
            st = run_tokens(sh, &t, io);
            sh->amiscr_depth--;
            if (sh->failat_leak) {   /* a nested script's FailAt leaks to this one, as on 3.2 (S7b-6 R17) */
                failat = sh->failat_leak_val;
                failat_set = 1;
                sh->failat_leak = 0;
            }
            if (st >= failat && sh->amiscr_reported)
                quit = 1;   /* a nested script already named the failing command (S7b-5 R9) */
            else if (st >= failat) {
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
    free(loaded);
    if (failat_set) {
        sh->failat_leak = 1;
        sh->failat_leak_val = failat;
    }
    sh->ctx.status = st;
    return st;
}
