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
#define SH_ATTR_ARRAY    32  /* indexed array: the elements are in arr */
#define SH_ATTR_ASSOC    64
#define SH_ATTR_NAMEREF  128
#define SH_ATTR_NOVALUE  256 /* declared (declare -a x) and never assigned: declare -p prints no value */
/* An array is a sparse vector sorted by index (indexed) or by key (associative):
 * append is O(1), lookup a binary search. */
typedef struct sh_elem {
    long idx;               /* indexed arrays */
    char *key;              /* associative arrays */
    char *val;
} sh_elem;
typedef struct sh_arr {
    long n, cap;
    sh_elem *e;
} sh_arr;
/* sv is the scalar value (0 for an array: read it through sh_var_str, never directly) */
typedef struct sh_var {
    char *name, *sv;
    struct sh_arr *arr;
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
    /* a warning the store raises (a circular or too deep name reference); NULL: silent */
    void (*warn)(struct sh_ctx *c, const char *name, const char *msg);
    /* special variables (RANDOM, LINENO, FUNCNAME ...): refresh is called before a name is looked up,
     * to bring the store's copy up to date; on_assign after a scalar assignment succeeded. NULL: none */
    void (*refresh)(struct sh_ctx *c, const char *name);
    void (*on_assign)(struct sh_ctx *c, const char *name, const char *value);
    /* ${x@E}: the text with backslash escapes read as $'...' reads them; ${x@P}: as a prompt string
     * (bash's backslash escapes, then expansion); ${x@A} (flags_only 0) and ${x@a} (1): the declare
     * text or the attribute letters of a variable, whole = every element. malloc'ed; NULL: no hook */
    char *(*unescape)(struct sh_ctx *c, const char *s);
    char *(*prompt)(struct sh_ctx *c, const char *ps);
    char *(*declared)(struct sh_ctx *c, const char *name, int flags_only, int whole);
    void *user;
} sh_ctx;

const char *sh_get(const sh_ctx *c, const char *name);       /* an array: element 0 */
const char *sh_var_str(const sh_var *v);                     /* scalar value, or element 0; 0 when none */
sh_var *sh_lookup(const sh_ctx *c, const char *name);        /* namerefs followed (at most 8) */
sh_var *sh_lookup_raw(const sh_ctx *c, const char *name);    /* the variable itself, a reference too */
const char *sh_resolve(const sh_ctx *c, const char *name);   /* the name a reference leads to */
const char *sh_get_elem(sh_ctx *c, const char *name, const char *sub);
/* NAME=value, NAME[sub]=value (sub already expanded: arithmetic for an indexed array, the key for an
 * associative one), += with append; 1: refused (readonly, bad subscript) */
int sh_assign(sh_ctx *c, const char *name, const char *sub, const char *value, int append);
int sh_unset_elem(sh_ctx *c, const char *name, const char *sub);
/* the [@] values: a malloc'ed vector of pointers into the store (the caller frees the vector only) */
char **sh_values(const sh_ctx *c, const char *name, long *n);
void sh_keys(const sh_ctx *c, const char *name, sh_list *out);
long sh_next_index(const sh_ctx *c, const char *name);       /* last index + 1 (0: none) */
int sh_array_reset(sh_ctx *c, const char *name, int assoc);  /* NAME=(...) starts empty; 1: refused */
sh_var *sh_var_copy(const sh_var *v);                         /* deep copy, not linked */
sh_var *sh_var_save(const sh_ctx *c, const char *name);       /* copy of the raw variable; 0: unset */
void sh_var_restore(sh_ctx *c, const char *name, sh_var *saved); /* put back (0: unset), takes saved */
void sh_var_link(sh_ctx *c, sh_var *v);                       /* v replaces any variable of its name */
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
#define SH_Q_ALWAYS 2   /* 'x' even when x needs no quoting (${x@Q}, ${x@A}) */
char *sh_quote(const char *s, int style);
char *sh_dquote(const char *t);   /* "t" for declare -p: \ " $ ` escaped; $'..' with control characters */
void sh_export(sh_ctx *c, const char *name);
void sh_ctx_free(sh_ctx *c);

#define SH_NO_SPLIT 1   /* one field: assignments, redirection targets, case words */
#define SH_NO_GLOB  2

/* Expand one word into fields appended to out. 0 on success; -1 with
 * *err set (${x:?msg}, a bad substitution). */
int sh_expand(sh_ctx *c, const char *word, int flags, sh_list *out, const char **err);

/* End of the $( ) $(( ) ${ } construct at s[i], bounded by len: see sh_expand.c; -1 when unterminated */
long sh_skip_sub(const char *s, long i, long len, int dq);

/* The word with quotes removed and nothing expanded (here-document
 * delimiters, alias names). */
char *sh_unquote(const char *word);

/* Glob pattern match (* ? [a-z] [!x], backslash escapes). */
int sh_match(const char *pattern, const char *name, int nocase);

/* $((expr)): integer arithmetic with + - * / % ( ) unary - and variables. */
long sh_arith(sh_ctx *c, const char *expr, const char **err);

#endif
