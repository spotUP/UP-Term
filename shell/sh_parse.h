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
    SH_R_HEREDOC, /* <<   target is the document's text; quoted delimiter: no expansion */
    SH_R_HERESTR, /* <<<  target is a word; its expansion and a newline are the input */
    SH_R_RDWR,    /* <>   read and write, no truncation */
    SH_R_CLOBBER, /* >|   like > but ignores noclobber */
    SH_R_BOTHAPP, /* &>>  stdout and stderr appended to a file */
    SH_R_CLOSE    /* n>&- and n<&-: the stream is closed */
};

typedef struct sh_redir {
    int fd;                 /* 0 in, 1 out, 2 err, ... */
    char *var;              /* {var}> and {var}<: the variable that gets the descriptor (fd is unused), else 0 */
    enum sh_redir_kind kind;
    char *target;           /* a word, or for << the document */
    char *delim;            /* << : the delimiter word as written (quotes kept), for sh_unparse */
    int quoted;             /* << with a quoted delimiter: the document is literal */
    int strip;              /* <<- : the leading tabs of the document's lines are removed */
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
    SH_FUNC,      /* name() a */
    SH_ARITHCMD,  /* (( expr )): words is the one expression text */
    SH_FORARITH,  /* for (( init; cond; step )) a: words are the three texts (empty when left out) */
    SH_DBRACK     /* [[ ]]: name is && || ! ( with the operands in a and b, or the test operator ("" for a bare
                   * string, "-f", "==", "=~", "<" ...) with its one or two operand words in words */,
    SH_SELECT,    /* select name in words do a (as SH_FOR) */
    SH_TIME       /* time [-p] a: a is the pipeline (0: none), has_in is -p */
};

typedef struct sh_case {
    sh_word *patterns;      /* p1 | p2 ... */
    struct sh_node *body;
    int term;               /* 0 ;;   1 ;& (run the next body too)   2 ;;& (test the next patterns) */
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
    int line;               /* the line the command starts on (the text's first line is line0) */
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
/* the same with the text's first line numbered line0 (LINENO): a script read command by command */
void sh_parse_text_at(sh_parse *p, const char *text, int line0);
void sh_parse_free(sh_parse *p);

/* The tree as an S-expression, for tests and `set -x`-style tracing:
 * (cmd a b), (pipe X Y), (and X Y), (if C T E) ... Written into out
 * (max bytes); returns the length. */
int  sh_dump(const sh_node *n, char *out, int max);

/* The text of a command as bash prints it (jobs, DEBUG's BASH_COMMAND, declare -f, type): malloc'd, the
 * caller frees it; 0 when memory runs out. fn: bash's function layout (a ; ends the line, a group is
 * multi-line); sh_unparse_func prints `name () ` and the body, as declare -f does (no final newline). */
char *sh_unparse(const sh_node *n, int fn);
char *sh_unparse_func(const char *name, const sh_node *body);

/* A copy of the tree n (and everything it points to) in out's own arena,
 * independent of n's parse: a subshell process runs it after the parse
 * it came from is gone. out->tree is 0 when memory runs out. */
void sh_parse_copy(const sh_node *n, sh_parse *out);

#endif
