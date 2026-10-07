/* vsh's parser: POSIX sh grammar to a tree (see sh_parse.h). */
#include <stdlib.h>
#include <string.h>
#include "sh_parse.h"

/* ---- arena ------------------------------------------------------------------ */

typedef struct block {
    struct block *next;
    long used, size;
    /* data follows */
} block;

static void *alloc(sh_parse *p, long n)
{
    block *b = (block *)p->arena;
    char *r;
    n = (n + 7) & ~7L;
    if (!b || b->used + n > b->size) {
        long size = n > 4000 ? n : 4000;
        block *nb = (block *)malloc(sizeof(block) + size);
        if (!nb)
            return 0;
        nb->next = b;
        nb->used = 0;
        nb->size = size;
        p->arena = nb;
        b = nb;
    }
    r = (char *)(b + 1) + b->used;
    b->used += n;
    memset(r, 0, n);
    return r;
}

static char *dup_n(sh_parse *p, const char *s, long n)
{
    char *r = (char *)alloc(p, n + 1);
    if (r) {
        memcpy(r, s, n);
        r[n] = 0;
    }
    return r;
}

void sh_parse_free(sh_parse *p)
{
    block *b = (block *)p->arena;
    while (b) {
        block *n = b->next;
        free(b);
        b = n;
    }
    p->arena = 0;
    p->tree = 0;
}

/* ---- lexer ------------------------------------------------------------------ */

enum tok {
    T_EOF, T_WORD, T_NEWLINE, T_SEMI, T_DSEMI, T_AMP, T_AND, T_PIPE, T_OR,
    T_LPAREN, T_RPAREN, T_LT, T_GT, T_DGT, T_LTAMP, T_GTAMP, T_AMPGT, T_DLT
};

typedef struct heredoc {
    sh_redir *r;
    char *delim;
    struct heredoc *next;
} heredoc;

typedef struct lexer {
    sh_parse *p;
    const char *s;
    long pos;
    enum tok tok;           /* the current token */
    char *word;             /* its text, for T_WORD */
    int quoted;             /* the word had quoting in it */
    int io_number;          /* -1, or the fd digits before a redirection operator */
    heredoc *pending;       /* << documents to read at the next newline */
    int had_error;
} lexer;

static int is_meta(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == ';' || c == '&' || c == '|' ||
           c == '(' || c == ')' || c == '<' || c == '>' || c == 0;
}

static void fail(lexer *L, const char *msg, int incomplete)
{
    if (!L->had_error) {
        L->p->error = msg;
        L->p->incomplete = incomplete;
    }
    L->had_error = 1;
}

/* The end of a word starting at i (quotes, escapes, $( ), ${ }, ` ` kept);
 * -1 when the input ends inside a quote. */
static long word_end(lexer *L, long i, int *quoted)
{
    const char *s = L->s;
    int depth;
    while (!is_meta(s[i])) {
        char c = s[i];
        if (c == '\\') {
            *quoted = 1;
            if (!s[i + 1])
                return -1;
            i += 2;
        } else if (c == '\'') {
            *quoted = 1;
            i++;
            while (s[i] && s[i] != '\'')
                i++;
            if (!s[i])
                return -1;
            i++;
        } else if (c == '"') {
            *quoted = 1;
            i++;
            while (s[i] && s[i] != '"') {
                if (s[i] == '\\' && s[i + 1])
                    i++;
                i++;
            }
            if (!s[i])
                return -1;
            i++;
        } else if (c == '`') {
            i++;
            while (s[i] && s[i] != '`') {
                if (s[i] == '\\' && s[i + 1])
                    i++;
                i++;
            }
            if (!s[i])
                return -1;
            i++;
        } else if (c == '$' && (s[i + 1] == '(' || s[i + 1] == '{')) {
            char open = s[i + 1], close = open == '(' ? ')' : '}';
            i += 2;
            depth = 1;
            while (s[i] && depth) {
                if (s[i] == '\\' && s[i + 1])
                    i++;
                else if (s[i] == '\'') {
                    i++;
                    while (s[i] && s[i] != '\'')
                        i++;
                } else if (s[i] == open)
                    depth++;
                else if (s[i] == close)
                    depth--;
                if (s[i])
                    i++;
            }
            if (depth)
                return -1;
        } else {
            i++;
        }
    }
    return i;
}

/* Backslash-newline is removed wherever it is not inside single quotes
 * (POSIX 2.2.1): "a\<newline>b" is the one word "ab". */
static void strip_continuations(char *w)
{
    char *o = w;
    int single = 0, dbl = 0;
    while (*w) {
        if (*w == '\'' && !dbl)
            single = !single;
        else if (*w == '"' && !single)
            dbl = !dbl;
        if (!single && w[0] == '\\' && w[1] == '\n') {
            w += 2;
            continue;
        }
        if (!single && w[0] == '\\' && w[1])
            *o++ = *w++; /* an escaped character: copied as a pair */
        *o++ = *w++;
    }
    *o = 0;
}

static void read_heredocs(lexer *L)
{
    heredoc *h = L->pending;
    L->pending = 0;
    for (; h; h = h->next) {
        const char *s = L->s;
        long start = L->pos, i = start, body_end = start;
        int dlen = (int)strlen(h->delim);
        for (;;) {
            long ls = i;
            while (s[i] && s[i] != '\n')
                i++;
            if ((long)(i - ls) == dlen && !strncmp(s + ls, h->delim, dlen)) {
                body_end = ls;
                if (s[i])
                    i++;
                break;
            }
            if (!s[i]) {
                fail(L, "here-document not ended", 1);
                return;
            }
            i++;
        }
        h->r->target = dup_n(L->p, s + start, body_end - start);
        L->pos = i;
    }
}

static void next(lexer *L)
{
    const char *s = L->s;
    long i;
    L->word = 0;
    L->quoted = 0;
    L->io_number = -1;
    for (;;) {
        while (s[L->pos] == ' ' || s[L->pos] == '\t')
            L->pos++;
        if (s[L->pos] == '\\' && s[L->pos + 1] == '\n') {
            L->pos += 2; /* line continuation */
            continue;
        }
        if (s[L->pos] == '#') {
            while (s[L->pos] && s[L->pos] != '\n')
                L->pos++;
        }
        break;
    }
    i = L->pos;
    switch (s[i]) {
    case 0:
        L->tok = T_EOF;
        return;
    case '\n':
        L->pos++;
        L->tok = T_NEWLINE;
        if (L->pending)
            read_heredocs(L);
        return;
    case ';':
        L->tok = s[i + 1] == ';' ? T_DSEMI : T_SEMI;
        L->pos += L->tok == T_DSEMI ? 2 : 1;
        return;
    case '&':
        if (s[i + 1] == '&') {
            L->tok = T_AND;
            L->pos += 2;
        } else if (s[i + 1] == '>') {
            L->tok = T_AMPGT;
            L->pos += 2;
        } else {
            L->tok = T_AMP;
            L->pos++;
        }
        return;
    case '|':
        L->tok = s[i + 1] == '|' ? T_OR : T_PIPE;
        L->pos += L->tok == T_OR ? 2 : 1;
        return;
    case '(':
        L->tok = T_LPAREN;
        L->pos++;
        return;
    case ')':
        L->tok = T_RPAREN;
        L->pos++;
        return;
    case '<':
        if (s[i + 1] == '<') {
            L->tok = T_DLT;
            L->pos += 2;
        } else if (s[i + 1] == '&') {
            L->tok = T_LTAMP;
            L->pos += 2;
        } else {
            L->tok = T_LT;
            L->pos++;
        }
        return;
    case '>':
        if (s[i + 1] == '>') {
            L->tok = T_DGT;
            L->pos += 2;
        } else if (s[i + 1] == '&') {
            L->tok = T_GTAMP;
            L->pos += 2;
        } else {
            L->tok = T_GT;
            L->pos++;
        }
        return;
    default:
        break;
    }
    /* digits right before < or >: an IO number */
    {
        long k = i;
        while (s[k] >= '0' && s[k] <= '9')
            k++;
        if (k > i && (s[k] == '<' || s[k] == '>')) {
            int fd = atoi(s + i);
            L->pos = k;
            next(L);
            L->io_number = fd;
            return;
        }
    }
    {
        int quoted = 0;
        long e = word_end(L, i, &quoted);
        if (e < 0) {
            fail(L, "quote not closed", 1);
            L->tok = T_EOF;
            return;
        }
        L->word = dup_n(L->p, s + i, e - i);
        strip_continuations(L->word);
        L->quoted = quoted;
        L->pos = e;
        L->tok = T_WORD;
    }
}

/* ---- parser ------------------------------------------------------------------ */

static sh_node *parse_list(lexer *L, int top);
static sh_node *parse_command(lexer *L);

static sh_node *node(lexer *L, enum sh_kind k)
{
    sh_node *n = (sh_node *)alloc(L->p, sizeof(sh_node));
    if (n)
        n->kind = k;
    return n;
}

/* A reserved word: only unquoted, only where a command starts. */
static int is_word(lexer *L, const char *w)
{
    return L->tok == T_WORD && !L->quoted && !strcmp(L->word, w);
}

/* A word that ends a list inside a compound command. */
static int at_list_end(lexer *L)
{
    static const char *const ends[] = { "then", "else", "elif", "fi", "do", "done", "esac", "}", 0 };
    int i;
    if (L->tok == T_EOF || L->tok == T_RPAREN || L->tok == T_DSEMI)
        return 1;
    for (i = 0; ends[i]; i++)
        if (is_word(L, ends[i]))
            return 1;
    return 0;
}

static void skip_newlines(lexer *L)
{
    while (L->tok == T_NEWLINE)
        next(L);
}

static void expect_word(lexer *L, const char *w)
{
    skip_newlines(L);
    if (is_word(L, w)) {
        next(L);
        return;
    }
    fail(L, "syntax error", L->tok == T_EOF);
}

static sh_word *new_word(lexer *L, char *text)
{
    sh_word *w = (sh_word *)alloc(L->p, sizeof(sh_word));
    if (w)
        w->text = text;
    return w;
}

static void append_word(sh_word **list, sh_word *w)
{
    while (*list)
        list = &(*list)->next;
    *list = w;
}

/* A redirection, if the current token starts one: 1, else 0. */
static int parse_redir(lexer *L, sh_redir **list)
{
    enum tok t = L->tok;
    int fd = L->io_number;
    sh_redir *r;
    if (t != T_LT && t != T_GT && t != T_DGT && t != T_LTAMP && t != T_GTAMP && t != T_AMPGT &&
        t != T_DLT)
        return 0;
    r = (sh_redir *)alloc(L->p, sizeof(sh_redir));
    if (!r)
        return 0;
    switch (t) {
    case T_LT: r->kind = SH_R_IN; break;
    case T_GT: r->kind = SH_R_OUT; break;
    case T_DGT: r->kind = SH_R_APPEND; break;
    case T_LTAMP: r->kind = SH_R_DUPIN; break;
    case T_GTAMP: r->kind = SH_R_DUPOUT; break;
    case T_AMPGT: r->kind = SH_R_BOTH; break;
    default: r->kind = SH_R_HEREDOC; break;
    }
    r->fd = fd >= 0 ? fd : (r->kind == SH_R_IN || r->kind == SH_R_DUPIN || r->kind == SH_R_HEREDOC) ? 0 : 1;
    next(L);
    if (L->tok != T_WORD) {
        fail(L, "redirection without a target", L->tok == T_EOF);
        return 1;
    }
    if (r->kind == SH_R_HEREDOC) {
        /* the delimiter: quotes removed; quoting makes the document literal */
        heredoc *h = (heredoc *)alloc(L->p, sizeof(heredoc)), **tail;
        char *d = L->word, *o;
        int k;
        r->quoted = L->quoted;
        o = d;
        for (k = 0; d[k]; k++)
            if (d[k] != '\'' && d[k] != '"' && d[k] != '\\')
                *o++ = d[k];
        *o = 0;
        h->r = r;
        h->delim = d;
        for (tail = &L->pending; *tail; tail = &(*tail)->next)
            ;
        *tail = h;
    } else {
        r->target = L->word;
    }
    next(L);
    while (*list)
        list = &(*list)->next;
    *list = r;
    return 1;
}

static int is_assignment(const char *w)
{
    int i = 0;
    if (!((w[0] >= 'A' && w[0] <= 'Z') || (w[0] >= 'a' && w[0] <= 'z') || w[0] == '_'))
        return 0;
    while ((w[i] >= 'A' && w[i] <= 'Z') || (w[i] >= 'a' && w[i] <= 'z') || w[i] == '_' ||
           (w[i] >= '0' && w[i] <= '9'))
        i++;
    return w[i] == '=' || (w[i] == '+' && w[i + 1] == '=');
}

static sh_node *parse_simple(lexer *L)
{
    sh_node *n = node(L, SH_CMD);
    int any = 0;
    for (;;) {
        if (parse_redir(L, &n->redirs)) {
            any = 1;
            continue;
        }
        if (L->tok != T_WORD)
            break;
        if (!n->words && is_assignment(L->word)) {
            append_word(&n->assigns, new_word(L, L->word));
        } else {
            append_word(&n->words, new_word(L, L->word));
        }
        any = 1;
        next(L);
    }
    if (!any) {
        fail(L, "syntax error", L->tok == T_EOF);
        return 0;
    }
    return n;
}

static void parse_trailing_redirs(lexer *L, sh_node *n)
{
    while (n && parse_redir(L, &n->redirs))
        ;
}

static sh_node *parse_if(lexer *L)
{
    sh_node *n = node(L, SH_IF);
    next(L); /* if / elif */
    n->a = parse_list(L, 0);
    expect_word(L, "then");
    n->b = parse_list(L, 0);
    skip_newlines(L);
    if (is_word(L, "elif")) {
        n->c = parse_if(L);
        return n; /* the elif consumed the fi */
    }
    if (is_word(L, "else")) {
        next(L);
        n->c = parse_list(L, 0);
    }
    expect_word(L, "fi");
    return n;
}

static sh_node *parse_loop(lexer *L, enum sh_kind k)
{
    sh_node *n = node(L, k);
    next(L);
    n->a = parse_list(L, 0);
    expect_word(L, "do");
    n->b = parse_list(L, 0);
    expect_word(L, "done");
    return n;
}

static sh_node *parse_for(lexer *L)
{
    sh_node *n = node(L, SH_FOR);
    next(L);
    if (L->tok != T_WORD) {
        fail(L, "for: a name is missing", L->tok == T_EOF);
        return n;
    }
    n->name = L->word;
    next(L);
    skip_newlines(L);
    if (is_word(L, "in")) {
        n->has_in = 1;
        next(L);
        while (L->tok == T_WORD) {
            append_word(&n->words, new_word(L, L->word));
            next(L);
        }
        if (L->tok == T_SEMI || L->tok == T_NEWLINE)
            next(L);
    } else if (L->tok == T_SEMI) {
        next(L);
    }
    expect_word(L, "do");
    n->a = parse_list(L, 0);
    expect_word(L, "done");
    return n;
}

static sh_node *parse_case(lexer *L)
{
    sh_node *n = node(L, SH_CASE);
    sh_case **tail = &n->cases;
    next(L);
    if (L->tok != T_WORD) {
        fail(L, "case: a word is missing", L->tok == T_EOF);
        return n;
    }
    append_word(&n->words, new_word(L, L->word));
    next(L);
    expect_word(L, "in");
    for (;;) {
        sh_case *c;
        skip_newlines(L);
        if (is_word(L, "esac")) {
            next(L);
            break;
        }
        if (L->tok == T_EOF) {
            fail(L, "case: esac is missing", 1);
            break;
        }
        c = (sh_case *)alloc(L->p, sizeof(sh_case));
        if (L->tok == T_LPAREN)
            next(L);
        for (;;) {
            if (L->tok != T_WORD) {
                fail(L, "case: a pattern is missing", L->tok == T_EOF);
                return n;
            }
            append_word(&c->patterns, new_word(L, L->word));
            next(L);
            if (L->tok != T_PIPE)
                break;
            next(L);
        }
        if (L->tok != T_RPAREN) {
            fail(L, "case: ) is missing", L->tok == T_EOF);
            return n;
        }
        next(L);
        c->body = parse_list(L, 0);
        *tail = c;
        tail = &c->next;
        skip_newlines(L);
        if (L->tok == T_DSEMI)
            next(L);
        else if (!is_word(L, "esac")) {
            fail(L, "case: ;; is missing", L->tok == T_EOF);
            return n;
        }
    }
    return n;
}

static sh_node *parse_command(lexer *L)
{
    sh_node *n;
    if (L->had_error)
        return 0;
    if (L->tok == T_LPAREN) {
        n = node(L, SH_SUBSHELL);
        next(L);
        n->a = parse_list(L, 0);
        skip_newlines(L);
        if (L->tok != T_RPAREN) {
            fail(L, ") is missing", L->tok == T_EOF);
            return n;
        }
        next(L);
        parse_trailing_redirs(L, n);
        return n;
    }
    if (is_word(L, "{")) {
        n = node(L, SH_GROUP);
        next(L);
        n->a = parse_list(L, 0);
        expect_word(L, "}");
        parse_trailing_redirs(L, n);
        return n;
    }
    if (is_word(L, "if")) {
        n = parse_if(L);
        parse_trailing_redirs(L, n);
        return n;
    }
    if (is_word(L, "while") || is_word(L, "until")) {
        n = parse_loop(L, is_word(L, "while") ? SH_WHILE : SH_UNTIL);
        parse_trailing_redirs(L, n);
        return n;
    }
    if (is_word(L, "for")) {
        n = parse_for(L);
        parse_trailing_redirs(L, n);
        return n;
    }
    if (is_word(L, "case")) {
        n = parse_case(L);
        parse_trailing_redirs(L, n);
        return n;
    }
    /* name ( ) body: a function */
    if (L->tok == T_WORD && !L->quoted && L->s[L->pos] == '(' && L->s[L->pos + 1] == ')') {
        n = node(L, SH_FUNC);
        n->name = L->word;
        L->pos += 2;
        next(L);
        skip_newlines(L);
        n->a = parse_command(L);
        return n;
    }
    return parse_simple(L);
}

static sh_node *parse_pipeline(lexer *L)
{
    int not = 0;
    sh_node *n;
    if (is_word(L, "!")) {
        not = 1;
        next(L);
    }
    n = parse_command(L);
    while (!L->had_error && L->tok == T_PIPE) {
        sh_node *p = node(L, SH_PIPE);
        next(L);
        skip_newlines(L);
        p->a = n;
        p->b = parse_command(L);
        n = p;
    }
    if (not) {
        sh_node *x = node(L, SH_NOT);
        x->a = n;
        n = x;
    }
    return n;
}

static sh_node *parse_and_or(lexer *L)
{
    sh_node *n = parse_pipeline(L);
    while (!L->had_error && (L->tok == T_AND || L->tok == T_OR)) {
        sh_node *p = node(L, L->tok == T_AND ? SH_AND : SH_OR);
        next(L);
        skip_newlines(L);
        p->a = n;
        p->b = parse_pipeline(L);
        n = p;
    }
    return n;
}

/* A list: and-or's separated by ; & or newlines, up to a word that ends
 * it (inside a compound) or the end of input (top). */
static sh_node *parse_list(lexer *L, int top)
{
    sh_node *first = 0, **link = &first;
    skip_newlines(L);
    while (!L->had_error && !at_list_end(L)) {
        sh_node *item = parse_and_or(L), *wrap;
        if (L->had_error)
            break;
        if (L->tok == T_AMP) {
            wrap = node(L, SH_BG);
            next(L);
        } else {
            wrap = node(L, SH_SEQ);
            if (L->tok == T_SEMI || L->tok == T_NEWLINE)
                next(L);
            else if (!at_list_end(L)) {
                fail(L, "syntax error", 0);
                break;
            }
        }
        wrap->a = item;
        *link = wrap;
        link = &wrap->b;
        skip_newlines(L);
    }
    if (top && !L->had_error && L->tok != T_EOF)
        fail(L, "syntax error", 0);
    return first;
}

void sh_parse_text(sh_parse *p, const char *text)
{
    lexer L;
    memset(p, 0, sizeof(*p));
    memset(&L, 0, sizeof(L));
    L.p = p;
    L.s = text;
    next(&L);
    p->tree = parse_list(&L, 1);
    if (!L.had_error && L.pending)
        fail(&L, "here-document not ended", 1);
}

/* ---- dump ------------------------------------------------------------------ */

typedef struct out {
    char *b;
    int n, max;
} out;

static void put(out *o, const char *s)
{
    while (*s && o->n < o->max - 1)
        o->b[o->n++] = *s++;
    o->b[o->n] = 0;
}

static void dump(out *o, const sh_node *n);

static void dump_words(out *o, const sh_word *w)
{
    for (; w; w = w->next) {
        put(o, " ");
        put(o, w->text);
    }
}

static void dump_redirs(out *o, const sh_redir *r)
{
    static const char *const ops[] = { "<", ">", ">>", "<&", ">&", "&>", "<<" };
    char fd[4];
    for (; r; r = r->next) {
        put(o, " [");
        fd[0] = (char)('0' + r->fd);
        fd[1] = 0;
        put(o, fd);
        put(o, ops[r->kind]);
        if (r->kind == SH_R_HEREDOC && r->quoted)
            put(o, "'");
        put(o, r->target ? r->target : "?");
        put(o, "]");
    }
}

static void dump(out *o, const sh_node *n)
{
    static const char *const names[] = {
        "cmd", "pipe", "and", "or", "seq", "bg", "not", "sub", "group", "if", "while", "until",
        "for", "case", "func"
    };
    const sh_case *c;
    if (!n) {
        put(o, "()");
        return;
    }
    put(o, "(");
    put(o, names[n->kind]);
    switch (n->kind) {
    case SH_CMD:
        if (n->assigns) {
            put(o, " {");
            dump_words(o, n->assigns);
            put(o, " }");
        }
        dump_words(o, n->words);
        break;
    case SH_FOR:
        put(o, " ");
        put(o, n->name ? n->name : "?");
        if (n->has_in) {
            put(o, " in");
            dump_words(o, n->words);
        }
        put(o, " ");
        dump(o, n->a);
        break;
    case SH_CASE:
        dump_words(o, n->words);
        for (c = n->cases; c; c = c->next) {
            put(o, " (");
            dump_words(o, c->patterns);
            put(o, " ->");
            put(o, " ");
            dump(o, c->body);
            put(o, ")");
        }
        break;
    case SH_FUNC:
        put(o, " ");
        put(o, n->name);
        put(o, " ");
        dump(o, n->a);
        break;
    default:
        put(o, " ");
        dump(o, n->a);
        if (n->b || n->kind == SH_PIPE || n->kind == SH_AND || n->kind == SH_OR ||
            n->kind == SH_IF || n->kind == SH_WHILE || n->kind == SH_UNTIL) {
            put(o, " ");
            dump(o, n->b);
        }
        if (n->kind == SH_IF && n->c) {
            put(o, " ");
            dump(o, n->c);
        }
        break;
    }
    dump_redirs(o, n->redirs);
    put(o, ")");
}

int sh_dump(const sh_node *n, char *buf, int max)
{
    out o;
    o.b = buf;
    o.n = 0;
    o.max = max;
    if (max > 0)
        buf[0] = 0;
    dump(&o, n);
    return o.n;
}

/* ---- copy ----------------------------------------------------------------- */

static char *copy_str(sh_parse *p, const char *s, int *bad)
{
    char *r;
    if (!s)
        return 0;
    r = dup_n(p, s, (long)strlen(s));
    if (!r)
        *bad = 1;
    return r;
}

static sh_word *copy_words(sh_parse *p, const sh_word *w, int *bad)
{
    sh_word *head = 0, **tail = &head;
    for (; w && !*bad; w = w->next) {
        sh_word *c = (sh_word *)alloc(p, sizeof(sh_word));
        if (!c) {
            *bad = 1;
            break;
        }
        c->text = copy_str(p, w->text, bad);
        c->next = 0;
        *tail = c;
        tail = &c->next;
    }
    return head;
}

static sh_node *copy_node(sh_parse *p, const sh_node *n, int *bad)
{
    sh_node *c;
    const sh_redir *r;
    const sh_case *k;
    sh_redir **rt;
    sh_case **kt;
    if (!n || *bad)
        return 0;
    c = (sh_node *)alloc(p, sizeof(sh_node));
    if (!c) {
        *bad = 1;
        return 0;
    }
    *c = *n;
    c->a = copy_node(p, n->a, bad);
    c->b = copy_node(p, n->b, bad);
    c->c = copy_node(p, n->c, bad);
    c->words = copy_words(p, n->words, bad);
    c->assigns = copy_words(p, n->assigns, bad);
    c->name = copy_str(p, n->name, bad);
    c->redirs = 0;
    for (r = n->redirs, rt = &c->redirs; r && !*bad; r = r->next) {
        sh_redir *d = (sh_redir *)alloc(p, sizeof(sh_redir));
        if (!d) {
            *bad = 1;
            break;
        }
        *d = *r;
        d->target = copy_str(p, r->target, bad);
        d->next = 0;
        *rt = d;
        rt = &d->next;
    }
    c->cases = 0;
    for (k = n->cases, kt = &c->cases; k && !*bad; k = k->next) {
        sh_case *d = (sh_case *)alloc(p, sizeof(sh_case));
        if (!d) {
            *bad = 1;
            break;
        }
        d->patterns = copy_words(p, k->patterns, bad);
        d->body = copy_node(p, k->body, bad);
        d->next = 0;
        *kt = d;
        kt = &d->next;
    }
    return c;
}

void sh_parse_copy(const sh_node *n, sh_parse *out)
{
    int bad = 0;
    memset(out, 0, sizeof(*out));
    out->tree = copy_node(out, n, &bad);
    if (bad) {
        sh_parse_free(out);
        out->tree = 0;
    }
}
