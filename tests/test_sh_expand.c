/* vsh's word expansion: variables, quoting, splitting, substitution,
 * arithmetic and globbing, against a fake directory tree. */
#include <stdlib.h>
#include "harness.h"
#include "../shell/sh_expand.h"

static char *fake_subst(sh_ctx *c, const char *cmd)
{
    char *r = (char *)malloc(64);
    (void)c;
    if (!strcmp(cmd, "date"))
        strcpy(r, "Tue Sep 29\n\n");
    else if (!strcmp(cmd, "two words"))
        strcpy(r, "alpha beta\n");
    else
        strcpy(r, "?");
    return r;
}

/* A fake disk: "" (current) holds a.c b.c B.C readme .hidden src/;
 * "src" holds x.c y.h; "Work:" holds Projects */
static int n_lists; /* directory listings made */

static int fake_list(sh_ctx *c, const char *dir, sh_list *out)
{
    (void)c;
    n_lists++;
    if (!*dir) {
        sh_list_add(out, "a.c");
        sh_list_add(out, "b.c");
        sh_list_add(out, "B.C");
        sh_list_add(out, "readme");
        sh_list_add(out, ".hidden");
        sh_list_add(out, "src");
    } else if (!strcmp(dir, "src")) {
        sh_list_add(out, "x.c");
        sh_list_add(out, "y.h");
    } else if (!strcmp(dir, "Work:")) {
        sh_list_add(out, "Projects");
    } else {
        return -1;
    }
    return 0;
}

static sh_ctx *ctx(void)
{
    static sh_ctx c;
    sh_ctx_free(&c);
    memset(&c, 0, sizeof(c));
    c.subst = fake_subst;
    c.listdir = fake_list;
    c.status = 5;
    c.pid = 42;
    sh_set(&c, "HOME", "Work:home");
    sh_set(&c, "A", "one two");
    sh_set(&c, "E", "");
    sh_list_add(&c.args, "p1");
    sh_list_add(&c.args, "p 2");
    return &c;
}

/* The fields of word, joined by | for comparison. */
static const char *ex(sh_ctx *c, const char *word, int flags)
{
    static char buf[512];
    sh_list out;
    const char *err = 0;
    int i;
    memset(&out, 0, sizeof(out));
    buf[0] = 0;
    if (sh_expand(c, word, flags, &out, &err)) {
        strcpy(buf, "ERR ");
        strcat(buf, err ? err : "?");
    }
    for (i = 0; i < out.n; i++) {
        if (i)
            strcat(buf, "|");
        strcat(buf, out.v[i]);
    }
    sh_list_free(&out);
    return buf;
}

static int fields(sh_ctx *c, const char *word)
{
    sh_list o;
    const char *e;
    int n;
    memset(&o, 0, sizeof(o));
    sh_expand(c, word, 0, &o, &e);
    n = o.n;
    sh_list_free(&o);
    return n;
}

static void variables_and_quotes(void)
{
    sh_ctx *c = ctx();
    /* ${X#p} ${X##p} ${X%p} ${X%%p}: prefix and suffix removal */
    sh_set(c, "X", "a/b/c.txt");
    CHECK_STR(ex(c, "${X#*/}", 0), "b/c.txt");
    CHECK_STR(ex(c, "${X##*/}", 0), "c.txt");
    CHECK_STR(ex(c, "${X%/*}", 0), "a/b");
    CHECK_STR(ex(c, "${X%%/*}", 0), "a");
    CHECK_STR(ex(c, "${X%.txt}", 0), "a/b/c");
    CHECK_STR(ex(c, "${X#nomatch}", 0), "a/b/c.txt");
    CHECK_STR(ex(c, "\"${X##*/}\"", 0), "c.txt");
    CHECK_STR(ex(c, "${NOPE%x}", 0), "");
    CHECK_STR(ex(c, "$A", 0), "one|two");            /* unquoted: split */
    CHECK_STR(ex(c, "\"$A\"", 0), "one two");        /* quoted: one field */
    CHECK_STR(ex(c, "'$A'", 0), "$A");               /* single quotes: literal */
    CHECK_STR(ex(c, "x${A}y", 0), "xone|twoy");
    CHECK_STR(ex(c, "\\$A", 0), "$A");
    CHECK_STR(ex(c, "$E", 0), "");                   /* empty unquoted: no field */
    CHECK_STR(ex(c, "\"$E\"", 0), "");               /* ... quoted: one empty field */
    CHECK_INT(fields(c, "$E"), 0);
    CHECK_INT(fields(c, "\"$E\""), 1);
    CHECK_STR(ex(c, "$? $$ $#", SH_NO_SPLIT), "5 42 2");
    /* $-: the shell's flags, "i" when interactive (vshrc prints the banner
     * only then, not for vsh -c from vim or screen) */
    CHECK_STR(ex(c, "[$-]", SH_NO_SPLIT), "[]");
    c->flags = "i";
    CHECK_STR(ex(c, "[$-] [${-}]", SH_NO_SPLIT), "[i] [i]");
    CHECK_STR(ex(c, "\"$-\"", 0), "i");
    c->flags = 0;
    CHECK_STR(ex(c, "$1-$2", 0), "p1-p|2");          /* only the expansion splits */
    CHECK_STR(ex(c, "\"$@\"", 0), "p1|p 2");         /* "$@": one field each */
    CHECK_STR(ex(c, "\"$*\"", 0), "p1 p 2");
    CHECK_STR(ex(c, "${Z:-def ault}", 0), "def|ault");
    CHECK_STR(ex(c, "\"${Z:-def ault}\"", 0), "def ault");
    CHECK_STR(ex(c, "${E:-x}${E-y}", 0), "x");       /* :- takes empty as unset, - does not */
    CHECK_STR(ex(c, "${A:+set}", 0), "set");
    CHECK_STR(ex(c, "${#A}", 0), "7");
    CHECK_STR(ex(c, "${N:=new}$N", 0), "newnew");
    CHECK_STR(ex(c, "${Q:?}", 0), "ERR parameter not set");
    CHECK_STR(ex(c, "~/docs", 0), "Work:home/docs");
    CHECK_STR(ex(c, "\"~\"", 0), "~");
}

static void substitution_and_arithmetic(void)
{
    sh_ctx *c = ctx();
    CHECK_STR(ex(c, "$(date)", SH_NO_SPLIT), "Tue Sep 29");       /* trailing newlines go */
    CHECK_STR(ex(c, "$(two words)", 0), "alpha|beta");
    CHECK_STR(ex(c, "\"$(two words)\"", 0), "alpha beta");
    CHECK_STR(ex(c, "`date`", SH_NO_SPLIT), "Tue Sep 29");
    sh_set(c, "N", "7");
    CHECK_STR(ex(c, "$((1 + 2 * 3))", 0), "7");
    CHECK_STR(ex(c, "$(( (N - 1) / 2 ))", 0), "3");
    CHECK_STR(ex(c, "$(( $N % 4 ))", 0), "3");
    CHECK_STR(ex(c, "$((-5 + 2))", 0), "-3");
    CHECK_STR(ex(c, "$((1/0))", 0), "ERR arithmetic: division by zero");
}

static void globbing(void)
{
    sh_ctx *c = ctx();
    CHECK_STR(ex(c, "*.c", 0), "a.c|b.c");               /* sorted; no dot files */
    c->nocase = 1;
    CHECK_STR(ex(c, "*.c", 0), "B.C|a.c|b.c");           /* Amiga: case-insensitive names */
    c->nocase = 0;
    CHECK_STR(ex(c, "?.c", 0), "a.c|b.c");
    CHECK_STR(ex(c, "[ab].c", 0), "a.c|b.c");
    CHECK_STR(ex(c, "[!a].c", 0), "b.c");
    CHECK_STR(ex(c, "src/*.c", 0), "src/x.c");
    CHECK_STR(ex(c, "Work:P*", 0), "Work:Projects");
    CHECK_STR(ex(c, "*.none", 0), "*.none");             /* no match: the word stays */
    CHECK_STR(ex(c, "\"*.c\"", 0), "*.c");               /* quoted: no glob */
    CHECK_STR(ex(c, "*.c", SH_NO_GLOB), "*.c");
    CHECK_STR(ex(c, ".*", 0), ".hidden");
    CHECK_INT(sh_match("a*b?c", "aXXbYc", 0), 1);
    CHECK_INT(sh_match("a*b?c", "aXXbc", 0), 0);
    CHECK_INT(sh_match("\\*x", "*x", 0), 1);
    CHECK_INT(sh_match("[a-c]1", "b1", 0), 1);
}

/* "[" and "]" alone, as in [ $i -lt 9 ], are no patterns: a bracket
 * that does not close is literal (POSIX). vsh listed the directory for
 * every [ it ran -- 85 ms per loop turn on the rig. */
static void unclosed_bracket_is_literal(void)
{
    sh_ctx *c = ctx();
    n_lists = 0;
    CHECK_STR(ex(c, "[", 0), "[");
    CHECK_STR(ex(c, "]", 0), "]");
    CHECK_STR(ex(c, "a[b", 0), "a[b");
    CHECK_INT(n_lists, 0);
    CHECK_STR(ex(c, "[ab].c", 0), "a.c|b.c");  /* a closed one still globs */
    CHECK_INT(n_lists, 1);
}

static void typed_arrays(void)
{
    sh_ctx *c = ctx();
    CHECK_INT(sh_assign(c, "ta", "0", "x", 0), 0);
    CHECK_INT(sh_assign(c, "ta", "3", "y", 0), 0);
    CHECK_INT(sh_assign(c, "ta", "-1", "z", 0), 0);      /* the last element */
    CHECK_STR(ex(c, "${ta[0]}${ta[3]}", 0), "xz");
    CHECK_STR(ex(c, "${#ta[@]}", 0), "2");
    CHECK_STR(ex(c, "${!ta[@]}", 0), "0|3");
    CHECK_STR(ex(c, "$ta", 0), "x");                      /* $a is ${a[0]} */
    CHECK_INT(sh_unset_elem(c, "ta", "0"), 0);
    CHECK_STR(ex(c, "${ta[@]}", 0), "z");
    sh_set(c, "tn", "5");
    sh_attr_change(c, "tn", SH_ATTR_ARRAY, 0);            /* the scalar becomes element 0 */
    CHECK_STR(ex(c, "${tn[0]}", 0), "5");
    sh_attr_change(c, "tm", SH_ATTR_ASSOC, 0);
    CHECK_INT(sh_assign(c, "tm", "k k", "v", 0), 0);
    CHECK_STR(ex(c, "${tm[k k]}", 0), "v");
    sh_set(c, "tr", "ta");
    sh_attr_change(c, "tr", SH_ATTR_NAMEREF, 0);
    CHECK_STR(ex(c, "${tr[3]}", 0), "z");                 /* the reference leads to ta */
    CHECK_INT(sh_assign(c, "tr", "5", "w", 0), 0);
    CHECK_STR(ex(c, "${ta[5]}", 0), "w");
}

void suite_sh_expand(void)
{
    typed_arrays();
    unclosed_bracket_is_literal();
    variables_and_quotes();
    substitution_and_arithmetic();
    globbing();
    sh_ctx_free(ctx());
}
