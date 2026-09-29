/* vsh's parser: POSIX sh grammar to a tree. Portable C89, no OS calls,
 * tested on the host (tests/test_sh_parse.c).
 *
 * Words keep their quoting as typed ('...', "...", \x); expansion
 * (sh_expand.c) interprets it, so "$A" and '$A' stay distinguishable. */
#ifndef SH_PARSE_H
#define SH_PARSE_H

typedef struct sh_word {
    char *text;
    struct sh_word *next;
} sh_word;

enum sh_redir_kind {
    SH_R_IN,      /* <    */
    SH_R_OUT,     /* >    */
    SH_R_APPEND,  /* >>   */
    SH_R_DUPIN,   /* <&   */
    SH_R_DUPOUT,  /* >&   */
    SH_R_BOTH,    /* &>   stdout and stderr to a file */
    SH_R_HEREDOC  /* <<   target is the document's text; quoted delimiter: no expansion */
};

typedef struct sh_redir {
    int fd;                 /* 0 in, 1 out, 2 err, ... */
    enum sh_redir_kind kind;
    char *target;           /* a word, or for << the document */
    int quoted;             /* << with a quoted delimiter: the document is literal */
    struct sh_redir *next;
} sh_redir;

enum sh_kind {
    SH_CMD,       /* words, assigns, redirs */
    SH_PIPE,      /* a | b */
    SH_AND,       /* a && b */
    SH_OR,        /* a || b */
    SH_SEQ,       /* a ; b  (b may be 0) */
    SH_BG,        /* a &    (then b, the rest of the list) */
    SH_NOT,       /* ! a */
    SH_SUBSHELL,  /* ( a ) + redirs */
    SH_GROUP,     /* { a; } + redirs */
    SH_IF,        /* if a then b else c (c: another SH_IF for elif, or the else list) */
    SH_WHILE,     /* while a do b */
    SH_UNTIL,     /* until a do b */
    SH_FOR,       /* for name in words do a (words 0 with no "in": "$@") */
    SH_CASE,      /* case words(the subject) in cases */
    SH_FUNC       /* name() a */
};

typedef struct sh_case {
    sh_word *patterns;      /* p1 | p2 ... */
    struct sh_node *body;
    struct sh_case *next;
} sh_case;

typedef struct sh_node {
    enum sh_kind kind;
    struct sh_node *a, *b, *c;
    sh_word *words;
    sh_word *assigns;       /* SH_CMD: NAME=value words before the command */
    sh_redir *redirs;
    char *name;             /* SH_FOR variable, SH_FUNC name */
    int has_in;             /* SH_FOR: an "in" list was given */
    sh_case *cases;
} sh_node;

/* A parse: the tree and everything it points to live in one arena. */
typedef struct sh_parse {
    sh_node *tree;          /* 0 for an empty line */
    const char *error;      /* 0, or what went wrong */
    int incomplete;         /* the input ends inside a construct: read another line */
    void *arena;
} sh_parse;

/* Parse a whole input (one or more lines). */
void sh_parse_text(sh_parse *p, const char *text);
void sh_parse_free(sh_parse *p);

/* The tree as an S-expression, for tests and `set -x`-style tracing:
 * (cmd a b), (pipe X Y), (and X Y), (if C T E) ... Written into out
 * (max bytes); returns the length. */
int  sh_dump(const sh_node *n, char *out, int max);

#endif
