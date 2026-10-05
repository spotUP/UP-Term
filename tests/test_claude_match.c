/* What C:Claude's Grep, Glob and WebFetch tools match and convert with
 * (ledger A4 WP2): the regular expressions (claude/regex.c), the file name
 * patterns (claude/glob.c) and a web page as Markdown (claude/html.c). */
#include <stdlib.h>
#include "harness.h"
#include "../claude/regex.h"
#include "../claude/glob.h"
#include "../claude/html.h"
#include "../claude/json.h"
#include "../claude/util.h"

/* the match of pat in s as "start-end", "none", or "error: ..." */
static const char *m(const char *pat, int flags, const char *s)
{
    static char out[200];
    char err[120];
    long ms = -1, me = -1;
    cl_re *re = re_compile(pat, flags, err, sizeof(err));
    int r;
    if (!re) {
        strcpy(out, "error: ");
        strcat(out, err);
        return out;
    }
    r = re_search(re, s, (long)strlen(s), 0, &ms, &me);
    re_free(re);
    if (r != 1)
        return "none";
    cl_ltoa(ms, out);
    strcat(out, "-");
    cl_ltoa(me, out + strlen(out));
    return out;
}

static void test_regex(void)
{
    /* literals, classes, anchors */
    CHECK_STR(m("needle", 0, "a needle here"), "2-8");
    CHECK_STR(m("Needle", 0, "a needle"), "none");
    CHECK_STR(m("Needle", RE_ICASE, "a NEEDLE"), "2-8");
    CHECK_STR(m("(?i)needle", 0, "NEEDLE"), "0-6");
    CHECK_STR(m("a.c", 0, "xabcx"), "1-4");
    CHECK_STR(m("a.c", 0, "a\nc"), "none");
    CHECK_STR(m("a.c", RE_DOTALL, "a\nc"), "0-3");
    CHECK_STR(m("^Set", 0, "x\nSetPatch"), "2-5");
    CHECK_STR(m("^Set", 0, "xSetPatch"), "none");
    CHECK_STR(m("QUIET$", 0, "SetPatch QUIET\nnext"), "9-14");
    CHECK_STR(m("QUIET$", 0, "SetPatch QUIET\r\n"), "9-14");
    CHECK_STR(m("[0-9]+", 0, "rev 42.1"), "4-6");
    CHECK_STR(m("[^a-z ]", 0, "abc D"), "4-5");
    CHECK_STR(m("[[:upper:]][[:digit:]]", 0, "x A1"), "2-4");
    CHECK_STR(m("[]a]", 0, "x]"), "1-2");
    CHECK_STR(m("[a-]", 0, "x-"), "1-2");
    CHECK_STR(m("\\d{2,3}", 0, "a1b1234"), "3-6");
    CHECK_STR(m("\\w+", 0, "  foo_1 "), "2-7");
    CHECK_STR(m("\\s\\S", 0, "ab c"), "2-4");
    CHECK_STR(m("[\\d.]+", 0, "v1.25x"), "1-5");
    CHECK_STR(m("\\bint\\b", 0, "print int"), "6-9");
    CHECK_STR(m("\\Bnt", 0, "int"), "1-3");
    /* alternation, groups, repetitions: leftmost first, greedy */
    CHECK_STR(m("cat|dog", 0, "hotdog cat"), "3-6");
    CHECK_STR(m("(ab)+", 0, "xababab"), "1-7");
    CHECK_STR(m("(?:ab)+?", 0, "xababab"), "1-7");
    CHECK_STR(m("a*", 0, "bbb"), "0-0");
    CHECK_STR(m("colou?r", 0, "the color"), "4-9");
    CHECK_STR(m("x{3}", 0, "xxxxx"), "0-3");
    CHECK_STR(m("x{2,}", 0, "axxxx"), "1-5");
    CHECK_STR(m("a{,2}", 0, "a{,2}"), "0-5");      /* not a repetition: literal */
    CHECK_STR(m("\\.info$", 0, "Disk.info"), "4-9");
    CHECK_STR(m("\\x41\\x{42}", 0, "zAB"), "1-3");
    CHECK_STR(m("\\t", 0, "a\tb"), "1-2");
    /* code points: UTF-8 as one character, Latin-1 bytes as one each */
    CHECK_STR(m("Gr.\xc3\x9f" "e", 0, "Gr\xc3\xbc\xc3\x9f" "e"), "0-7");
    CHECK_STR(m("Gr..e", 0, "Gr\xfc\xdf" "e"), "0-5");
    CHECK_STR(m("[\xc3\xa4\xc3\xb6\xc3\xbc]", 0, "M\xc3\xbcll"), "1-3");
    CHECK_STR(m("\xc3\x9c", RE_ICASE, "\xc3\xbc"), "0-2");
    /* linear time: the classic exponential pattern is instant */
    CHECK_STR(m("(a|a)*b", 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac"), "none");
    CHECK_STR(m("(x+x+)+y", 0, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"), "none");
    /* refused with a reason */
    CHECK_STR(m("(a", 0, "a"), "error: a ( without its )");
    CHECK_STR(m("a)", 0, "a"), "error: a ) without its (");
    CHECK_STR(m("[a", 0, "a"), "error: a [ without its ]");
    CHECK_STR(m("*a", 0, "a"), "error: a repetition with nothing before it");
    CHECK_STR(m("(a)\\1", 0, "aa"), "error: back references (\\1) are not supported");
    CHECK_STR(m("(?=a)", 0, "a"), "error: look-around ((?=, (?!, (?<) is not supported");
    CHECK_STR(m("[z-a]", 0, "a"), "error: a range out of order in a bracket expression");
    CHECK_STR(m("a{1,99999}", 0, "a"), "error: a {m,n} repetition out of range");
    /* a search from an offset, and the same compiled pattern reused */
    {
        char err[80];
        long ms, me;
        cl_re *re = re_compile("o", 0, err, sizeof(err));
        CHECK(re != 0);
        if (re) {
            CHECK_INT(re_search(re, "foo boo", 7, 2, &ms, &me), 1);
            CHECK_INT(ms, 2);
            CHECK_INT(re_search(re, "foo boo", 7, 3, &ms, &me), 1);
            CHECK_INT(ms, 5);
            CHECK_INT(re_search(re, "xyz", 3, 0, &ms, &me), 0);
            re_free(re);
        }
    }
}

static void test_glob(void)
{
    char o[200];
    CHECK(glob_match("*.c", "main.c"));
    CHECK(glob_match("*.c", "MAIN.C"));
    CHECK(!glob_match("*.c", "src/main.c"));
    CHECK(glob_match("**/*.c", "main.c"));
    CHECK(glob_match("**/*.c", "src/deep/main.c"));
    CHECK(glob_match("src/**", "src/a/b"));
    CHECK(glob_match("src/**/x.h", "src/x.h"));
    CHECK(!glob_match("src/**/x.h", "lib/x.h"));
    CHECK(glob_match("?.txt", "a.txt"));
    CHECK(!glob_match("?.txt", "ab.txt"));
    CHECK(glob_match("[a-c]*", "beta"));
    CHECK(!glob_match("[!a-c]*", "beta"));
    CHECK(glob_match("*.{c,h}", "x.h"));
    CHECK(glob_match("{Makefile,*.mk}", "Makefile"));
    CHECK(glob_match("{src,lib}/*.{c,s}", "lib/a.s"));
    CHECK(!glob_match("*.{c,h}", "x.s"));
    CHECK(glob_match("a\\*", "a*"));
    CHECK_INT(glob_depth("*.c"), 0);
    CHECK_INT(glob_depth("a/b/*.c"), 2);
    CHECK_INT(glob_depth("**/*.c"), -1);
    CHECK(glob_wild("#?.info"));
    CHECK(!glob_wild("Startup-Sequence"));
    /* the AmigaDOS forms */
    CHECK_INT(glob_from_amiga("#?.info", o, sizeof(o)), 0);
    CHECK_STR(o, "*.info");
    CHECK_INT(glob_from_amiga("(a|b)#?.c", o, sizeof(o)), 0);
    CHECK_STR(o, "{a,b}*.c");
    CHECK_INT(glob_from_amiga("x'#y", o, sizeof(o)), 0);
    CHECK_STR(o, "x\\#y");
    CHECK_INT(glob_from_amiga("~(#?.info)", o, sizeof(o)), -1);
    CHECK_INT(glob_from_amiga("#a", o, sizeof(o)), -1);
}

static const char *md(const char *html)
{
    static jw w;
    jw_free(&w);
    jw_init(&w);
    html_to_md(html, (long)strlen(html), &w, 0);
    return w.p ? w.p : "";
}

static void test_html(void)
{
    CHECK_STR(md("<h1>Title</h1><p>One  two\n three.</p><p>Four</p>"), "# Title\n\nOne two three.\n\nFour");
    CHECK_STR(md("<p>a <b>bold</b> and <em>it</em> and <code>x()</code></p>"), "a **bold** and *it* and `x()`");
    CHECK_STR(md("<a href=\"https://x.org/?a=1&amp;b=2\">link</a>"), "[link](https://x.org/?a=1&b=2)");
    CHECK_STR(md("<a href=\"#top\">top</a>"), "top");
    CHECK_STR(md("<ul><li>a</li><li>b</li></ul><ol><li>c</li><li>d</li></ol>"), "- a\n- b\n\n1. c\n2. d");
    CHECK_STR(md("<pre>int x;\n  y();</pre><p>after</p>"), "```\nint x;\n  y();\n```\n\nafter");
    CHECK_STR(md("<script>var x = '<p>';</script><style>p{}</style>text<!-- <p>no</p> -->"), "text");
    CHECK_STR(md("&lt;tag&gt; &amp; &#65;&#x42; &auml; &nbsp;x &bogus; 5 < 6"), "<tag> & AB \xc3\xa4 x &bogus; 5 < 6");
    CHECK_STR(md("<table><tr><th>a</th><th>b</th></tr><tr><td>1</td><td>2</td></tr></table>"), "a | b\n1 | 2");
    CHECK_STR(md("line<br>next<hr>end"), "line\nnext\n\n---\n\nend");
    CHECK_STR(md("<img src=x alt=\"A picture\"> text"), "A picture text");
    CHECK_STR(md("<div>a</div><div>b</div>"), "a\nb");
    CHECK_STR(md("Gr\xfc\xdf" "e"), "Gr\xfc\xdf" "e");
    /* the cut */
    {
        jw w;
        jw_init(&w);
        html_to_md("<p>0123456789</p>", 17, &w, 4);
        CHECK_STR(w.p, "0123");
        jw_free(&w);
    }
    md("");
}

void suite_claude_match(void)
{
    test_regex();
    test_glob();
    test_html();
}
