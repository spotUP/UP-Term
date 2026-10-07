/* vsh's parser: every construct to its tree (sh_dump's S-expressions). */
#include "harness.h"
#include "../shell/sh_parse.h"

static void parses(const char *text, const char *want)
{
    sh_parse p;
    char buf[512];
    sh_parse_text(&p, text);
    if (p.error) {
        CHECK_STR(p.error, "(no error)");
    } else {
        sh_parse copy;
        sh_dump(p.tree, buf, sizeof(buf));
        CHECK_STR(buf, want);
        /* every tree also copies whole: the copy outlives the parse
         * (ASan catches a pointer left into the freed arena) */
        sh_parse_copy(p.tree, &copy);
        sh_parse_free(&p);
        CHECK_INT(copy.tree != 0 || !*text, 1);
        sh_dump(copy.tree, buf, sizeof(buf));
        CHECK_STR(buf, want);
        sh_parse_free(&copy);
        return;
    }
    sh_parse_free(&p);
}

static void fails(const char *text, int incomplete)
{
    sh_parse p;
    sh_parse_text(&p, text);
    CHECK_INT(p.error != 0, 1);
    CHECK_INT(p.incomplete, incomplete);
    sh_parse_free(&p);
}

static void simple_commands_and_quoting(void)
{
    parses("", "()");
    parses("list", "(seq (cmd list))");
    parses("echo 'a b' \"c $D\" e\\ f # comment", "(seq (cmd echo 'a b' \"c $D\" e\\ f))");
    parses("A=1 B=x cmd arg", "(seq (cmd { A=1 B=x } cmd arg))");
    parses("echo $(ls -l) ${HOME}/x `date`", "(seq (cmd echo $(ls -l) ${HOME}/x `date`))");
    parses("echo a\\\nb", "(seq (cmd echo ab))");       /* backslash-newline joins */
    parses("echo 'a\\\nb'", "(seq (cmd echo 'a\\\nb'))"); /* not inside single quotes */
    parses("echo \"it's a\\\nb\"", "(seq (cmd echo \"it's ab\"))"); /* ' inside \"...\" */
}

static void array_assignment_words(void)
{
    parses("a=(x y z)", "(seq (cmd { a=(x y z) }))");
    parses("a+=(1 \"2 3\")", "(seq (cmd { a+=(1 \"2 3\") }))");
    parses("a[1]=v", "(seq (cmd { a[1]=v }))");
    parses("m[a b]=v", "(seq (cmd { m[a b]=v }))");
    parses("a[2]+=v cmd", "(seq (cmd { a[2]+=v } cmd))");
    parses("declare -A m=([k]=1 [j k]=2)", "(seq (cmd declare -A m=([k]=1 [j k]=2)))");
    parses("a=(\n x\n y\n)", "(seq (cmd { a=(\n x\n y\n) }))");
    fails("a=(x y", 1);
}

static void lists_pipes_and_logic(void)
{
    parses("a | b | c", "(seq (pipe (pipe (cmd a) (cmd b)) (cmd c)))");
    parses("a && b || c", "(seq (or (and (cmd a) (cmd b)) (cmd c)))");
    parses("a; b\nc", "(seq (cmd a) (seq (cmd b) (seq (cmd c))))");
    parses("a & b", "(bg (cmd a) (seq (cmd b)))");
    parses("! a | b", "(seq (not (pipe (cmd a) (cmd b))))");
    parses("a |\n b", "(seq (pipe (cmd a) (cmd b)))");
}

static void redirections(void)
{
    parses("cmd <in >out 2>err", "(seq (cmd cmd [0<in] [1>out] [2>err]))");
    parses("cmd 12>out 10<in 9>&10 {v}>f 3>&- 4<>rw 5>|c 6<<<w &>>ap", "(seq (cmd cmd [12>out] [10<in] [9>&10] [{v}>f] [3>&-] [4<>rw] [5>|c] [6<<<w] [1&>>ap]))");
    parses("cmd >>log 2>&1 &>both", "(seq (cmd cmd [1>>log] [2>&1] [1&>both]))");
    parses("cat <<EOF\nhello $X\nEOF\necho after",
           "(seq (cmd cat [0<<hello $X\n]) (seq (cmd echo after)))");
    parses("cat <<'EOF'\n$literal\nEOF\n", "(seq (cmd cat [0<<'$literal\n]))");
}

static void compound_commands(void)
{
    parses("if a; then b; elif c; then d; else e; fi",
           "(seq (if (seq (cmd a)) (seq (cmd b)) (if (seq (cmd c)) (seq (cmd d)) (seq (cmd e)))))");
    parses("while a\ndo\n b\ndone", "(seq (while (seq (cmd a)) (seq (cmd b))))");
    parses("until a; do b; done", "(seq (until (seq (cmd a)) (seq (cmd b))))");
    parses("for f in *.c x; do cc $f; done", "(seq (for f in *.c x (seq (cmd cc $f))))");
    parses("for a; do echo $a; done", "(seq (for a (seq (cmd echo $a))))");
    parses("case $x in a|b) one;; *) two;; esac",
           "(seq (case $x ( a b -> (seq (cmd one))) ( * -> (seq (cmd two)))))");
    parses("(cd x; ls) >out", "(seq (sub (seq (cmd cd x) (seq (cmd ls))) [1>out]))");
    parses("{ a; b; }", "(seq (group (seq (cmd a) (seq (cmd b)))))");
    parses("f() { echo hi; }", "(seq (func f (group (seq (cmd echo hi)))))");
}

static void errors_and_continuation(void)
{
    fails("echo 'open", 1);     /* the line ends inside a quote: read another */
    fails("if a; then b", 1);   /* ... inside an if */
    fails("a |", 1);
    fails("cat <<EOF\nno end", 1);
    fails("a )", 0);            /* a plain error */
    fails("fi", 0);
}

void suite_sh_parse(void)
{
    simple_commands_and_quoting();
    array_assignment_words();
    lists_pipes_and_logic();
    redirections();
    compound_commands();
    errors_and_continuation();
}
