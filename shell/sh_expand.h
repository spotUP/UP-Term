/* vsh's word expansion (POSIX 2.6): tilde, parameters, command
 * substitution, arithmetic, field splitting, globbing, quote removal.
 * Portable C89; the OS parts (running a command for $(...), listing a
 * directory for a glob) are callbacks, so it is tested on the host
 * (tests/test_sh_expand.c). */
#ifndef SH_EXPAND_H
#define SH_EXPAND_H

/* A list of strings (fields, directory names). */
typedef struct sh_list {
    char **v;
    int n, cap;
} sh_list;

void sh_list_add(sh_list *l, const char *s);
void sh_list_free(sh_list *l);

/* Shell variables: name=value with attributes. */
#define SH_ATTR_EXPORT   1
#define SH_ATTR_READONLY 2
#define SH_ATTR_INTEGER  4   /* every assignment is evaluated with sh_arith */
#define SH_ATTR_UPPER    8
#define SH_ATTR_LOWER    16
#define SH_ATTR_ARRAY    32  /* reserved for phase 2 */
#define SH_ATTR_ASSOC    64
#define SH_ATTR_NAMEREF  128
typedef struct sh_var {
    char *name, *value;
    unsigned short attr;    /* SH_ATTR_* */
    struct sh_var *next;
} sh_var;

typedef struct sh_ctx {
    sh_var *vars;
    sh_list args;           /* $1.. */
    char *arg0;             /* $0 */
    const char *flags;      /* $-: "i" in an interactive shell (not owned) */
    long status;            /* $? */
    long pid;               /* $$ */
    long last_bg;           /* $! (0: none yet) */
    int nocase;             /* globs match names without regard to case (Amiga filesystems) */
    /* $(cmd): run cmd, return its output (malloc'ed, the caller frees) */
    char *(*subst)(struct sh_ctx *c, const char *cmd);
    /* list the names in directory dir ("" = current) into out; 0 = ok */
    int (*listdir)(struct sh_ctx *c, const char *dir, sh_list *out);
    int nounset;            /* set -u: an unset parameter is an error */
    int noglob;             /* set -f */
    int allexport;          /* set -a: an assigned variable is exported */
    long *pstat;            /* PIPESTATUS: the last pipeline's stage statuses (owned) */
    int npstat;
    void *user;
} sh_ctx;

const char *sh_get(const sh_ctx *c, const char *name);
void sh_ltoa(long v, char *out);   /* decimal, no printf: out has 24 bytes */
int sh_set(sh_ctx *c, const char *name, const char *value);   /* 1: refused (readonly) */
int sh_unset(sh_ctx *c, const char *name);                    /* 1: refused (readonly) */
unsigned sh_attr(const sh_ctx *c, const char *name);          /* 0 when unset */
/* set and clear attribute bits (a variable that is unset is created empty for an
 * attribute other than readonly-less export, as bash's declare -x x does) */
void sh_attr_change(sh_ctx *c, const char *name, unsigned set, unsigned clear);
void sh_pstat(sh_ctx *c, const long *st, int n);               /* record PIPESTATUS */

/* bash's quoting of a word: SH_Q_SINGLE 'a b' (set, xtrace, ${x@Q}), SH_Q_BACKSLASH a\ b
 * (printf %q); empty is '', control characters make $'..'. malloc'ed. */
#define SH_Q_SINGLE 0
#define SH_Q_BACKSLASH 1
char *sh_quote(const char *s, int style);
void sh_export(sh_ctx *c, const char *name);
void sh_ctx_free(sh_ctx *c);

#define SH_NO_SPLIT 1   /* one field: assignments, redirection targets, case words */
#define SH_NO_GLOB  2

/* Expand one word into fields appended to out. 0 on success; -1 with
 * *err set (${x:?msg}, a bad substitution). */
int sh_expand(sh_ctx *c, const char *word, int flags, sh_list *out, const char **err);

/* The word with quotes removed and nothing expanded (here-document
 * delimiters, alias names). */
char *sh_unquote(const char *word);

/* Glob pattern match (* ? [a-z] [!x], backslash escapes). */
int sh_match(const char *pattern, const char *name, int nocase);

/* $((expr)): integer arithmetic with + - * / % ( ) unary - and variables. */
long sh_arith(sh_ctx *c, const char *expr, const char **err);

#endif
