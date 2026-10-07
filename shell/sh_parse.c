/* vsh's parser: POSIX sh grammar to a tree (see sh_parse.h). */
#include <stdlib.h>
#include <string.h>
#include "sh_parse.h"
#include "sh_expand.h" /* sh_skip_sub */

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
    T_LPAREN, T_RPAREN, T_LT, T_GT, T_DGT, T_LTAMP, T_GTAMP, T_AMPGT, T_DLT,
    T_SEMIAMP, T_DSEMIAMP, T_PIPEAMP, T_DLTDASH, T_TLT, T_AMPDGT, T_LTGT, T_GTPIPE
};

typedef struct heredoc {
    sh_redir *r;
    char *delim;
    int strip;
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
    char io_var[64];        /* {name} right before a redirection operator, else empty */
    heredoc *pending;       /* << documents to read at the next newline */
    int had_error;
    long tokpos;            /* where the current token starts */
    long lpos;              /* line counting: the text before lpos has lline-line0 newlines */
    int lline;
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
static int assign_head(const char *w);

/* the first n bytes of w are a plain NAME */
static int assign_head_name(const char *w, long n)
{
    long k;
    for (k = 0; k < n; k++)
        if (!((w[k] >= 'A' && w[k] <= 'Z') || (w[k] >= 'a' && w[k] <= 'z') || w[k] == '_' ||
              (k && w[k] >= '0' && w[k] <= '9')))
            return 0;
    return 1;
}

static long word_end(lexer *L, long i, int *quoted)
{
    const char *s = L->s;
    int depth;
    long start = i;
    for (;;) {
        char c = s[i];
        if (c == '[' && (i == start || assign_head_name(s + start, i - start))) {
            /* NAME[sub]= and, in an array list, [sub]=: the subscript may hold spaces */
            long j = i + 1;
            int d2 = 1;
            while (s[j] && d2) {
                if (s[j] == '"' || s[j] == '\'') {
                    char qc = s[j++];
                    while (s[j] && s[j] != qc)
                        j++;
                } else
                    d2 += s[j] == '[' ? 1 : s[j] == ']' ? -1 : 0;
                if (s[j])
                    j++;
            }
            if (!d2 && (s[j] == '=' || (s[j] == '+' && s[j + 1] == '='))) {
                i = j;
                *quoted = 1;
                continue;
            }
        }
        if (c == '(' && i > start && (s[i - 1] == '=') && assign_head(s + start) >= 0) {
            /* NAME=( ... ) and NAME+=( ... ): the list is part of the word */
            int h = assign_head(s + start);
            if (start + h + 1 == i || (s[start + h] == '+' && start + h + 2 == i)) {
                depth = 1;
                i++;
                while (s[i] && depth) {
                    if (s[i] == '\\' && s[i + 1])
                        i++;
                    else if (s[i] == '\'') {
                        for (i++; s[i] && s[i] != '\''; i++)
                            ;
                    } else if (s[i] == '"') {
                        for (i++; s[i] && s[i] != '"'; i++)
                            if (s[i] == '\\' && s[i + 1])
                                i++;
                    } else if (s[i] == '(')
                        depth++;
                    else if (s[i] == ')')
                        depth--;
                    if (s[i])
                        i++;
                }
                if (depth)
                    return -1;
                *quoted = 1;
                continue;
            }
        }
        if ((c == '<' || c == '>') && s[i + 1] == '(') {
            /* <( ) and >( ): process substitution, part of the word */
            i = sh_skip_sub(s, i, 0x7fffffffL, 0);
            if (i < 0)
                return -1;
            continue;
        }
        if (c == '(' && i > start && sh_get_extglob() && strchr("@?*+!", s[i - 1])) {
            /* extglob: @( ) ?( ) *( ) +( ) !( ) belongs to the word */
            long e = sh_skip_sub(s, i - 1, 0x7fffffffL, 0);
            if (e < 0)
                return -1;
            i = e;
            *quoted = 1;
            continue;
        }
        if (is_meta(c))
            break;
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
                else if (s[i] == '$' && (s[i + 1] == '(' || s[i + 1] == '{')) {
                    i = sh_skip_sub(s, i, 0x7fffffffL, 1);
                    if (i < 0)
                        return -1;
                    continue;
                }
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
        } else if (c == '$' && s[i + 1] == '\'') {
            /* $'...': backslash escapes, \' does not end it */
            *quoted = 1;
            for (i += 2; s[i] && s[i] != '\''; i++)
                if (s[i] == '\\' && s[i + 1])
                    i++;
            if (!s[i])
                return -1;
            i++;
        } else if (c == '$' && (s[i + 1] == '(' || s[i + 1] == '{')) {
            i = sh_skip_sub(s, i, 0x7fffffffL, 0);
            if (i < 0)
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
            long ls = i, ts = i;
            while (s[i] && s[i] != '\n')
                i++;
            if (h->strip)
                while (ts < i && s[ts] == '\t')
                    ts++;
            if ((long)(i - ts) == dlen && !strncmp(s + ts, h->delim, dlen)) {
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
        if (h->strip) {
            /* <<- : the leading tabs of every line of the document go */
            char *o = h->r->target, *q = o;
            int bol = 1;
            for (; *q; q++) {
                if (bol && *q == '\t')
                    continue;
                bol = *q == '\n';
                *o++ = *q;
            }
            *o = 0;
        }
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
    L->io_var[0] = 0;
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
    i = L->tokpos = L->pos;
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
        if (s[i + 1] == ';' && s[i + 2] == '&') {
            L->tok = T_DSEMIAMP;
            L->pos += 3;
        } else if (s[i + 1] == ';') {
            L->tok = T_DSEMI;
            L->pos += 2;
        } else if (s[i + 1] == '&') {
            L->tok = T_SEMIAMP;
            L->pos += 2;
        } else {
            L->tok = T_SEMI;
            L->pos++;
        }
        return;
    case '&':
        if (s[i + 1] == '&') {
            L->tok = T_AND;
            L->pos += 2;
        } else if (s[i + 1] == '>' && s[i + 2] == '>') {
            L->tok = T_AMPDGT;
            L->pos += 3;
        } else if (s[i + 1] == '>') {
            L->tok = T_AMPGT;
            L->pos += 2;
        } else {
            L->tok = T_AMP;
            L->pos++;
        }
        return;
    case '|':
        L->tok = s[i + 1] == '|' ? T_OR : s[i + 1] == '&' ? T_PIPEAMP : T_PIPE;
        L->pos += L->tok == T_PIPE ? 1 : 2;
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
        if (s[i + 1] == '(')
            break; /* <( ): a word */
        if (s[i + 1] == '<' && s[i + 2] == '<') {
            L->tok = T_TLT;
            L->pos += 3;
        } else if (s[i + 1] == '<' && s[i + 2] == '-') {
            L->tok = T_DLTDASH;
            L->pos += 3;
        } else if (s[i + 1] == '<') {
            L->tok = T_DLT;
            L->pos += 2;
        } else if (s[i + 1] == '&') {
            L->tok = T_LTAMP;
            L->pos += 2;
        } else if (s[i + 1] == '>') {
            L->tok = T_LTGT;
            L->pos += 2;
        } else {
            L->tok = T_LT;
            L->pos++;
        }
        return;
    case '>':
        if (s[i + 1] == '(')
            break; /* >( ): a word */
        if (s[i + 1] == '>') {
            L->tok = T_DGT;
            L->pos += 2;
        } else if (s[i + 1] == '&') {
            L->tok = T_GTAMP;
            L->pos += 2;
        } else if (s[i + 1] == '|') {
            L->tok = T_GTPIPE;
            L->pos += 2;
        } else {
            L->tok = T_GT;
            L->pos++;
        }
        return;
    default:
        break;
    }
    /* {name} right before < or >: the descriptor goes to the variable */
    if (s[i] == '{') {
        long k = i + 1;
        if ((s[k] >= 'a' && s[k] <= 'z') || (s[k] >= 'A' && s[k] <= 'Z') || s[k] == '_') {
            while ((s[k] >= 'a' && s[k] <= 'z') || (s[k] >= 'A' && s[k] <= 'Z') || s[k] == '_' || (s[k] >= '0' && s[k] <= '9'))
                k++;
            if (s[k] == '}' && (s[k + 1] == '<' || s[k + 1] == '>') && s[k + 2] != '(' && k - i - 1 < (long)sizeof(L->io_var)) {
                char name[64];
                memcpy(name, s + i + 1, (size_t)(k - i - 1));
                name[k - i - 1] = 0;
                L->pos = k + 1;
                next(L);
                strcpy(L->io_var, name);
                return;
            }
        }
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

/* the line the current token is on */
static int tok_line(lexer *L)
{
    for (; L->lpos < L->tokpos; L->lpos++)
        if (L->s[L->lpos] == '\n')
            L->lline++;
    return L->lline;
}

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
    if (L->tok == T_EOF || L->tok == T_RPAREN || L->tok == T_DSEMI || L->tok == T_SEMIAMP ||
        L->tok == T_DSEMIAMP)
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
        t != T_DLT && t != T_DLTDASH && t != T_TLT && t != T_AMPDGT && t != T_LTGT && t != T_GTPIPE)
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
    case T_AMPDGT: r->kind = SH_R_BOTHAPP; break;
    case T_LTGT: r->kind = SH_R_RDWR; break;
    case T_GTPIPE: r->kind = SH_R_CLOBBER; break;
    case T_TLT: r->kind = SH_R_HERESTR; break;
    default: r->kind = SH_R_HEREDOC; break;
    }
    r->strip = t == T_DLTDASH;
    if (L->io_var[0]) {
        r->var = (char *)alloc(L->p, (long)strlen(L->io_var) + 1);
        if (r->var)
            strcpy(r->var, L->io_var);
    }
    r->fd = fd >= 0 ? fd : (r->kind == SH_R_IN || r->kind == SH_R_DUPIN || r->kind == SH_R_HEREDOC ||
                            r->kind == SH_R_HERESTR || r->kind == SH_R_RDWR) ? 0 : 1;
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
        r->delim = dup_n(L->p, d, (long)strlen(d));
        o = d;
        for (k = 0; d[k]; k++)
            if (d[k] != '\'' && d[k] != '"' && d[k] != '\\')
                *o++ = d[k];
        *o = 0;
        h->r = r;
        h->delim = d;
        h->strip = r->strip;
        for (tail = &L->pending; *tail; tail = &(*tail)->next)
            ;
        *tail = h;
    } else {
        r->target = L->word;
        if ((r->kind == SH_R_DUPIN || r->kind == SH_R_DUPOUT) && !strcmp(r->target, "-") && !L->quoted)
            r->kind = SH_R_CLOSE;
    }
    next(L);
    while (*list)
        list = &(*list)->next;
    *list = r;
    return 1;
}

/* the end of the NAME or NAME[sub] that starts an assignment word, or -1 */
static int assign_head(const char *w)
{
    int i = 0, d;
    if (!((w[0] >= 'A' && w[0] <= 'Z') || (w[0] >= 'a' && w[0] <= 'z') || w[0] == '_'))
        return -1;
    while ((w[i] >= 'A' && w[i] <= 'Z') || (w[i] >= 'a' && w[i] <= 'z') || w[i] == '_' ||
           (w[i] >= '0' && w[i] <= '9'))
        i++;
    if (w[i] == '[') {
        for (d = 1, i++; w[i] && d; i++)
            d += w[i] == '[' ? 1 : w[i] == ']' ? -1 : 0;
        if (d)
            return -1;
    }
    return i;
}

static int is_assignment(const char *w)
{
    int i = assign_head(w);
    return i >= 0 && (w[i] == '=' || (w[i] == '+' && w[i + 1] == '='));
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

static long dparen_end(lexer *L, long from, long *semi, int *ns);
static char *arena_text(lexer *L, long from, long to);

static sh_node *parse_for(lexer *L)
{
    sh_node *n = node(L, SH_FOR);
    next(L);
    if (L->tok == T_LPAREN && L->s[L->pos] == '(') {
        long semi[2], e, b = L->pos + 1;
        int ns;
        e = dparen_end(L, b, semi, &ns);
        if (e < 0 || ns != 2) {
            fail(L, "for: (( init; cond; step )) is malformed", e == -2);
            return n;
        }
        n->kind = SH_FORARITH;
        append_word(&n->words, new_word(L, arena_text(L, b, semi[0])));
        append_word(&n->words, new_word(L, arena_text(L, semi[0] + 1, semi[1])));
        append_word(&n->words, new_word(L, arena_text(L, semi[1] + 1, e)));
        L->pos = e + 2;
        next(L);
        if (L->tok == T_SEMI)
            next(L);
        skip_newlines(L);
        expect_word(L, "do");
        n->a = parse_list(L, 0);
        expect_word(L, "done");
        return n;
    }
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
        if (L->tok == T_DSEMI || L->tok == T_SEMIAMP || L->tok == T_DSEMIAMP) {
            c->term = L->tok == T_SEMIAMP ? 1 : L->tok == T_DSEMIAMP ? 2 : 0;
            next(L);
        } else if (!is_word(L, "esac")) {
            fail(L, "case: ;; is missing", L->tok == T_EOF);
            return n;
        }
    }
    return n;
}

/* the position after "( )" (blanks allowed) right after the current word, 0 when it is not there */
static long parens_after(lexer *L)
{
    long k = L->pos;
    while (L->s[k] == ' ' || L->s[k] == '\t')
        k++;
    if (L->s[k] != '(')
        return 0;
    k++;
    while (L->s[k] == ' ' || L->s[k] == '\t')
        k++;
    return L->s[k] == ')' ? k + 1 : 0;
}

/* ((: s[from..) follows the two opening parentheses. The index of the first ) of the closing )) when the
 * text up to it is balanced (so it is an arithmetic command and not two subshells), -1 when it is not,
 * -2 at the end of the text. semi[0..1] (when given) get the ; at depth 0, *ns how many there are. */
static long dparen_end(lexer *L, long from, long *semi, int *ns)
{
    const char *s = L->s;
    long i;
    int depth = 0;
    if (ns)
        *ns = 0;
    for (i = from; s[i]; i++) {
        char ch = s[i];
        if (ch == '\\' && s[i + 1])
            i++;
        else if (ch == '\'' || ch == '"') {
            for (i++; s[i] && s[i] != ch; i++)
                if (ch == '"' && s[i] == '\\' && s[i + 1])
                    i++;
            if (!s[i])
                return -2;
        } else if (ch == '(')
            depth++;
        else if (ch == ')') {
            if (depth)
                depth--;
            else
                return s[i + 1] == ')' ? i : -1;
        } else if (ch == ';' && !depth && semi && ns && *ns < 2)
            semi[(*ns)++] = i;
        else if (ch == ';' && !depth && ns)
            (*ns)++;
    }
    return -2;
}

static char *arena_text(lexer *L, long from, long to)
{
    char *t = (char *)alloc(L->p, to - from + 1);
    if (t) {
        memcpy(t, L->s + from, (size_t)(to - from));
        t[to - from] = 0;
    }
    return t;
}

/* ---- [[ ]] ------------------------------------------------------------------------ */

static sh_node *db_or(lexer *L);

static int db_binop(const char *w)
{
    static const char *const ops[] = {"==", "=", "!=", "=~", "-eq", "-ne", "-lt", "-le", "-gt", "-ge", "-nt", "-ot",
                                      "-ef", 0};
    int i;
    for (i = 0; ops[i]; i++)
        if (!strcmp(w, ops[i]))
            return 1;
    return 0;
}

static sh_node *db_leaf(lexer *L, const char *op, char *a, char *b)
{
    sh_node *n = node(L, SH_DBRACK);
    if (!n)
        return n;
    n->name = (char *)op;
    n->words = new_word(L, a);
    if (b)
        append_word(&n->words, new_word(L, b));
    return n;
}

/* the right side of =~: the text up to a blank at parenthesis depth 0 (parentheses, | and blanks inside
 * ( ) belong to the regular expression) */
static char *db_regex_word(lexer *L)
{
    const char *s = L->s;
    long i = L->pos, from;
    int depth = 0;
    while (s[i] == ' ' || s[i] == '\t')
        i++;
    from = i;
    for (; s[i]; i++) {
        char ch = s[i];
        if (ch == '\\' && s[i + 1])
            i++;
        else if (ch == '\'' || ch == '"') {
            for (i++; s[i] && s[i] != ch; i++)
                if (ch == '"' && s[i] == '\\' && s[i + 1])
                    i++;
            if (!s[i])
                break;
        } else if (ch == '$' && (s[i + 1] == '(' || s[i + 1] == '{')) {
            long e = sh_skip_sub(s, i, (long)strlen(s), 0);
            if (e < 0)
                break;
            i = e - 1;
        } else if (ch == '(')
            depth++;
        else if (ch == ')') {
            if (!depth)
                break;
            depth--;
        } else if ((ch == ' ' || ch == '\t' || ch == '\n') && !depth)
            break;
    }
    L->pos = i;
    return arena_text(L, from, i);
}

static sh_node *db_primary(lexer *L)
{
    sh_node *n;
    char *first;
    int fq;
    const char *op = 0;
    if (L->tok == T_LPAREN) {
        n = node(L, SH_DBRACK);
        n->name = "(";
        next(L);
        n->a = db_or(L);
        if (L->tok != T_RPAREN)
            fail(L, "[[: ) is missing", L->tok == T_EOF);
        else
            next(L);
        return n;
    }
    if (L->tok != T_WORD || is_word(L, "]]")) {
        fail(L, "[[: a word is missing", L->tok == T_EOF);
        return db_leaf(L, "", "", 0);
    }
    first = L->word;
    fq = L->quoted;
    next(L);
    if (L->tok == T_LT)
        op = "<";
    else if (L->tok == T_GT)
        op = ">";
    else if (L->tok == T_WORD && !L->quoted && db_binop(L->word))
        op = L->word;
    if (op) {
        char *rhs;
        if (!strcmp(op, "=~"))
            rhs = db_regex_word(L);
        else {
            next(L);
            if (L->tok != T_WORD || is_word(L, "]]")) {
                fail(L, "[[: a word is missing after the operator", L->tok == T_EOF);
                return db_leaf(L, "", "", 0);
            }
            rhs = L->word;
        }
        n = db_leaf(L, op, first, rhs);
        next(L);
        return n;
    }
    if (!fq && first[0] == '-' && first[1] && !first[2] && L->tok == T_WORD && !is_word(L, "]]")) {
        char *a = L->word;
        n = db_leaf(L, first, a, 0);
        next(L);
        return n;
    }
    if (!fq && first[0] == '-' && first[1] && !first[2] && strchr("abcdefghknoprstuvwxzGLNORS", first[1]))
        fail(L, "[[: a unary operator needs an operand", L->tok == T_EOF);
    return db_leaf(L, "", first, 0);
}

static sh_node *db_not(lexer *L)
{
    if (is_word(L, "!")) {
        sh_node *n = node(L, SH_DBRACK);
        n->name = "!";
        next(L);
        n->a = db_not(L);
        return n;
    }
    return db_primary(L);
}

static sh_node *db_and(lexer *L)
{
    sh_node *l = db_not(L);
    while (!L->had_error && L->tok == T_AND) {
        sh_node *n = node(L, SH_DBRACK);
        n->name = "&&";
        next(L);
        skip_newlines(L);
        n->a = l;
        n->b = db_not(L);
        l = n;
    }
    return l;
}

static sh_node *db_or(lexer *L)
{
    sh_node *l = db_and(L);
    while (!L->had_error && L->tok == T_OR) {
        sh_node *n = node(L, SH_DBRACK);
        n->name = "||";
        next(L);
        skip_newlines(L);
        n->a = l;
        n->b = db_and(L);
        l = n;
    }
    return l;
}

static sh_node *parse_dbrack(lexer *L)
{
    sh_node *n;
    next(L);
    skip_newlines(L);
    n = db_or(L);
    skip_newlines(L);
    if (L->had_error)
        return n;
    if (!is_word(L, "]]")) {
        fail(L, "[[: ]] is missing", L->tok == T_EOF);
        return n;
    }
    next(L);
    parse_trailing_redirs(L, n);
    return n;
}

static sh_node *parse_command1(lexer *L);

static sh_node *parse_command(lexer *L)
{
    int line = tok_line(L);
    sh_node *n = parse_command1(L);
    if (n && !n->line)
        n->line = line;
    return n;
}

static sh_node *parse_command1(lexer *L)
{
    sh_node *n;
    if (L->had_error)
        return 0;
    if (L->tok == T_LPAREN && L->s[L->pos] == '(') {
        long e = dparen_end(L, L->pos + 1, 0, 0);
        if (e >= 0) {
            n = node(L, SH_ARITHCMD);
            n->words = new_word(L, arena_text(L, L->pos + 1, e));
            L->pos = e + 2;
            next(L);
            parse_trailing_redirs(L, n);
            return n;
        }
    }
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
    if (is_word(L, "[[")) {
        n = parse_dbrack(L);
        return n;
    }
    if (is_word(L, "coproc")) {
        /* coproc [NAME] compound-command, or coproc simple-command (NAME is COPROC) */
        n = node(L, SH_COPROC);
        next(L);
        if (L->tok == T_WORD && !L->quoted && L->word[0] && (L->word[0] < '0' || L->word[0] > '9') &&
            strspn(L->word, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == strlen(L->word)) {
            /* a name only when a compound command follows it; else the word starts the simple command */
            long keep = L->tokpos;
            char *nm = L->word;
            next(L);
            if (L->tok == T_LPAREN || is_word(L, "{") || is_word(L, "if") || is_word(L, "while") ||
                is_word(L, "until") || is_word(L, "for") || is_word(L, "select") || is_word(L, "case") ||
                is_word(L, "[[")) {
                n->name = nm;
            } else {
                L->pos = keep;
                next(L);
            }
        }
        n->a = parse_command(L);
        return n;
    }
    if (is_word(L, "for") || is_word(L, "select")) {
        int sel = is_word(L, "select");
        n = parse_for(L);
        if (sel && n->kind == SH_FOR)
            n->kind = SH_SELECT;
        parse_trailing_redirs(L, n);
        return n;
    }
    if (is_word(L, "case")) {
        n = parse_case(L);
        parse_trailing_redirs(L, n);
        return n;
    }
    /* function name { ... }, function name() { ... } */
    if (is_word(L, "function")) {
        long k;
        next(L);
        if (L->tok != T_WORD) {
            fail(L, "function: a name is missing", L->tok == T_EOF);
            return 0;
        }
        n = node(L, SH_FUNC);
        n->name = L->word;
        k = parens_after(L);
        if (k) {
            L->pos = k;
            next(L);
        } else
            next(L);
        skip_newlines(L);
        n->a = parse_command(L);
        return n;
    }
    /* name ( ) body: a function; blanks may stand before and inside the parentheses */
    if (L->tok == T_WORD && !L->quoted && parens_after(L)) {
        n = node(L, SH_FUNC);
        n->name = L->word;
        L->pos = parens_after(L);
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
    sh_node *n, *last;
    if (is_word(L, "time")) {
        /* time [-p] [--] pipeline: the pipeline (0: none) in a, -p in has_in */
        sh_node *t = node(L, SH_TIME);
        next(L);
        while (L->tok == T_WORD && !L->quoted && (!strcmp(L->word, "-p") || !strcmp(L->word, "--"))) {
            int dd = L->word[1] == '-';
            if (!dd)
                t->has_in = 1;
            next(L);
            if (dd)
                break;
        }
        if (L->tok == T_WORD || L->tok == T_LPAREN || is_word(L, "!"))
            t->a = parse_pipeline(L);
        return t;
    }
    if (is_word(L, "!")) {
        not = 1;
        next(L);
    }
    n = parse_command(L);
    last = n;
    while (!L->had_error && (L->tok == T_PIPE || L->tok == T_PIPEAMP)) {
        sh_node *p = node(L, SH_PIPE);
        if (L->tok == T_PIPEAMP && last) {
            /* a |& b is a 2>&1 | b */
            sh_redir **rl = &last->redirs, *r = (sh_redir *)alloc(L->p, sizeof(sh_redir));
            if (r) {
                r->kind = SH_R_DUPOUT;
                r->fd = 2;
                r->target = dup_n(L->p, "1", 1);
                while (*rl)
                    rl = &(*rl)->next;
                *rl = r;
            }
        }
        next(L);
        skip_newlines(L);
        p->a = n;
        p->b = parse_command(L);
        last = p->b;
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
    sh_parse_text_at(p, text, 1);
}

void sh_parse_text_at(sh_parse *p, const char *text, int line0)
{
    lexer L;
    memset(p, 0, sizeof(*p));
    memset(&L, 0, sizeof(L));
    L.p = p;
    L.s = text;
    L.lline = line0;
    next(&L);
    p->tree = parse_list(&L, 1);
    if (!L.had_error && L.pending)
        fail(&L, "here-document not ended", 1);
    if (!L.had_error) { /* the text ends in a backslash-newline: the command goes on in the next line */
        size_t n = strlen(text), k = 0;
        while (n > k + 1 && text[n - 2 - k] == '\\')
            k++;
        if (n >= 2 && text[n - 1] == '\n' && (k & 1))
            fail(&L, "unexpected end of file", 1);
    }
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
    static const char *const ops[] = { "<", ">", ">>", "<&", ">&", "&>", "<<", "<<<", "<>", ">|", "&>>", ">&-" };
    char fd[16];
    for (; r; r = r->next) {
        long v = r->fd;
        int k = 0, j;
        char rev[16];
        put(o, " [");
        if (r->var) {
            put(o, "{");
            put(o, r->var);
            put(o, "}");
        } else {
            do {
                rev[k++] = (char)('0' + v % 10);
                v /= 10;
            } while (v > 0 && k < 15);
            for (j = 0; j < k; j++)
                fd[j] = rev[k - 1 - j];
            fd[k] = 0;
            put(o, fd);
        }
        put(o, ops[r->kind]);
        if (r->kind == SH_R_HEREDOC && r->quoted)
            put(o, "'");
        if (r->kind != SH_R_CLOSE)
            put(o, r->target ? r->target : "?");
        put(o, "]");
    }
}

static void dump(out *o, const sh_node *n)
{
    static const char *const names[] = {
        "cmd", "pipe", "and", "or", "seq", "bg", "not", "sub", "group", "if", "while", "until",
        "for", "case", "func", "arith", "forarith", "dbrack", "select", "time", "coproc"
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
    case SH_ARITHCMD:
        dump_words(o, n->words);
        break;
    case SH_DBRACK:
        put(o, " ");
        put(o, n->name && n->name[0] ? n->name : "str");
        dump_words(o, n->words);
        if (n->a) {
            put(o, " ");
            dump(o, n->a);
        }
        if (n->b) {
            put(o, " ");
            dump(o, n->b);
        }
        break;
    case SH_FORARITH:
        dump_words(o, n->words);
        put(o, " ");
        dump(o, n->a);
        break;
    case SH_TIME:
        put(o, n->has_in ? " -p " : " ");
        dump(o, n->a);
        break;
    case SH_COPROC:
        put(o, " ");
        put(o, n->name ? n->name : "COPROC");
        put(o, " ");
        dump(o, n->a);
        break;
    case SH_SELECT:
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
        d->delim = copy_str(p, r->delim, bad);
        d->var = copy_str(p, r->var, bad);
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
        d->term = k->term;
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

/* ---- unparse: a command as bash prints it (print_cmd.c) ------------------------ */

typedef struct unp {
    char *b;
    long n, cap;
    int bad;
    int indent, infn, skip;
    const sh_redir *hd[32];
    int nhd;
} unp;

static void up_add(unp *u, const char *s, long len)
{
    if (u->bad)
        return;
    if (u->n + len + 1 > u->cap) {
        long nc = u->cap ? u->cap * 2 : 256;
        char *nb;
        while (nc < u->n + len + 1)
            nc *= 2;
        nb = (char *)realloc(u->b, (size_t)nc);
        if (!nb) {
            u->bad = 1;
            return;
        }
        u->b = nb;
        u->cap = nc;
    }
    memcpy(u->b + u->n, s, (size_t)len);
    u->n += len;
    u->b[u->n] = 0;
}

static void up_s(unp *u, const char *s)
{
    up_add(u, s, (long)strlen(s));
}

static void up_indent(unp *u)
{
    int i;
    for (i = 0; i < u->indent; i++)
        up_add(u, " ", 1);
}

static void up_newline(unp *u, const char *s)
{
    up_add(u, "\n", 1);
    up_indent(u);
    up_s(u, s);
}

/* ; unless the text already ends in & or a newline */
static void up_semicolon(unp *u)
{
    if (u->n > 0 && (u->b[u->n - 1] == '&' || u->b[u->n - 1] == '\n'))
        return;
    up_add(u, ";", 1);
}

static void up_cmd(unp *u, const sh_node *n);

/* a here-document's delimiter without its quotes */
static void up_delim_plain(unp *u, const char *d)
{
    for (; *d; d++)
        if (*d != '\'' && *d != '"' && *d != '\\')
            up_add(u, d, 1);
}

/* the documents waiting for the end of the line, then cstring (a lone ; is not printed) */
static void up_flush(unp *u, const char *cstring)
{
    int i;
    for (i = 0; i < u->nhd; i++) {
        const sh_redir *r = u->hd[i];
        up_add(u, "\n", 1);
        up_s(u, r->target ? r->target : "");
        up_delim_plain(u, r->delim ? r->delim : "");
        up_add(u, "\n", 1);
    }
    u->nhd = 0;
    if (cstring && cstring[0] && (cstring[0] != ';' || cstring[1]))
        up_s(u, cstring);
}

static void up_word(unp *u, const char *w)
{
    /* $'text' with no escape inside prints as 'text' (bash expands it while parsing) */
    if (w[0] == '$' && w[1] == '\'') {
        const char *e = strchr(w + 2, '\'');
        if (e && !e[1] && !memchr(w + 2, '\\', (size_t)(e - w - 2))) {
            up_s(u, w + 1);
            return;
        }
    }
    up_s(u, w);
}

static void up_words(unp *u, const sh_word *w, const char *sep)
{
    for (; w; w = w->next) {
        up_word(u, w->text);
        if (w->next)
            up_s(u, sep);
    }
}

static void up_redirs(unp *u, const sh_redir *r)
{
    char d[16];
    int first = 1;
    for (; r; r = r->next) {
        const char *op = ">";
        int dflt = r->fd == 1, dup = 0, file = 1;
        switch (r->kind) {
        case SH_R_IN: op = "<"; dflt = r->fd == 0; break;
        case SH_R_OUT: break;
        case SH_R_APPEND: op = ">>"; break;
        case SH_R_DUPIN: op = "<&"; dup = 1; break;
        case SH_R_DUPOUT: op = ">&"; dup = 1; break;
        case SH_R_BOTH: op = "&>"; dflt = 1; break;
        case SH_R_BOTHAPP: op = "&>>"; dflt = 1; break;
        case SH_R_HERESTR: op = "<<<"; dflt = r->fd == 0; break;
        case SH_R_RDWR: op = "<>"; dflt = r->fd == 0; break;
        case SH_R_CLOBBER: op = ">|"; break;
        case SH_R_CLOSE: op = ">&-"; dup = 1; file = 0; break;
        case SH_R_HEREDOC: op = r->strip ? "<<-" : "<<"; dflt = r->fd == 0; break;
        }
        if (!first)
            up_add(u, " ", 1);
        first = 0;
        if (r->var) {
            up_s(u, "{");
            up_s(u, r->var);
            up_s(u, "}");
        } else if (!dflt || dup) {
            {
                int v = r->fd, k = 0, j;
                char t[16];
                do {
                    t[k++] = (char)('0' + v % 10);
                    v /= 10;
                } while (v && k < 15);
                for (j = 0; j < k; j++)
                    d[j] = t[k - 1 - j];
                d[k] = 0;
            }
            up_s(u, d);
        }
        up_s(u, op);
        if (r->kind == SH_R_HEREDOC) {
            up_s(u, r->delim ? r->delim : "");
            if (u->nhd < 32)
                u->hd[u->nhd++] = r;
        } else if (file) {
            if (!dup)
                up_add(u, " ", 1);
            up_s(u, r->target);
        }
    }
}

static void up_dbrack(unp *u, const sh_node *n)
{
    const char *op = n->name ? n->name : "";
    if (!strcmp(op, "(")) {
        up_s(u, "( ");
        up_dbrack(u, n->a);
        up_s(u, " )");
    } else if (!strcmp(op, "!")) {
        up_s(u, "! ");
        up_dbrack(u, n->a);
    } else if (!strcmp(op, "&&") || !strcmp(op, "||")) {
        up_dbrack(u, n->a);
        up_add(u, " ", 1);
        up_s(u, op);
        up_add(u, " ", 1);
        up_dbrack(u, n->b);
    } else if (n->words->next) {
        up_s(u, n->words->text);
        up_add(u, " ", 1);
        up_s(u, op);
        up_add(u, " ", 1);
        up_s(u, n->words->next->text);
    } else {
        if (op[0]) {
            up_s(u, op);
            up_add(u, " ", 1);
        }
        up_s(u, n->words->text);
    }
}

/* the part of an arithmetic text from its first non-blank */
static const char *up_trim(const char *t)
{
    while (*t == ' ' || *t == '\t' || *t == '\n')
        t++;
    return t;
}

static void up_redirs_after(unp *u, const sh_node *n)
{
    if (n->redirs) {
        up_add(u, " ", 1);
        up_redirs(u, n->redirs);
    }
}

static void up_func(unp *u, const sh_node *n, int top)
{
    if (!top) {
        up_s(u, "function ");
    }
    up_s(u, n->name);
    up_s(u, " () \n");
    up_indent(u);
    up_s(u, "{ \n");
    u->infn++;
    u->indent += 4;
    u->skip = 0;
    up_cmd(u, n->a && n->a->kind == SH_GROUP && !n->a->redirs ? n->a->a : n->a);
    up_flush(u, "");
    u->indent -= 4;
    u->infn--;
    up_newline(u, "}");
}

static void up_cmd(unp *u, const sh_node *n)
{
    const sh_word *w;
    const sh_case *k;
    if (!n)
        return;
    if (u->skip)
        u->skip--;
    else
        up_indent(u);
    switch (n->kind) {
    case SH_CMD: {
        int any = 0;
        for (w = n->assigns; w; w = w->next) {
            if (any)
                up_add(u, " ", 1);
            up_word(u, w->text);
            any = 1;
        }
        for (w = n->words; w; w = w->next) {
            if (any)
                up_add(u, " ", 1);
            up_word(u, w->text);
            any = 1;
        }
        if (n->redirs) {
            if (any)
                up_add(u, " ", 1);
            up_redirs(u, n->redirs);
        }
        break;
    }
    case SH_PIPE:
        u->skip++;
        up_cmd(u, n->a);
        up_flush(u, " |");
        up_add(u, " ", 1);
        u->skip++;
        up_cmd(u, n->b);
        break;
    case SH_AND:
    case SH_OR:
        u->skip++;
        up_cmd(u, n->a);
        up_flush(u, n->kind == SH_AND ? " &&" : " ||");
        up_add(u, " ", 1);
        u->skip++;
        up_cmd(u, n->b);
        break;
    case SH_SEQ:
        if (!n->b) {
            u->skip++;
            up_cmd(u, n->a);
            break;
        }
        u->skip++;
        up_cmd(u, n->a);
        if (u->nhd)
            up_flush(u, ";");
        else
            up_semicolon(u);
        if (u->infn)
            up_add(u, "\n", 1);
        else {
            up_add(u, " ", 1);
            u->skip++;
        }
        up_cmd(u, n->b);
        break;
    case SH_BG:
        u->skip++;
        up_cmd(u, n->a);
        if (u->nhd)
            up_flush(u, " &");
        else
            up_s(u, " &");
        if (n->b) {
            up_add(u, " ", 1);
            u->skip++;
            up_cmd(u, n->b);
        }
        break;
    case SH_NOT:
        up_s(u, "! ");
        u->skip++;
        up_cmd(u, n->a);
        break;
    case SH_SUBSHELL:
        up_s(u, "( ");
        u->skip++;
        up_cmd(u, n->a);
        up_s(u, " )");
        up_redirs_after(u, n);
        break;
    case SH_GROUP:
        up_s(u, "{ ");
        if (!u->infn)
            u->skip++;
        else {
            up_add(u, "\n", 1);
            u->indent += 4;
        }
        up_cmd(u, n->a);
        up_flush(u, "");
        if (u->infn) {
            up_add(u, "\n", 1);
            u->indent -= 4;
            up_indent(u);
        } else {
            up_semicolon(u);
            up_add(u, " ", 1);
        }
        up_s(u, "}");
        up_redirs_after(u, n);
        break;
    case SH_IF:
        up_s(u, "if ");
        u->skip++;
        up_cmd(u, n->a);
        up_semicolon(u);
        up_s(u, " then\n");
        u->indent += 4;
        up_cmd(u, n->b);
        u->indent -= 4;
        if (n->c) {
            up_semicolon(u);
            up_newline(u, "else\n");
            u->indent += 4;
            up_cmd(u, n->c);
            u->indent -= 4;
        }
        up_semicolon(u);
        up_newline(u, "fi");
        up_redirs_after(u, n);
        break;
    case SH_WHILE:
    case SH_UNTIL:
        up_s(u, n->kind == SH_WHILE ? "while " : "until ");
        u->skip++;
        up_cmd(u, n->a);
        up_semicolon(u);
        up_s(u, " do\n");
        u->indent += 4;
        up_cmd(u, n->b);
        up_flush(u, "");
        u->indent -= 4;
        up_semicolon(u);
        up_newline(u, "done");
        up_redirs_after(u, n);
        break;
    case SH_TIME:
        up_s(u, n->has_in ? "time -p " : "time ");
        if (n->a) {
            u->skip++;
            up_cmd(u, n->a);
        }
        break;
    case SH_COPROC:
        up_s(u, "coproc ");
        if (n->name) {
            up_s(u, n->name);
            up_s(u, " ");
        }
        u->skip++;
        up_cmd(u, n->a);
        break;
    case SH_SELECT:
    case SH_FOR:
        up_s(u, n->kind == SH_SELECT ? "select " : "for ");
        up_s(u, n->name);
        up_s(u, " in ");
        if (n->has_in)
            up_words(u, n->words, " ");
        else
            up_s(u, "\"$@\"");
        up_add(u, ";", 1);
        up_newline(u, "do\n");
        u->indent += 4;
        up_cmd(u, n->a);
        up_flush(u, "");
        u->indent -= 4;
        up_semicolon(u);
        up_newline(u, "done");
        up_redirs_after(u, n);
        break;
    case SH_FORARITH:
        up_s(u, "for ((");
        up_s(u, up_trim(n->words->text));
        up_s(u, "; ");
        up_s(u, up_trim(n->words->next->text));
        up_s(u, "; ");
        up_s(u, up_trim(n->words->next->next->text));
        up_s(u, "))");
        up_newline(u, "do\n");
        u->indent += 4;
        up_cmd(u, n->a);
        up_flush(u, "");
        u->indent -= 4;
        up_semicolon(u);
        up_newline(u, "done");
        up_redirs_after(u, n);
        break;
    case SH_CASE:
        up_s(u, "case ");
        up_word(u, n->words->text);
        up_s(u, " in ");
        u->indent += 4;
        for (k = n->cases; k; k = k->next) {
            up_newline(u, "");
            up_words(u, k->patterns, " | ");
            up_s(u, ")\n");
            u->indent += 4;
            up_cmd(u, k->body);
            u->indent -= 4;
            up_flush(u, "");
            up_newline(u, k->term == 1 ? ";&" : k->term == 2 ? ";;&" : ";;");
        }
        u->indent -= 4;
        up_newline(u, "esac");
        up_redirs_after(u, n);
        break;
    case SH_FUNC:
        up_func(u, n, 0);
        break;
    case SH_ARITHCMD:
        up_s(u, "(( ");
        {
            const char *t = up_trim(n->words->text);
            long len = (long)strlen(t);
            while (len && (t[len - 1] == ' ' || t[len - 1] == '\t' || t[len - 1] == '\n'))
                len--;
            up_add(u, t, len);
        }
        up_s(u, " ))");
        up_redirs_after(u, n);
        break;
    case SH_DBRACK:
        up_s(u, "[[ ");
        up_dbrack(u, n);
        up_s(u, " ]]");
        up_redirs_after(u, n);
        break;
    }
}

char *sh_unparse(const sh_node *n, int fn)
{
    unp u;
    memset(&u, 0, sizeof u);
    u.infn = fn;
    up_cmd(&u, n);
    up_flush(&u, "");
    if (u.bad) {
        free(u.b);
        return 0;
    }
    if (!u.b) {
        u.b = (char *)malloc(1);
        if (u.b)
            u.b[0] = 0;
    }
    return u.b;
}

char *sh_unparse_func(const char *name, const sh_node *body)
{
    unp u;
    sh_node f;
    memset(&u, 0, sizeof u);
    memset(&f, 0, sizeof f);
    f.kind = SH_FUNC;
    f.name = (char *)name;
    f.a = (sh_node *)body;
    up_func(&u, &f, 1);
    if (u.bad) {
        free(u.b);
        return 0;
    }
    return u.b;
}
