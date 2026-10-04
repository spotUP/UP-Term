/* view/md: Markdown fixtures -> the exact text (no colour, the layout),
 * a few -> the exact ANSI (colours, OSC 8), word wrap and table widths. */
#include <string.h>
#include "harness.h"
#include "../view/md.h"
#include "../view/hl_lex.h"

static char out[16384];
static long olen;

static void sink(void *u, const char *s, long n)
{
    (void)u;
    if (olen + n < (long)sizeof(out)) {
        memcpy(out + olen, s, n);
        olen += n;
        out[olen] = 0;
    }
}

static hl_theme theme;

/* doc rendered at width w: cs VW_UTF8 / VW_LATIN1, depth 0 (no colour) or 16 */
static const char *render(const char *doc, int w, int cs, int depth, int osc8)
{
    vw_out o;
    md_opts mo;
    hl_theme_builtin(&theme, "ansi");
    olen = 0;
    out[0] = 0;
    vo_init(&o, sink, 0, cs, depth, &theme);
    o.osc8 = osc8;
    mo.width = w;
    mo.urls = 1;
    CHECK_INT(md_render(doc, (long)strlen(doc), &mo, &o), 0);
    vo_flush(&o);
    return out;
}

static const char *plain(const char *doc, int w)
{
    return render(doc, w, VW_UTF8, 0, 0);
}

static void headings_and_paragraphs(void)
{
    CHECK_STR(plain("# Title\n\nSome *text* here.\n", 20),
              "Title\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
              "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95"
              "\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\nSome text here.\n");
    /* setext, the closing #s, and ### shown when there are no colours */
    CHECK_STR(plain("Sub\n---\n### Three ###\npara\nlazy\n", 10),
              "Sub\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
              "\xe2\x94\x80\xe2\x94\x80\n\n### Three\n\npara lazy\n");
    /* ASCII for a Latin-1 terminal */
    CHECK_STR(render("A\n=\n\n***\n", 5, VW_LATIN1, 0, 0), "A\n========\n\n--------\n"); /* 8: the narrowest */
}

static void wrapping(void)
{
    /* words move whole; blanks fold; a word longer than the line is cut */
    CHECK_STR(plain("one two  three four five\n", 9), "one two\nthree\nfour five\n");
    CHECK_STR(plain("abcdefghijklmnop qr\n", 8), "abcdefgh\nijklmnop\nqr\n");
    /* a hard break: two blanks or a backslash at the line's end */
    CHECK_STR(plain("a  \nb\\\nc\nd\n", 20), "a\nb\nc d\n");
    /* widths are the terminal's: é one column, a CJK character two */
    CHECK_STR(plain("caf\xc3\xa9 caf\xc3\xa9 \xe4\xb8\xad\xe4\xb8\xad\n", 10),
              "caf\xc3\xa9 caf\xc3\xa9\n\xe4\xb8\xad\xe4\xb8\xad\n");
}

static void inlines(void)
{
    CHECK_STR(plain("***a*** b *c **d** e* snake_case_name a * b * c\n", 80),
              "a b c d e snake_case_name a * b * c\n");
    CHECK_STR(plain("`a ``b`` c` and `` ` `` x\\*y\\_ &amp; &copy; &#65;&#x42; &bogus;\n", 80),
              "a ``b`` c and ` x*y_ & \xc2\xa9 AB &bogus;\n");
    CHECK_STR(plain("~~gone~~ ~kept~ un*bal\n", 80), "gone kept un*bal\n");
    /* links: inline, reference (case and blanks folded), autolink, a bare
     * URL without its full stop; an anchor shows no URL */
    CHECK_STR(plain("[docs](http://a.b/c \"T\") [Ref  Name][] <http://x.y> see https://e.f/g. [top](#top)\n\n"
                    "[ref name]: http://r.s\n", 200),
              "docs http://a.b/c Ref Name http://r.s http://x.y see https://e.f/g. top\n");
    /* images: their alt text; inline HTML: <br> breaks, <img> shows alt */
    CHECK_STR(plain("![Logo](l.png) [![CI](b.svg)](http://ci) a<br>b <kbd>K</kbd> <img src=\"x\" alt=\"Pic\">\n", 80),
              "Logo CI http://ci a\nb K Pic\n");
    /* a bracket that makes no link stays text */
    CHECK_STR(plain("[not a link] [x](\n", 80), "[not a link] [x](\n");
}

/* g n times */
static const char *rep(const char *g, int n)
{
    static char b[512];
    size_t k = strlen(g);
    int i;
    b[0] = 0;
    for (i = 0; i < n && (i + 1) * k < sizeof(b); i++)
        strcat(b, g);
    return b;
}

static void colours_and_links(void)
{
    char want[1024];
    /* the exact ANSI: the heading's class and rule, bold, a code span, an
     * OSC 8 link in the link class, then its URL (linked too) */
    strcpy(want, "\033[0;1;96mHi\033[0m\n\033[0;96m");
    strcat(want, rep("\xe2\x94\x80", 20));
    strcat(want, "\033[0m\n\n"
                 "\033[0;1mb\033[0m \033[0;33mc\033[0m \033[0;4;94m\033]8;;http://u\033\\l\033[0m\033]8;;\033\\ "
                 "\033[0;90m\033]8;;http://u\033\\http://u\033]8;;\033\\\033[0m\n");
    CHECK_STR(render("## Hi\n\n**b** `c` [l](http://u)\n", 20, VW_UTF8, 16, 1), want);
    /* without OSC 8 (another terminal, -L): the same, no hyperlink */
    CHECK_STR(render("<http://x.y>\n", 20, VW_UTF8, 16, 0), "\033[0;4;94mhttp://x.y\033[0m\n");
}

static void lists(void)
{
    /* tight: no blank lines; nested bullets change shape; numbers go on */
    CHECK_STR(plain("Intro\n- a\n- b\n  - c\n    long words here\n- d\n\n3. x\n1. y\n", 14),
              "Intro\n\n\xe2\x80\xa2 a\n\xe2\x80\xa2 b\n  \xe2\x97\x8b c long\n    words here\n\xe2\x80\xa2 d\n\n3. x\n4. y\n");
    /* loose: a blank line between items; a second paragraph in an item */
    CHECK_STR(plain("1. one\n\n2. two\n\n   more\n", 20), "1. one\n\n2. two\n\n   more\n");
    /* task items; an empty item still shows its marker */
    CHECK_STR(plain("- [ ] todo\n- [x] done\n-\n", 20), "\xe2\x80\xa2 [ ] todo\n\xe2\x80\xa2 [x] done\n\xe2\x80\xa2 \n");
    /* "- " after a paragraph line starts a list; "2." does not */
    CHECK_STR(plain("text\n2. not\n", 20), "text 2. not\n");
    CHECK_STR(render("* a\n  + b\n", 20, VW_LATIN1, 0, 0), "* a\n  o b\n");
}

static void quotes(void)
{
    CHECK_STR(plain("> quoted text that wraps\nlazy line\n>\n> - item\n\nafter\n", 14),
              "\xe2\x94\x82 quoted text\n\xe2\x94\x82 that wraps\n\xe2\x94\x82 lazy line\n\xe2\x94\x82\n"
              "\xe2\x94\x82 \xe2\x80\xa2 item\n\nafter\n");
    /* the blank line before a quote has no bar */
    CHECK_STR(plain("para\n\n> q\n", 20), "para\n\n\xe2\x94\x82 q\n");
}

static void code_blocks(void)
{
    /* fenced: indented by two, blank lines kept, trailing ones dropped */
    CHECK_STR(plain("```\na  b\n\nc\n\n```\nafter\n", 20), "  a  b\n\n  c\n\nafter\n");
    /* indented code, and a fence inside a list item */
    CHECK_STR(plain("    x = 1\n\n- item\n\n  ```sh\n  ls\n  ```\n", 20), "  x = 1\n\n\xe2\x80\xa2 item\n\n    ls\n");
    /* a long code line is cut, not wrapped at words */
    CHECK_STR(plain("```\nabcdefghij\n```\n", 8), "  abcdef\n  ghij\n");
    /* highlighted through hl_lex: the fence's language */
    CHECK_STR(render("```c\nint x; // c\n```\n", 40, VW_UTF8, 16, 0),
              "  \033[0;36mint\033[0m x; \033[0;3;90m// c\033[0m\n");
    /* an unknown language: plain */
    CHECK_STR(render("~~~ nolang\nint x;\n~~~\n", 40, VW_UTF8, 16, 0), "  int x;\n");
}

static void tables(void)
{
    /* natural widths, alignment, inline markup in cells */
    CHECK_STR(render("| a | bb |\n|:-:|--:|\n| **x** | 1 |\n| yyy | 22 |\n", 40, VW_LATIN1, 0, 0),
              "+-----+----+\n"
              "|  a  | bb |\n"
              "+-----+----+\n"
              "|  x  |  1 |\n"
              "| yyy | 22 |\n"
              "+-----+----+\n");
    /* too wide: the widest column is capped, its cell wraps */
    CHECK_STR(render("Name | Text\n--- | ---\nk | one two three four\n", 20, VW_LATIN1, 0, 0),
              "+------+-----------+\n"
              "| Name | Text      |\n"
              "+------+-----------+\n"
              "| k    | one two   |\n"
              "|      | three     |\n"
              "|      | four      |\n"
              "+------+-----------+\n");
    /* box drawing in UTF-8; an escaped pipe and a pipe in code stay in the cell */
    CHECK_STR(plain("|a\\|b|`c|d`|\n|-|-|\n", 40),
              "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xac\xe2\x94\x80\xe2\x94\x80"
              "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90\n"
              "\xe2\x94\x82 a|b \xe2\x94\x82 c|d \xe2\x94\x82\n"
              "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xbc\xe2\x94\x80\xe2\x94\x80"
              "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xa4\n"
              "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xb4\xe2\x94\x80\xe2\x94\x80"
              "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x98\n");
    /* a delimiter row whose column count differs: a paragraph */
    CHECK_STR(plain("a | b\n--- | --- | ---\n", 40), "a | b --- | --- | ---\n");
}

static void fit_columns(void)
{
    int nat[4], w[4];
    nat[0] = 5;
    nat[1] = 10;
    md_fit_columns(nat, 2, 20, w);
    CHECK_INT(w[0], 5);
    CHECK_INT(w[1], 10);
    /* 3 + 30 + 12 into 30: the cap is the largest that fits, the rest shared */
    nat[0] = 3;
    nat[1] = 30;
    nat[2] = 12;
    md_fit_columns(nat, 3, 30, w);
    CHECK_INT(w[0], 3);
    CHECK_INT(w[1] + w[2], 27);
    CHECK(w[1] >= 13 && w[2] <= 14);
    /* no room at all: every column keeps one */
    md_fit_columns(nat, 3, 0, w);
    CHECK_INT(w[0], 1);
    CHECK_INT(w[1], 1);
    CHECK_INT(w[2], 1);
}

static void html_and_refs(void)
{
    /* a README's centred logo block: the alt text, the tags gone */
    CHECK_STR(plain("<p align=\"center\">\n  <img src=\"logo.png\" alt=\"Logo\" width=\"200\">\n</p>\n\ntext\n", 40),
              "Logo\n\ntext\n");
    /* a block of tags only shows nothing, and leaves no gap */
    CHECK_STR(plain("<div>\n</div>\n\ntext\n", 40), "text\n");
    /* a definition in a paragraph is text; one after a blank line is not shown */
    CHECK_STR(plain("[a]: http://x\npara [a]\n", 40), "para a http://x\n");
    /* definitions straight after an HTML comment line (EmulatorJS's README
     * badges stayed "![x][Badge y]") */
    CHECK_STR(plain("see ![b][B l]\n\n<!-- Link Definitions -->\n[B l]: http://img\n", 40), "see b\n");
}

static void robust(void)
{
    /* odd input: no crash, every line accounted for */
    static const char *const docs[] = {
        "", "\n\n\n", "#", "######## seven", "> > > deep\n> > back", "- \n- \n  -", "```", "|", "|-|",
        "[", "]", "![", "**", "<", "&", "`", "\\", "1.", "1)", "* * *", "\t\tcode\ttabs",
        "- a\n\n\n- b\n  > q\n  > - c\n    ```\n    x", "<!-- open", "[a]: <b", "a\r\nb\r\n", 0
    };
    int i;
    for (i = 0; docs[i]; i++) {
        render(docs[i], 20, VW_UTF8, 16, 1);
        render(docs[i], 1, VW_LATIN1, 0, 0);
    }
    CHECK_STR(plain("a\r\nb\r\n", 20), "a b\n");
}

/* The stream (md_open / md_feed / md_close, C:Claude's answers, W32): the
 * same screen as the whole document, for every split of it into two feeds
 * and for a byte at a time; a block is drawn as soon as a later line
 * closes it, not at the end. */
static void streaming(void)
{
    static const char *const docs[] = {
        "# Plan\n\nRead **the** file, then:\n\n1. one\n2. two `x`\n\n```c\nint a = 1; /* c */\n```\n\n"
        "| a | b |\n|---|---|\n| 1 | 22 |\n\n> quoted\n> more\n\ntail\twith tab",
        "- [x] done\n- [ ] open\n\n---\nA [link](http://x.y) and ~~gone~~.\n", 0
    };
    static char whole[16384];
    int d;
    for (d = 0; docs[d]; d++) {
        long n = (long)strlen(docs[d]), cut;
        int ok = 1;
        strcpy(whole, render(docs[d], 30, VW_UTF8, 16, 1));
        for (cut = 0; cut <= n && ok; cut++) {
            vw_out o;
            md_opts mo;
            md *m;
            long k;
            olen = 0;
            out[0] = 0;
            vo_init(&o, sink, 0, VW_UTF8, 16, &theme);
            o.osc8 = 1;
            mo.width = 30;
            mo.urls = 1;
            m = md_open(&mo, &o);
            if (cut == n) {
                for (k = 0; k < n; k++)
                    md_feed(m, docs[d] + k, 1);
            } else {
                md_feed(m, docs[d], cut);
                md_feed(m, docs[d] + cut, n - cut);
            }
            CHECK_INT(md_close(m), 0);
            vo_flush(&o);
            if (strcmp(out, whole)) {
                ok = 0;
                CHECK_STR(out, whole);
            }
        }
    }
    /* drawn when closed: the heading at once, the paragraph only once a
     * blank line ends it */
    {
        vw_out o;
        md_opts mo;
        md *m;
        olen = 0;
        out[0] = 0;
        vo_init(&o, sink, 0, VW_UTF8, 0, &theme);
        mo.width = 20;
        mo.urls = 0;
        m = md_open(&mo, &o);
        md_feed(m, "### Head\nsome te", 16);
        vo_flush(&o);
        CHECK_STR(out, "### Head\n");
        CHECK(md_pending(m));
        md_feed(m, "xt\n", 3);
        vo_flush(&o);
        CHECK_STR(out, "### Head\n");
        md_feed(m, "\n", 1);
        vo_flush(&o);
        CHECK_STR(out, "### Head\n\nsome text\n");
        CHECK(!md_pending(m));
        CHECK_INT(md_close(m), 0);
    }
}

void suite_md(void)
{
    streaming();
    headings_and_paragraphs();
    wrapping();
    inlines();
    colours_and_links();
    lists();
    quotes();
    code_blocks();
    tables();
    fit_columns();
    html_and_refs();
    robust();
    hl_cleanup();
}
