/* view/hl_lex, hl_langs, hl_style, hl_view, vw_text: the token streams of
 * every language on small fixtures (and what spans lines), detection,
 * themes and SGR, the output's character sets, the line view. */
#include <string.h>
#include "harness.h"
#include "../view/hl_lex.h"
#include "../view/hl_style.h"
#include "../view/hl_view.h"
#include "../view/vw_text.h"

static char got[8192];
static long glen;

static void add(const char *s, long n)
{
    if (glen + n >= (long)sizeof(got) - 1)
        n = (long)sizeof(got) - 1 - glen;
    memcpy(got + glen, s, n);
    glen += n;
    got[glen] = 0;
}

/* plain text as it is, every other token as class[text] */
static void tok(void *u, int cls, const char *s, long n)
{
    (void)u;
    CHECK(n > 0);
    if (cls == HL_PLAIN) {
        add(s, n);
        return;
    }
    add(hl_class_names[cls], (long)strlen(hl_class_names[cls]));
    add("[", 1);
    add(s, n);
    add("]", 1);
}

/* the lines of text (split at '\n') through one state, lines joined by '\n' */
static const char *lex(const char *lang, const char *text)
{
    hl_state st;
    const hl_lang *L = hl_find(lang, (long)strlen(lang));
    const char *p = text;
    CHECK(L != 0);
    glen = 0;
    got[0] = 0;
    hl_begin(&st, L);
    for (;;) {
        const char *e = strchr(p, '\n');
        long n = e ? (long)(e - p) : (long)strlen(p);
        hl_line(&st, p, n, tok, 0);
        if (!e)
            break;
        add("\n", 1);
        p = e + 1;
    }
    return got;
}

static void c_family(void)
{
    CHECK_STR(lex("c", "int x = 0x1F; /* a */"), "type[int] x = number[0x1F]; comment[/* a */]");
    CHECK_STR(lex("c", "#include <stdio.h>"), "preproc[#include] string[<stdio.h>]");
    CHECK_STR(lex("c", "  #  define N 10"), "  preproc[#  define] N number[10]");
    CHECK_STR(lex("c", "printf(\"a\\\"b\\n\", 'x');"),
              "function[printf](string[\"a]escape[\\\"]string[b]escape[\\n]string[\"], string['x']);");
    /* a block comment over three lines, then code again */
    CHECK_STR(lex("c", "x = 1; /* one\ntwo\nthree */ return NULL;"),
              "x = number[1]; comment[/* one]\ncomment[two]\ncomment[three */] keyword[return] builtin[NULL];");
    /* C comments do not nest: the first close ends it */
    CHECK_STR(lex("c", "/* a /* b */ c */"), "comment[/* a /* b */] c */");
    /* a backslash-newline goes on with the string */
    CHECK_STR(lex("c", "s = \"ab\\\ncd\";"), "s = string[\"ab]escape[\\]\nstring[cd\"];");
    CHECK_STR(lex("c", "UBYTE *p = (UBYTE *)AllocVec(n, 0L); // x"),
              "type[UBYTE] *p = (type[UBYTE] *)function[AllocVec](n, number[0L]); comment[// x]");
    CHECK_STR(lex("c", "x = 1.5e-3f + .5;"), "x = number[1.5e-3f] + number[.5];");
}

static void asm68k(void)
{
    CHECK_STR(lex("asm", "start:\tmove.l\t#$dff000,a5\t; custom"),
              "label[start:]\tkeyword[move.l]\t#number[$dff000],builtin[a5]\tcomment[; custom]");
    CHECK_STR(lex("asm", "* Devpac comment"), "comment[* Devpac comment]");
    CHECK_STR(lex("asm", ".loop\tDBRA D0,.loop"), "label[.loop]\tkeyword[DBRA] builtin[D0],.loop");
    /* after the operands, a blank: the rest is a comment (vasm, Devpac) */
    CHECK_STR(lex("asm", "\tlea\t(a0,d0.w),a1  next entry"),
              "\tkeyword[lea]\t(builtin[a0],builtin[d0.w]),builtin[a1]  comment[next entry]");
    CHECK_STR(lex("asm", "\tdc.b\t'it''s', 0, %0101"),
              "\ttype[dc.b]\tstring['it]escape['']string[s'], number[0], number[%0101]");
    CHECK_STR(lex("asm", "\tSECTION code,CODE_C"), "\ttype[SECTION] code,CODE_C");
    CHECK_STR(lex("asm", "\tMYMACRO\t\\1,@17"), "\tfunction[MYMACRO]\tvariable[\\1],number[@17]");
}

static void amigae(void)
{
    /* E's comments nest */
    CHECK_STR(lex("e", "/* a /* b */ c */ PROC main()"), "comment[/* a /* b */ c */] keyword[PROC] function[main]()");
    CHECK_STR(lex("e", "/* one /* two\n*/ still */ x:=$FF -> rest"),
              "comment[/* one /* two]\ncomment[*/ still */] x:=number[$FF] comment[-> rest]");
    CHECK_STR(lex("e", "WriteF('n=\\d\\n', n)"), "builtin[WriteF](string['n=]escape[\\d\\n]string['], n)");
    CHECK_STR(lex("e", "DEF p:PTR TO LONG"), "keyword[DEF] p:type[PTR] keyword[TO] type[LONG]");
}

static void python(void)
{
    CHECK_STR(lex("python", "def f(x):  # c"), "keyword[def] function[f](x):  comment[# c]");
    CHECK_STR(lex("python", "s = \"\"\"one\ntwo \" still\nend\"\"\" + 'x'"),
              "s = string[\"\"\"one]\nstring[two \" still]\nstring[end\"\"\"] + string['x']");
    CHECK_STR(lex("python", "f'{a}\\n' r'\\d' b\"x\""), "string[f'{a}]escape[\\n]string['] string[r'\\d'] string[b\"x\"]");
    CHECK_STR(lex("python", "@app.route('/')"), "preproc[@app.route](string['/'])");
    CHECK_STR(lex("python", "return None if x else True"),
              "keyword[return] builtin[None] keyword[if] x keyword[else] builtin[True]");
}

static void shell(void)
{
    CHECK_STR(lex("sh", "echo \"$HOME/x\" 'no $y' # c"),
              "builtin[echo] string[\"]variable[$HOME]string[/x\"] string['no $y'] comment[# c]");
    CHECK_STR(lex("sh", "a#b ${#x} $(pwd)"), "a#b variable[${#x}] variable[$(pwd)]");
    CHECK_STR(lex("sh", "if [ -f x ]; then fi"), "keyword[if] [ -f x ]; keyword[then] keyword[fi]");
    /* a heredoc: its lines are text until the end word */
    CHECK_STR(lex("sh", "cat <<EOF\n$x # y\nEOF\necho"), "cat <<label[EOF]\nstring[$x # y]\nlabel[EOF]\nbuiltin[echo]");
    CHECK_STR(lex("sh", "f() {\n\tcat <<-'E'\n\tx\n\tE\n}"),
              "function[f]() {\n\tcat <<-label['E']\nstring[\tx]\n\tlabel[E]\n}");
    CHECK_STR(lex("sh", "x='multi\nline'"), "x=string['multi]\nstring[line']");
}

static void amigados(void)
{
    CHECK_STR(lex("amigados", ".KEY DIR/A,QUIET/S"), "preproc[.KEY DIR/A,QUIET/S]");
    CHECK_STR(lex("amigados", "If EXISTS <DIR>  ; test"), "keyword[If] type[EXISTS] variable[<DIR>]  comment[; test]");
    CHECK_STR(lex("amigados", "Copy \"{DIR}\" RAM: CLONE"), "builtin[Copy] string[\"{DIR}\"] RAM: type[CLONE]");
    CHECK_STR(lex("amigados", "Echo \"a*Nb*\"c\" $v"), "builtin[Echo] string[\"a]escape[*N]string[b]escape[*\"]string[c\"] variable[$v]");
    CHECK_STR(lex("amigados", "Copy >NIL: <in"), "builtin[Copy] >NIL: <in");
}

static void arexx(void)
{
    CHECK_STR(lex("arexx", "/* a /* b */ c */ say 'it''s' x"),
              "comment[/* a /* b */ c */] keyword[say] string['it]escape['']string[s'] x");
    CHECK_STR(lex("arexx", "IF Left(s,1) = 'x' THEN SAY 1"),
              "keyword[IF] builtin[Left](s,number[1]) = string['x'] keyword[THEN] keyword[SAY] number[1]");
}

static void lua(void)
{
    CHECK_STR(lex("lua", "local s = [[a]] -- c"), "keyword[local] s = string[[[a]]] comment[-- c]");
    /* long brackets with a level, over lines; a shorter close does not end them */
    CHECK_STR(lex("lua", "--[==[ a ]] b\nc ]==] x = 1"), "comment[--[==[ a ]] b]\ncomment[c ]==]] x = number[1]");
    CHECK_STR(lex("lua", "local function f(t) return nil end"),
              "keyword[local] keyword[function] function[f](t) keyword[return] builtin[nil] keyword[end]");
}

static void javascript(void)
{
    CHECK_STR(lex("js", "const r = /a\\/b[/]/g; x = a / b / c;"),
              "keyword[const] r = string[/a\\/b[/]/g]; x = a / b / c;");
    CHECK_STR(lex("ts", "let s = `a ${b}\nc`; // x"), "keyword[let] s = string[`a ${b}]\nstring[c`]; comment[// x]");
    CHECK_STR(lex("ts", "function f(n: number): void {}"),
              "keyword[function] function[f](n: type[number]): keyword[void] {}");
}

static void data_formats(void)
{
    CHECK_STR(lex("json", "{\"a\\\"b\": [true, null, -1.5e3, \"v\"]}"),
              "{key[\"a\\\"b\"]: [builtin[true], builtin[null], -number[1.5e3], string[\"v\"]]}");
    CHECK_STR(lex("yaml", "---\nkey: value # c\n- name: 'it''s'\n  \"q k\": &a *b\nurl: http://x"),
              "meta[---]\nkey[key]: value comment[# c]\nkeyword[-] key[name]: string['it]escape['']string[s']\n  "
              "key[\"q k\"]: variable[&a] variable[*b]\nkey[url]: http://x");
    CHECK_STR(lex("toml", "[server]\nport = 8080 # c\ns = \"\"\"a\nb\"\"\""),
              "section[[server]]\nkey[port] = number[8080] comment[# c]\nkey[s] = string[\"\"\"a]\nstring[b\"\"\"]");
    /* UP-Term's own preferences file */
    CHECK_STR(lex("ini", "[profile default]\nfont = TOPAZ:8.8.font\n; fg = C0C0C0\nbold-bright = on"),
              "section[[profile default]]\nkey[font] = TOPAZ:8.8.font\ncomment[; fg = C0C0C0]\nkey[bold-bright] = builtin[on]");
}

static void makefile(void)
{
    CHECK_STR(lex("make", "CC ?= gcc\nall: $(BUILD)/x # c\n\t$(CC) -o $@ x.c\nifeq ($(A),b)"),
              "key[CC] ?= gcc\nlabel[all:] variable[$(BUILD)]/x comment[# c]\n\tvariable[$(CC)] -o variable[$@] x.c\n"
              "keyword[ifeq] (variable[$(A)],b)");
}

static void markup(void)
{
    CHECK_STR(lex("html", "<a href=\"x\" disabled>t &amp; u</a><!-- c\nd --> e"),
              "tag[<a] attr[href]=string[\"x\"] attr[disabled]tag[>]t escape[&amp;] utag[</a>]comment[<!-- c]\n"
              "comment[d -->] e");
    CHECK_STR(lex("xml", "<?xml version=\"1.0\"?>\n<r><![CDATA[a<b]]></r>"),
              "preproc[<?xml version=\"1.0\"?>]\ntag[<r>]string[<![CDATA[a<b]]>]tag[</r>]");
    /* a tag over lines */
    CHECK_STR(lex("html", "<img\n  src='a.png'\n/>"), "tag[<img]\n  attr[src]=string['a.png']\ntag[/>]");
}

static void markdown_source(void)
{
    CHECK_STR(lex("md", "# Title\n- item `code` **b**\n```c\nint x;\n```\n[l](u)"),
              "heading[# Title]\nkeyword[-] item code[`code`] emphasis[**b**]\nmeta[```c]\ncode[int x;]\nmeta[```]\n"
              "link[[l]]string[(u)]");
    CHECK_STR(lex("md", "> quote\n1. one\n- [x] done"), "comment[> ]quote\nkeyword[1.] one\nkeyword[-] builtin[[x]] done");
}

static void diff(void)
{
    /* the hunk's counts decide: "--- x" inside it is a removed line */
    CHECK_STR(lex("diff", "--- a/f\n+++ b/f\n@@ -1,2 +1,2 @@ fn\n--- x\n same\n+new\n--- c/g"),
              "meta[--- a/f]\nmeta[+++ b/f]\nsection[@@ -1,2 +1,2 @@] fn\nremoved[--- x]\n same\nadded[+new]\nmeta[--- c/g]");
    CHECK_STR(lex("diff", "@@ -3 +3 @@\n-a\n+b\n\\ No newline at end of file"),
              "section[@@ -3 +3 @@]\nremoved[-a]\nadded[+b]\ncomment[\\ No newline at end of file]");
}

static void css(void)
{
    CHECK_STR(lex("css", "@media screen {\n.a:hover, #id > p { color: #fff; margin: 0 1.5em !important; }\n}"),
              "preproc[@media] tag[screen] {\ntype[.a]builtin[:hover], type[#id] > tag[p] { key[color]: number[#fff]; key[margin]: number[0] "
              "number[1.5em] keyword[!important]; }\n}");
    CHECK_STR(lex("css", "a::before { content: \"x\"; } /* c */"),
              "tag[a]builtin[::before] { key[content]: string[\"x\"]; } comment[/* c */]");
}

static void rust_go_java(void)
{
    /* Rust: comments nest, 'a is a lifetime, 'x' a character, name! a macro */
    CHECK_STR(lex("rust", "/* a /* b */ c */ fn f<'a>(c: char) { println!(\"{}\", 'x'); }"),
              "comment[/* a /* b */ c */] keyword[fn] f<label['a]>(c: type[char]) { function[println!](string[\"{}\"], "
              "string['x']); }");
    CHECK_STR(lex("rust", "#[derive(Debug)]\nlet s = r\"a\\b\";"), "preproc[#[derive(Debug)]]\nkeyword[let] s = string[r\"a\\b\"];");
    CHECK_STR(lex("rust", "let c = '\\n';"), "keyword[let] c = string[']escape[\\n]string['];");
    /* Go: a raw string has no escapes and runs over lines */
    CHECK_STR(lex("go", "s := `a\\n\nb` + \"\\t\""), "s := string[`a\\n]\nstring[b`] + string[\"]escape[\\t]string[\"]");
    CHECK_STR(lex("go", "func main() { fmt.Println(nil) }"),
              "keyword[func] function[main]() { fmt.function[Println](builtin[nil]) }");
    CHECK_STR(lex("java", "@Override\npublic String s = \"\"\"\n  x\n  \"\"\";"),
              "preproc[@Override]\nkeyword[public] type[String] s = string[\"\"\"]\nstring[  x]\nstring[  \"\"\"];");
}

static void detection(void)
{
    CHECK_STR(hl_detect("Work:src/main.c", 0, 0)->name, "c");
    CHECK_STR(hl_detect("x/README.MD", 0, 0)->name, "markdown");
    CHECK_STR(hl_detect("Makefile", 0, 0)->name, "make");
    CHECK_STR(hl_detect("Makefile.amiga", 0, 0)->name, "make");
    CHECK_STR(hl_detect("S:Startup-Sequence", 0, 0)->name, "amigados");
    CHECK_STR(hl_detect("ENVARC:up-term/up-term", 0, 0)->name, "ini");
    CHECK_STR(hl_detect(".vshrc", 0, 0)->name, "sh");
    CHECK_STR(hl_detect("intro.s", 0, 0)->name, "asm");
    CHECK_STR(hl_detect("hello.e", 0, 0)->name, "e");
    CHECK_STR(hl_detect("x.rexx", 0, 0)->name, "arexx");
    CHECK_STR(hl_detect("fix.patch", 0, 0)->name, "diff");
    CHECK_STR(hl_detect("a.tsx", 0, 0)->name, "javascript");
    CHECK_STR(hl_detect("run", "#!/usr/bin/env python3.11", 25)->name, "python");
    CHECK_STR(hl_detect(0, "#!/bin/sh -e", 12)->name, "sh");
    CHECK_STR(hl_detect("x", "#!/gg/bin/vsh", 13)->name, "sh");
    CHECK_STR(hl_detect("Install", ".KEY DIR/A", 10)->name, "amigados");
    CHECK_STR(hl_detect(0, "<?xml version", 13)->name, "xml");
    CHECK_STR(hl_detect(0, "diff --git a/x b/x", 18)->name, "diff");
    CHECK(hl_detect("notes.txt", "hello", 5) == 0);
    CHECK(hl_detect(0, 0, 0) == 0);
    CHECK_STR(hl_find("C++", 3)->name, "c");
    CHECK_STR(hl_find("yml", 3)->name, "yaml");
    CHECK_STR(hl_find("console", 7)->name, "sh");
    CHECK_STR(hl_find("68k", 3)->name, "asm");
    CHECK(hl_find("plaintext", 9) == 0);
}

/* every byte of a line comes back once, in order, whatever the language */
static char cover[512];
static long coverlen;
static void cover_tok(void *u, int cls, const char *s, long n)
{
    (void)u;
    (void)cls;
    if (coverlen + n < (long)sizeof(cover)) {
        memcpy(cover + coverlen, s, n);
        coverlen += n;
    }
}

static void every_byte_once(void)
{
    static const char *const lines[] = {
        "x = \"unterminated", "/* open", "'", "\"\"\"", "[[", "--[=[", "<a b='c", "#", "$", "@", "``` x",
        "\\", "a\\", "<!--", "@@ -x +y @@", "{\"k\": }", "- - -", "|a|b|", "\t", "", "<<", "r#\"x", "%", "->",
        "\xe4\xf6 caf\xc3\xa9 \xe2\x94\x80", 0
    };
    int i, k;
    for (k = 0; k < hl_nlangs; k++) {
        hl_state st;
        hl_begin(&st, &hl_langs[k]);
        for (i = 0; lines[i]; i++) {
            long n = (long)strlen(lines[i]);
            coverlen = 0;
            hl_line(&st, lines[i], n, cover_tok, 0);
            CHECK_INT(coverlen, n);
            CHECK(!memcmp(cover, lines[i], n));
        }
    }
}

/* ---- themes, SGR, output -------------------------------------------------- */

static void themes_and_sgr(void)
{
    hl_theme t;
    char sgr[64], err[96];
    hl_sty s;
    int n;
    CHECK(hl_theme_builtin(&t, "ansi"));
    CHECK(!hl_theme_builtin(&t, "nope"));
    n = hl_sgr(sgr, &t.s[HL_KEYWORD], 16);
    sgr[n] = 0;
    CHECK_STR(sgr, "\033[0;1;94m");
    s.fg = HL_RGB(0xc6, 0x78, 0xdd);
    s.bg = HL_DEFAULT;
    s.attr = HL_A_ITALIC;
    n = hl_sgr(sgr, &s, 24);
    sgr[n] = 0;
    CHECK_STR(sgr, "\033[0;3;38;2;198;120;221m");
    n = hl_sgr(sgr, &s, 256);
    sgr[n] = 0;
    CHECK_STR(sgr, "\033[0;3;38;5;176m");
    n = hl_sgr(sgr, &s, 16);
    sgr[n] = 0;
    CHECK_STR(sgr, "\033[0;3;95m");
    s.fg = 208; /* a 256-colour index on a 16-colour terminal: its nearest */
    s.bg = 4;
    s.attr = 0;
    n = hl_sgr(sgr, &s, 16);
    sgr[n] = 0;
    CHECK_STR(sgr, "\033[0;93;44m");
    CHECK_INT(hl_rgb_to_256(0, 0, 0), 16);
    CHECK_INT(hl_rgb_to_256(128, 128, 128), 244);
    CHECK_INT(hl_rgb_to_16(250, 250, 250), 15);
    CHECK_INT(hl_depth_from_env("vtcon", ""), 256);
    CHECK_INT(hl_depth_from_env("xterm", "truecolor"), 24);
    CHECK_INT(hl_depth_from_env("xterm", ""), 16);
    CHECK_INT(hl_depth_from_env("", ""), 16);
    /* a theme file over the default */
    hl_theme_builtin(&t, "ansi");
    CHECK_INT(hl_theme_parse(&t, "; mine\ncomment = green italic\nkeyword: #ff8000 bold on 4\n\n", 56, err, sizeof(err)), 0);
    CHECK_INT(t.s[HL_COMMENT].fg, 2);
    CHECK_INT(t.s[HL_COMMENT].attr, HL_A_ITALIC);
    CHECK(t.s[HL_KEYWORD].fg == HL_RGB(255, 128, 0));
    CHECK_INT(t.s[HL_KEYWORD].bg, 4);
    CHECK_INT(t.s[HL_KEYWORD].attr, HL_A_BOLD);
    CHECK_INT(hl_theme_parse(&t, "h1 = bright-cyan underline", 26, err, sizeof(err)), 0);
    CHECK_INT(t.s[MD_H1].fg, 14);
    CHECK_INT(hl_theme_parse(&t, "\nbogus = red", 12, err, sizeof(err)), -1);
    CHECK_STR(err, "unknown class on line 2");
    CHECK_INT(hl_theme_parse(&t, "string = sparkly", 16, err, sizeof(err)), -1);
    CHECK_STR(err, "unknown word on line 1");
}

static char out[4096];
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

static void charsets(void)
{
    vw_out o;
    hl_theme t;
    hl_theme_builtin(&t, "ansi");
    /* to UTF-8: UTF-8 stays, a Latin-1 byte (the Amiga's text) is converted */
    olen = 0;
    vo_init(&o, sink, 0, VW_UTF8, 0, &t);
    vo_textz(&o, "caf\xc3\xa9 caf\xe9 \xe2\x94\x80");
    vo_flush(&o);
    CHECK_STR(out, "caf\xc3\xa9 caf\xc3\xa9 \xe2\x94\x80");
    /* to Latin-1: UTF-8 is converted, what Latin-1 lacks is '?' */
    olen = 0;
    vo_init(&o, sink, 0, VW_LATIN1, 0, &t);
    vo_textz(&o, "caf\xc3\xa9 caf\xe9 \xe2\x94\x80");
    vo_flush(&o);
    CHECK_STR(out, "caf\xe9 caf\xe9 ?");
    CHECK_INT(vw_width("caf\xc3\xa9", 5), 4);
    CHECK_INT(vw_width("\xe4\xb8\xad", 3), 2); /* CJK: two cells, vtwidth.h's answer */
    CHECK_INT(vw_width("a\xcc\x81", 3), 1);    /* a combining mark */
    CHECK_INT(vw_fit("ab\xe4\xb8\xad", 5, 3), 2);
    /* no colour: no SGR at all */
    olen = 0;
    vo_init(&o, sink, 0, VW_UTF8, 0, &t);
    vo_class(&o, HL_KEYWORD);
    vo_textz(&o, "x");
    vo_reset(&o);
    vo_flush(&o);
    CHECK_STR(out, "x");
    /* a style is sent once while it holds */
    olen = 0;
    vo_init(&o, sink, 0, VW_UTF8, 16, &t);
    vo_class(&o, HL_STRING);
    vo_textz(&o, "a");
    vo_class(&o, HL_STRING);
    vo_textz(&o, "b");
    vo_reset(&o);
    vo_flush(&o);
    CHECK_STR(out, "\033[0;32mab\033[0m");
}

static void line_view(void)
{
    vw_out o;
    hl_view v;
    hl_theme t;
    hl_theme_builtin(&t, "ansi");
    olen = 0;
    vo_init(&o, sink, 0, VW_UTF8, 16, &t);
    hl_view_begin(&v, &o, hl_find("c", 1), 1, 8);
    hl_view_line(&v, "\tx;\t/* c */", 11, 1);
    hl_view_line(&v, "", 0, 1);
    vo_flush(&o);
    /* the gutter moves the tab stops: tabs become blanks to the file's own */
    CHECK_STR(out, "\033[0;90m    1 \xe2\x94\x82 \033[0m        x;      \033[0;3;90m/* c */\033[0m\n"
                   "\033[0;90m    2 \xe2\x94\x82 \033[0m\n");
    olen = 0;
    vo_init(&o, sink, 0, VW_LATIN1, 0, &t);
    hl_view_begin(&v, &o, 0, 1, 0);
    hl_view_line(&v, "a\tb\r", 4, 0);
    vo_flush(&o);
    CHECK_STR(out, "    1 | a\tb");
}

/* A 100 KB C file goes through the lexer (the timing is in the ledger,
 * measured with build/hl; this only proves it ends and keeps every byte). */
static void big_input(void)
{
    static char big[102400];
    long i, total = 0, lines = 0;
    hl_state st;
    const char *src = "static int f(int x) { /* comment */ return x * 0x10 + \"str\\n\"[0]; }\n";
    long sl = (long)strlen(src);
    for (i = 0; i + sl < (long)sizeof(big); i += sl)
        memcpy(big + i, src, sl);
    big[i] = 0;
    hl_begin(&st, hl_find("c", 1));
    coverlen = 0;
    for (i = 0; big[i];) {
        long e = i;
        while (big[e] && big[e] != '\n')
            e++;
        coverlen = 0;
        hl_line(&st, big + i, e - i, cover_tok, 0);
        total += coverlen;
        lines++;
        i = big[e] ? e + 1 : e;
    }
    CHECK_INT(total + lines, (long)strlen(big));
}

void suite_hl(void)
{
    c_family();
    asm68k();
    amigae();
    python();
    shell();
    amigados();
    arexx();
    lua();
    javascript();
    data_formats();
    makefile();
    markup();
    markdown_source();
    diff();
    css();
    rust_go_java();
    detection();
    every_byte_once();
    themes_and_sgr();
    charsets();
    line_view();
    big_input();
    hl_cleanup();
}
