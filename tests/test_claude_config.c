/* A4 WP3's loaders, parsers and rules on the host, on a temporary tree
 * through the portable sys layer (claude/sys_posix.c): settings merged
 * from three levels and written back, the permission rules, model
 * aliases, CLAUDE.md with its imports and nested memory, the definition
 * files (commands, agents, skills, output styles) and a command's
 * expansion, hooks with Claude Code's exit codes, sessions as JSONL
 * (byte-exact, append-only, resumed, branched, found by name), and the
 * checkpoints /rewind puts back. The REPL's own use of all of it is in
 * test_claude_repl.c (test_wp3, the reachability test). */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "harness.h"
#include "../claude/config.h"
#include "../claude/memory.h"
#include "../claude/commands.h"
#include "../claude/hooks.h"
#include "../claude/session.h"
#include "../claude/checkpoint.h"
#include "../claude/conv.h"
#include "../claude/sys_posix.h"
#include "../claude/util.h"

static char dir[512];
static sys_posix sp;
static cl_sys sys;

static void at(char *out, const char *rel)
{
    strcpy(out, dir);
    if (*rel) {
        strcat(out, "/");
        strcat(out, rel);
    }
}

static void put(const char *rel, const char *text)
{
    char p[700];
    FILE *f;
    at(p, rel);
    f = fopen(p, "wb");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void mk(const char *rel)
{
    char p[700];
    at(p, rel);
    mkdir(p, 0700);
}

static char *slurp(const char *rel)
{
    char p[700], *b = 0;
    long n = 0;
    at(p, rel);
    if (sys.read(sys.u, p, 1L << 20, &b, &n))
        return 0;
    return b;
}

static void tree(void)
{
    const char *base = getenv("TMPDIR");
    strcpy(dir, base && *base ? base : "/tmp");
    if (dir[strlen(dir) - 1] == '/')
        dir[strlen(dir) - 1] = 0;
    strcat(dir, "/claude_cfg_XXXXXX");
    if (!mkdtemp(dir))
        return;
    sys_posix_init(&sp, &sys);
    mk("home");
    mk("proj");
    mk("proj/.claude");
    mk("t");
}

static jv obj(const char *s)
{
    jv v;
    v.p = 0;
    v.n = 0;
    json_parse(s, (long)strlen(s), &v);
    return v;
}

/* ---- settings ---- */

static void test_settings(void)
{
    cl_settings s;
    char home[600], root[600];
    char *f;
    at(home, "home");
    at(root, "proj");
    put("home/settings.json",
        "{\"model\":\"sonnet\",\"effort\":\"low\",\"theme\":\"dark\",\"editorMode\":\"vim\",\"env\":{\"A\":\"1\",\"B\":\"user\"},"
        "\"vimInsertModeRemaps\":{\"jj\":\"<Esc>\",\"jjj\":\"<Esc>\",\"kj\":\"x\",\"jk\":\"<Esc>\"},"
        "\"permissions\":{\"allow\":[\"Read\"],\"deny\":[\"Bash(rm *)\"]},"
        "\"hooks\":{\"PreToolUse\":[{\"matcher\":\"Bash\",\"hooks\":[{\"type\":\"command\",\"command\":\"check\",\"timeout\":5}]}]}}");
    put("proj/.claude/settings.json",
        "{\"model\":\"opus\",\"outputStyle\":\"Explanatory\",\"env\":{\"B\":\"project\"},"
        "\"vimInsertModeRemaps\":{\"qq\":\"<Esc>\"},"
        "\"permissions\":{\"allow\":[\"Bash(make *)\"],\"defaultMode\":\"acceptEdits\","
        "\"additionalDirectories\":[\"Work:Lib\"]},\"statusLine\":{\"type\":\"command\",\"command\":\"ctx\"},"
        "\"autoCompactEnabled\":false}");
    put("proj/.claude/settings.local.json", "{\"effortLevel\":\"high\",\"permissions\":{\"ask\":[\"Edit\"]}}");
    cfg_init(&s);
    CHECK_INT(cfg_load(&s, &sys, home, root), 3);
    /* scalars: the later file wins; lists add up */
    CHECK_STR(s.model, "opus");
    CHECK_STR(s.effort, "high");
    CHECK_STR(s.theme, "dark");
    CHECK_STR(s.editor_mode, "vim");
    /* remaps: two characters to <Esc> only, and never from a project's file */
    CHECK_STR(s.vim_remaps, "jjjk");
    CHECK_STR(s.output_style, "Explanatory");
    CHECK_STR(s.status_cmd, "ctx");
    CHECK_STR(s.default_mode, "acceptEdits");
    CHECK_INT(s.auto_compact, 0);
    CHECK_INT(s.nrules, 4);
    CHECK_INT(s.nhooks, 1);
    CHECK(s.nhooks == 1 && s.hooks[0].event == HK_PRE_TOOL && s.hooks[0].timeout_s == 5 &&
          !strcmp(s.hooks[0].matcher, "Bash") && !strcmp(s.hooks[0].cmd, "check"));
    CHECK_INT(s.nenv, 2);
    CHECK(s.nenv == 2 && !strcmp(s.env[1].k, "B") && !strcmp(s.env[1].v, "project"));
    CHECK(s.ndirs == 1 && !strcmp(s.dirs[0], "Work:Lib"));
    CHECK(s.found[CFG_USER] && s.found[CFG_PROJECT] && s.found[CFG_LOCAL]);
    cfg_free(&s);

    /* a file that does not parse is skipped, with the reason kept */
    put("proj/.claude/settings.local.json", "{\"model\": oops}");
    cfg_init(&s);
    CHECK_INT(cfg_load(&s, &sys, home, root), 2);
    CHECK(strstr(s.err, "settings.local.json is not a JSON object") != 0);
    CHECK_STR(s.effort, "low");
    cfg_free(&s);

    /* written back: one key set, the others kept; a rule added and removed */
    put("proj/.claude/settings.local.json", "{\"theme\": \"light\", \"permissions\": {\"allow\": [\"Read\"]}}\n");
    {
        char p[600];
        at(p, "proj/.claude/settings.local.json");
        CHECK_INT(cfg_write_key(&sys, p, "outputStyle", "\"Learning\""), 0);
        CHECK_INT(cfg_write_rule(&sys, p, RULE_ALLOW, "Bash(make *)", 1), 0);
        CHECK_INT(cfg_write_rule(&sys, p, RULE_DENY, "WebFetch", 1), 0);
        CHECK_INT(cfg_write_rule(&sys, p, RULE_ALLOW, "Read", 0), 0);
        CHECK_INT(cfg_write_rule(&sys, p, RULE_ALLOW, "Nothing", 0), -1);
        CHECK_INT(cfg_write_key(&sys, p, "theme", 0), 0);
        f = slurp("proj/.claude/settings.local.json");
        CHECK(f != 0);
        if (f) {
            cfg_init(&s);
            CHECK_INT(cfg_merge(&s, CFG_LOCAL, f, (long)strlen(f), "x"), 0);
            CHECK_STR(s.output_style, "Learning");
            CHECK_STR(s.theme, "");
            CHECK_INT(s.nrules, 2);
            CHECK(s.nrules == 2 && !strcmp(s.rules[0].text, "Bash(make *)") && s.rules[1].kind == RULE_DENY);
            cfg_free(&s);
            free(f);
        }
        /* a new file, its directory made */
        at(p, "proj/sub/.claude/settings.local.json");
        mk("proj/sub");
        CHECK_INT(cfg_write_rule(&sys, p, RULE_ASK, "Bash", 1), 0);
        f = slurp("proj/sub/.claude/settings.local.json");
        CHECK(f && strstr(f, "\"ask\": [\"Bash\"]") != 0);
        free(f);
    }
    /* the WebSearch switch: on unless "webSearch": false or a deny rule names
     * WebSearch alone; an ask rule or a pattern does not turn it off; the
     * statusLine's padding and refreshInterval */
    {
        static const char *const on[] = { "{}", "{\"webSearch\":true}", "{\"permissions\":{\"ask\":[\"WebSearch\"]}}",
                                          "{\"permissions\":{\"deny\":[\"WebFetch\",\"WebSearch(domain:x.org)\"]}}" };
        static const char *const off[] = { "{\"webSearch\":false}", "{\"permissions\":{\"deny\":[\"WebSearch\"]}}",
                                           "{\"permissions\":{\"deny\":[\"WebSearch(*)\"]}}" };
        const char *sl = "{\"statusLine\":{\"type\":\"command\",\"command\":\"x\",\"padding\":2,\"refreshInterval\":5}}";
        int k;
        for (k = 0; k < 4; k++) {
            cfg_init(&s);
            CHECK_INT(cfg_merge(&s, CFG_USER, on[k], (long)strlen(on[k]), "x"), 0);
            CHECK_INT(cfg_web_search(&s), 1);
            cfg_free(&s);
        }
        for (k = 0; k < 3; k++) {
            cfg_init(&s);
            CHECK_INT(cfg_merge(&s, CFG_USER, off[k], (long)strlen(off[k]), "x"), 0);
            CHECK_INT(cfg_web_search(&s), 0);
            cfg_free(&s);
        }
        cfg_init(&s);
        CHECK_INT(cfg_merge(&s, CFG_USER, sl, (long)strlen(sl), "x"), 0);
        CHECK_STR(s.status_cmd, "x");
        CHECK_INT(s.status_pad, 2);
        CHECK_INT(s.status_refresh_s, 5);
        cfg_free(&s);
    }
}

static void test_rules(void)
{
    cl_settings s;
    const cl_rule *w;
    const char *root = "Work:proj";
    jv bash_make = obj("{\"command\":\"make test\"}");
    jv bash_mk2 = obj("{\"command\":\"make test && make install\"}");
    jv bash_rm = obj("{\"command\":\"make && rm -rf x\"}");
    jv bash_bare = obj("{\"command\":\"make\"}");
    jv bash_makex = obj("{\"command\":\"makefoo\"}");
    jv rd_src = obj("{\"path\":\"src/a/b.c\"}");
    jv rd_doc = obj("{\"path\":\"doc/x.txt\"}");
    jv wr_c = obj("{\"path\":\"deep/dir/main.c\",\"content\":\"\"}");
    jv wr_h = obj("{\"file_path\":\"Work:proj/x.h\",\"content\":\"\"}");
    jv url = obj("{\"url\":\"https://www.aminet.net/x\",\"prompt\":\"p\"}");
    jv url2 = obj("{\"url\":\"https://evil.example/x\",\"prompt\":\"p\"}");
    char t[64], p[300];
    /* the parts of a rule */
    CHECK_INT(cfg_rule_parse("Bash(make *)", t, sizeof(t), p, sizeof(p)), 0);
    CHECK_STR(t, "Bash");
    CHECK_STR(p, "make *");
    CHECK_INT(cfg_rule_parse("Read", t, sizeof(t), p, sizeof(p)), 0);
    CHECK_STR(p, "");
    CHECK_INT(cfg_rule_parse("Edit(*)", t, sizeof(t), p, sizeof(p)), 0);
    CHECK_STR(p, "");
    CHECK_INT(cfg_rule_parse("Bash(make", t, sizeof(t), p, sizeof(p)), -1);
    CHECK_INT(cfg_rule_parse("(x)", t, sizeof(t), p, sizeof(p)), -1);
    /* Bash: exact, prefix forms, compound lines */
    CHECK(cfg_rule_match("Bash(make test)", "run_command", bash_make, root));
    CHECK(cfg_rule_match("Bash(make:*)", "Bash", bash_make, root));
    CHECK(cfg_rule_match("Bash(make:*)", "Bash", bash_bare, root));
    CHECK(!cfg_rule_match("Bash(make:*)", "Bash", bash_makex, root));
    CHECK(cfg_rule_match("Bash(make *)", "Bash", bash_bare, root));
    CHECK(cfg_rule_match("Bash(make *)", "Bash", bash_mk2, root));
    CHECK(!cfg_rule_match("Bash(make *)", "Bash", bash_rm, root));
    CHECK(cfg_rule_match("Bash", "run_command", bash_rm, root));
    /* paths: relative to the root, a bare name at any depth, absolute */
    CHECK(cfg_rule_match("Read(src/**)", "read_file", rd_src, root));
    CHECK(cfg_rule_match("Read(src)", "list_dir", rd_src, root));
    CHECK(!cfg_rule_match("Read(src/**)", "read_file", rd_doc, root));
    CHECK(cfg_rule_match("Read(./doc/*.txt)", "grep", rd_doc, root));
    CHECK(cfg_rule_match("Edit(*.c)", "write_file", wr_c, root));
    CHECK(!cfg_rule_match("Edit(*.c)", "write_file", wr_h, root));
    CHECK(cfg_rule_match("Edit(Work:proj/#?.h)", "Write", wr_h, root));
    CHECK(cfg_rule_match("Edit(WORK:Proj/*.H)", "MultiEdit", wr_h, root));
    CHECK(!cfg_rule_match("Read(*.c)", "write_file", wr_c, root));    /* a Read allow does not cover Write */
    /* A4 gaps 2: Claude Code -- an Edit allow rule grants Read on its paths;
     * a Read deny rule blocks Edit and Write there (cfg_decide) */
    CHECK(cfg_rule_match("Edit", "read_file", rd_src, root));
    CHECK(cfg_rule_match("Edit(src/**)", "Read", rd_src, root));
    CHECK(!cfg_rule_match("Edit(src/**)", "Read", rd_doc, root));
    CHECK(!cfg_rule_match_any("Edit", "Read", rd_src, root));     /* a hook's "if": the tool itself */
    {
        cl_settings ds;
        const cl_rule *which = 0;
        cfg_init(&ds);
        cfg_add_rule(&ds, RULE_DENY, CFG_USER, "Read(*.c)");
        CHECK_INT(cfg_decide(&ds, "Write", wr_c, root, &which), RULE_DENY);
        CHECK_INT(cfg_decide(&ds, "Edit", wr_c, root, &which), RULE_DENY);
        CHECK_INT(cfg_decide(&ds, "Write", wr_h, root, &which), RULE_NONE);
        cfg_free(&ds);
        cfg_init(&ds);
        cfg_add_rule(&ds, RULE_ASK, CFG_USER, "Read(*.c)");
        CHECK_INT(cfg_decide(&ds, "Write", wr_c, root, &which), RULE_NONE);    /* only a deny reaches over */
        cfg_free(&ds);
    }
    /* WebFetch domains, subdomains included */
    CHECK(cfg_rule_match("WebFetch(domain:aminet.net)", "WebFetch", url, root));
    CHECK(!cfg_rule_match("WebFetch(domain:aminet.net)", "WebFetch", url2, root));
    /* deny wins over ask wins over allow */
    cfg_init(&s);
    cfg_add_rule(&s, RULE_ALLOW, CFG_USER, "Bash");
    cfg_add_rule(&s, RULE_ASK, CFG_PROJECT, "Bash(make install)");
    cfg_add_rule(&s, RULE_DENY, CFG_LOCAL, "Bash(rm *)");
    cfg_add_rule(&s, RULE_ALLOW, CFG_LOCAL, "Bash");    /* a duplicate is not added */
    CHECK_INT(s.nrules, 3);
    CHECK_INT(cfg_decide(&s, "run_command", bash_make, root, &w), RULE_ALLOW);
    CHECK(w && !strcmp(w->text, "Bash"));
    CHECK_INT(cfg_decide(&s, "run_command", bash_mk2, root, &w), RULE_ASK);
    CHECK_INT(cfg_decide(&s, "run_command", bash_rm, root, &w), RULE_DENY);
    CHECK(w && w->src == CFG_LOCAL);
    CHECK_INT(cfg_decide(&s, "read_file", rd_src, root, &w), RULE_NONE);
    CHECK(w == 0);
    cfg_free(&s);
    /* the names */
    CHECK_STR(cfg_cc_tool("run_command"), "Bash");
    CHECK_STR(cfg_cc_tool("list_dir"), "LS");
    CHECK_STR(cfg_cc_tool("WebFetch"), "WebFetch");
    /* the glob */
    CHECK(cfg_glob("a/**/b", "a/b", 0));
    CHECK(cfg_glob("a/**/b", "a/x/y/b", 0));
    CHECK(!cfg_glob("a/*/b", "a/x/y/b", 0));
    CHECK(cfg_glob("[a-c]?.md", "b1.md", 0));
    CHECK(!cfg_glob("[!a-c]?.md", "b1.md", 0));
}

static void test_models(void)
{
    cl_price p;
    CHECK_STR(cfg_model("opus"), "claude-opus-5-5");
    CHECK_STR(cfg_model("Sonnet"), "claude-sonnet-5-5");
    CHECK_STR(cfg_model("haiku"), "claude-haiku-4-5-20251001");
    CHECK_STR(cfg_model("fable"), "claude-fable-5-1");
    CHECK_STR(cfg_model("claude-opus-5"), "claude-opus-5");
    /* the dated Haiku id is priced as Haiku 4.5; Opus 5.5 is not Opus 5 */
    CHECK_INT(conv_price("claude-haiku-4-5-20251001", &p), 1);
    CHECK_INT(p.in, 100);
    CHECK_INT(conv_price("claude-opus-5-5", &p), 1);
    CHECK_INT(p.in, 400);
    CHECK_INT(conv_price("claude-opus-5-x", &p), 0);
    /* the request body for each alias: only the fields its model takes */
    {
        static const char *const al[] = { "opus", "sonnet", "fable", "haiku" };
        int i;
        for (i = 0; i < 4; i++) {
            cl_conv c;
            cl_opts o;
            jw b;
            jv v, x, y;
            int haiku = i == 3;
            conv_init(&c);
            conv_add_user_text(&c, "hi", 2);
            memset(&o, 0, sizeof(o));
            o.model = cfg_model(al[i]);
            o.effort = "high";
            o.max_tokens = 64000;
            o.system = "s";
            o.tools = "";
            jw_init(&b);
            CHECK_INT(conv_body(&c, &o, &b), 0);
            CHECK_INT(json_parse(b.p, b.n, &v), 0);
            CHECK(json_get(v, "model", &x) && json_streq(x, o.model));
            if (haiku) {
                CHECK(!json_get(v, "thinking", &x));
                CHECK(!json_get(v, "output_config", &x));
                CHECK(!json_get(v, "fallbacks", &x));
                CHECK_STR(conv_beta(o.model), "");
            } else {
                CHECK(json_get(v, "thinking", &x) && json_get(x, "type", &y) && json_streq(y, "adaptive"));
                CHECK(json_get(v, "thinking", &x) && json_get(x, "display", &y) && json_streq(y, "summarized"));
                CHECK(json_get(v, "output_config", &x) && json_get(x, "effort", &y) && json_streq(y, "high"));
                CHECK(json_get(v, "fallbacks", &x) && json_streq(x, "default"));
                CHECK_STR(conv_beta(o.model), "server-side-fallback-2026-07-01");
            }
            jw_free(&b);
            conv_free(&c);
        }
    }
}

/* ---- memory ---- */

static void test_memory(void)
{
    cl_memory m;
    char home[600], root[600], f[600];
    jw o;
    const char *t;
    mk("proj/lib");
    mk("proj/lib/deep");
    put("home/CLAUDE.md", "USER-MEMORY\n");
    put("proj/CLAUDE.md", "PROJECT-MEMORY see @notes.md, and `@not-imported.md`.\n```\n@code.md\n```\n");
    put("proj/notes.md", "IMPORTED-NOTES @more/x.md\n");
    mk("proj/more");
    put("proj/more/x.md", "DEEPER @../notes.md\n");    /* a cycle: notes is read once */
    put("proj/not-imported.md", "NEVER\n");
    put("proj/code.md", "NEVER-CODE\n");
    put("proj/AMIGA.md", "AMIGA-NOTES\n");
    put("proj/CLAUDE.local.md", "LOCAL-MEMORY\n");
    put("proj/lib/CLAUDE.md", "LIB-MEMORY\n");
    at(home, "home");
    at(root, "proj");
    mem_init(&m);
    CHECK_INT(mem_load(&m, &sys, home, root), 6);
    t = m.text.p ? m.text.p : "";
    CHECK(strstr(t, "USER-MEMORY") != 0);
    CHECK(strstr(t, "(user's private global instructions for all projects)") != 0);
    /* the order: user, project, its imports, AMIGA.md, local */
    CHECK(strstr(t, "USER-MEMORY") < strstr(t, "PROJECT-MEMORY"));
    CHECK(strstr(t, "PROJECT-MEMORY") < strstr(t, "IMPORTED-NOTES"));
    CHECK(strstr(t, "IMPORTED-NOTES") < strstr(t, "DEEPER"));
    CHECK(strstr(t, "DEEPER") < strstr(t, "AMIGA-NOTES"));
    CHECK(strstr(t, "AMIGA-NOTES") < strstr(t, "LOCAL-MEMORY"));
    CHECK(strstr(t, "NEVER") == 0);
    CHECK(strstr(t, "LIB-MEMORY") == 0);
    CHECK(strstr(t, "(user's private project instructions, not checked in)") != 0);
    /* a file below the root was read: its directories' memory, once */
    jw_init(&o);
    at(f, "proj/lib/deep/x.c");
    CHECK_INT(mem_nested(&m, &sys, root, f, &o), 1);
    CHECK(o.p && strstr(o.p, "LIB-MEMORY") != 0);
    jw_reset(&o);
    CHECK_INT(mem_nested(&m, &sys, root, f, &o), 0);
    at(f, "proj/notes.md");
    CHECK_INT(mem_nested(&m, &sys, root, f, &o), 0);
    jw_free(&o);
    mem_free(&m);
    /* the files /memory edits */
    mem_file_of(MEM_LOCAL, "ENVARC:Claude", "Work:p", f, sizeof(f));
    CHECK_STR(f, "Work:p/CLAUDE.local.md");
    mem_file_of(MEM_USER, "ENVARC:Claude", "Work:p", f, sizeof(f));
    CHECK_STR(f, "ENVARC:Claude/CLAUDE.md");
}

/* ---- definitions ---- */

static void test_defs(void)
{
    cl_defs s;
    const cl_def *d;
    char home[600], root[600], err[300];
    jw o;
    cl_def x;
    mk("home/commands");
    mk("home/agents");
    mk("proj/.claude/commands");
    mk("proj/.claude/commands/git");
    mk("proj/.claude/agents");
    mk("proj/.claude/skills");
    mk("proj/.claude/skills/pdf");
    mk("proj/.claude/output-styles");
    put("home/commands/review.md", "---\ndescription: user review\n---\nUSER $ARGUMENTS\n");
    put("home/commands/hello.md", "Say hello to $0 and $1.\n");
    put("proj/.claude/commands/review.md",
        "---\ndescription: \"Review the code\"\nallowed-tools: Bash(git diff:*), Read\nmodel: haiku\n"
        "argument-hint: [file]\n---\n\nReview $ARGUMENTS. Diff: !`echo DIFF-OUT`\nAlso @notes.md here.\n");
    put("proj/.claude/commands/git/commit.md", "Commit it.\n");
    put("proj/.claude/agents/tester.md",
        "---\nname: tester\ndescription: Runs the tests\ntools:\n  - Read\n  - Bash\nmodel: sonnet\n---\nYou test.\n");
    put("proj/.claude/skills/pdf/SKILL.md", "---\nname: pdf-tools\ndescription: PDFs\n---\nUse pdftotext.\n");
    put("proj/.claude/output-styles/terse.md", "---\nname: Terse\ndescription: Short\nkeep-coding-instructions: true\n---\nBe terse.\n");
    at(home, "home");
    at(root, "proj");
    defs_init(&s);
    defs_load(&s, &sys, home, root);
    CHECK_INT(defs_count(&s, DEF_COMMAND), 3);     /* review (project hides user), hello, git:commit (its subdirectory: Claude Code's namespace) */
    CHECK_INT(defs_count(&s, DEF_AGENT), 1);
    CHECK_INT(defs_count(&s, DEF_SKILL), 9);       /* one of the project's, eight bundled ones */
    CHECK_INT(defs_count(&s, DEF_STYLE), 6);       /* Default, Proactive, Concise, Explanatory, Learning, Terse */
    d = defs_find(&s, DEF_COMMAND, "review");
    CHECK(d && d->src == CFG_PROJECT);
    CHECK(d && !strcmp(d->description, "Review the code") && !strcmp(d->model, "haiku") &&
          !strcmp(d->hint, "[file]") && !strcmp(d->tools, "Bash(git diff:*), Read"));
    CHECK(d && def_has_tool(d, "Bash") && def_has_tool(d, "Read") && !def_has_tool(d, "Edit"));
    CHECK(defs_find(&s, DEF_COMMAND, "git:commit") != 0 && !defs_find(&s, DEF_COMMAND, "commit"));
    d = defs_find(&s, DEF_AGENT, "tester");
    CHECK(d && !strcmp(d->tools, "Read, Bash") && !strcmp(d->body, "You test.\n") && !strcmp(d->model, "sonnet"));
    d = defs_find(&s, DEF_SKILL, "pdf-tools");
    CHECK(d && strstr(d->path, "skills/pdf/SKILL.md") != 0 && !strcmp(d->body, "Use pdftotext.\n"));
    d = defs_find(&s, DEF_STYLE, "terse");
    CHECK(d && d->keep_coding && !strcmp(d->body, "Be terse.\n"));
    CHECK(defs_find(&s, DEF_STYLE, "Explanatory") && defs_find(&s, DEF_STYLE, "Explanatory")->src == DEF_BUILTIN);

    /* expansion: $ARGUMENTS, !`cmd` (allowed: Bash), @file */
    jw_init(&o);
    put("proj/notes.md", "NOTES\n");
    d = defs_find(&s, DEF_COMMAND, "review");
    CHECK_INT(cmd_expand(d, "main.c", &sys, root, &o, err, sizeof(err)), 0);
    CHECK_STR(o.p ? o.p : "", "Review main.c. Diff: DIFF-OUT\nAlso notes.md:\n```\nNOTES\n```\n here.\n");
    /* $0 $1 (0-based, Claude Code), quoted words; no $ARGUMENTS: nothing appended when $n used */
    jw_reset(&o);
    d = defs_find(&s, DEF_COMMAND, "hello");
    CHECK_INT(cmd_expand(d, "\"Amiga 1200\" world", &sys, root, &o, err, sizeof(err)), 0);
    CHECK_STR(o.p ? o.p : "", "Say hello to Amiga 1200 and world.\n");
    /* no placeholder at all: the arguments are appended */
    jw_reset(&o);
    d = defs_find(&s, DEF_COMMAND, "git:commit");
    CHECK_INT(cmd_expand(d, "fast", &sys, root, &o, err, sizeof(err)), 0);
    CHECK_STR(o.p ? o.p : "", "Commit it.\n\n\nARGUMENTS: fast");
    jw_free(&o);
    /* !`cmd` without Bash in allowed-tools is refused */
    memset(&x, 0, sizeof(x));
    CHECK_INT(defs_parse("Run !`echo hi` now", 18, &x), 0);
    jw_init(&o);
    CHECK_INT(cmd_expand(&x, "", &sys, root, &o, err, sizeof(err)), -1);
    CHECK(strstr(err, "has no Bash") != 0);
    jw_free(&o);
    def_free(&x);
    {
        /* Claude Code's skill substitutions (A4 gaps X2): $ARGUMENTS[N] and $N
         * 0-based, an index with no argument stays, \$ a dollar, $name from
         * the arguments list (empty when missing), the ${CLAUDE_*} values;
         * an argument's own $1 is not expanded again */
        static const char t[] = "---\narguments: [issue, branch, extra]\n---\n"
                                "A=$ARGUMENTS[1] B=$0 C=$5 D=\\$1 E=$issue F=$extra G=${CLAUDE_SKILL_DIR}/s "
                                "H=${CLAUDE_SESSION_ID} I=${CLAUDE_EFFORT} J=${CLAUDE_PROJECT_DIR}";
        cl_cmd_vars v;
        memset(&v, 0, sizeof(v));
        v.session_id = "abc123";
        v.effort = "high";
        v.skill_dir = "S:skills/x";
        v.project_dir = "Work:proj";
        memset(&x, 0, sizeof(x));
        CHECK_INT(defs_parse(t, (long)sizeof(t) - 1, &x), 0);
        CHECK_STR(x.arg_names, "issue, branch, extra");
        jw_init(&o);
        CHECK_INT(cmd_expand_vars(&x, "\"$1 lit\" two", &v, &sys, root, &o, err, sizeof(err)), 0);
        CHECK_STR(o.p ? o.p : "", "A=two B=$1 lit C=$5 D=$1 E=$1 lit F= G=S:skills/x/s H=abc123 I=high J=Work:proj");
        jw_free(&o);
        def_free(&x);
        memset(&x, 0, sizeof(x));
    }
    def_free(&x);
    defs_free(&s);
}

/* ---- hooks ---- */

static void test_hooks(void)
{
    cl_settings s;
    cl_hooks h;
    cl_hookres r;
    char cmd[800], tmp[600], json[1600];
    const char *const m[] = { "Edit|Write", "Bash" };
    CHECK(hooks_match("", "anything"));
    CHECK(hooks_match("*", "anything"));
    CHECK(hooks_match(m[0], "write_file"));     /* our name, Claude Code's matcher */
    CHECK(hooks_match(m[0], "Edit"));
    CHECK(!hooks_match(m[0], "read_file"));
    CHECK(hooks_match(m[1], "run_command"));
    CHECK(hooks_match("mcp__.*", "mcp__x__y"));
    put("block.sh", "cat > \"$(dirname \"$0\")/seen.json\"\necho 'no rm here' >&2\nexit 2\n");
    put("ok.sh", "cat >/dev/null\necho 'CONTEXT-LINE'\nexit 0\n");
    put("fail.sh", "cat >/dev/null\necho 'broken'\nexit 1\n");
    put("deny.sh", "cat >/dev/null\necho '{\"hookSpecificOutput\":{\"permissionDecision\":\"deny\",\"permissionDecisionReason\":\"policy\"}}'\n");
    put("allow.sh", "cat >/dev/null\necho '{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\",\"additionalContext\":\"ADDED\"}}'\n");
    strcpy(json, "{\"hooks\":{\"PreToolUse\":[{\"matcher\":\"Bash\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    at(cmd, "block.sh");
    strcat(json, cmd);
    strcat(json, "\"}]},{\"matcher\":\"Read\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    at(cmd, "deny.sh");
    strcat(json, cmd);
    strcat(json, "\"}]},{\"matcher\":\"Grep\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    at(cmd, "allow.sh");
    strcat(json, cmd);
    strcat(json, "\"}]}],\"UserPromptSubmit\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    at(cmd, "ok.sh");
    strcat(json, cmd);
    strcat(json, "\"}]}],\"SessionStart\":[{\"matcher\":\"startup\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    at(cmd, "fail.sh");
    strcat(json, cmd);
    strcat(json, "\"}]}]}}");
    cfg_init(&s);
    CHECK_INT(cfg_merge(&s, CFG_PROJECT, json, (long)strlen(json), "x"), 0);
    CHECK_INT(s.nhooks, 5);
    memset(&h, 0, sizeof(h));
    at(tmp, "t");
    h.cfg = &s;
    h.sys = &sys;
    h.session_id = "0000abcd";
    h.transcript = "T:x.jsonl";
    h.cwd = "Work:proj";
    h.tmp = tmp;
    CHECK(hooks_any(&h, HK_PRE_TOOL, "run_command"));
    CHECK(!hooks_any(&h, HK_PRE_TOOL, "write_file"));
    CHECK(!hooks_any(&h, HK_STOP, ""));
    /* exit 2: blocked, the output is the reason; the event JSON on stdin */
    hookres_init(&r);
    CHECK_INT(hooks_run(&h, HK_PRE_TOOL, "run_command", ",\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"rm x\"}", &r), 1);
    CHECK_INT(r.blocked, 1);
    CHECK_STR(r.reason.p ? r.reason.p : "", "no rm here");
    {
        char *seen = slurp("seen.json");
        jv v, x;
        CHECK(seen && json_parse(seen, (long)strlen(seen), &v) == 0);
        CHECK(seen && json_get(v, "hook_event_name", &x) && json_streq(x, "PreToolUse"));
        CHECK(seen && json_get(v, "session_id", &x) && json_streq(x, "0000abcd"));
        CHECK(seen && json_get(v, "cwd", &x) && json_streq(x, "Work:proj"));
        CHECK(seen && json_get(v, "tool_input", &x) && json_get(x, "command", &x) && json_streq(x, "rm x"));
        free(seen);
    }
    hookres_free(&r);
    /* JSON: permissionDecision deny / allow with context */
    hookres_init(&r);
    hooks_run(&h, HK_PRE_TOOL, "read_file", ",\"tool_name\":\"Read\"", &r);
    CHECK_INT(r.blocked, 1);
    CHECK_INT(r.decision, RULE_DENY);
    CHECK_STR(r.reason.p ? r.reason.p : "", "policy");
    hookres_free(&r);
    hookres_init(&r);
    hooks_run(&h, HK_PRE_TOOL, "grep", ",\"tool_name\":\"Grep\"", &r);
    CHECK_INT(r.blocked, 0);
    CHECK_INT(r.decision, RULE_ALLOW);
    CHECK_STR(r.context.p ? r.context.p : "", "ADDED");
    hookres_free(&r);
    /* exit 0 on UserPromptSubmit: the output is context */
    hookres_init(&r);
    hooks_run(&h, HK_PROMPT, "", ",\"prompt\":\"hi\"", &r);
    CHECK_STR(r.context.p ? r.context.p : "", "CONTEXT-LINE");
    hookres_free(&r);
    /* another code: an error for the user, nothing blocked */
    hookres_init(&r);
    CHECK_INT(hooks_run(&h, HK_SESSION_START, "startup", ",\"source\":\"startup\"", &r), 1);
    CHECK_INT(r.blocked, 0);
    CHECK(r.shown.p && strstr(r.shown.p, "SessionStart hook") && strstr(r.shown.p, "failed with 1: broken"));
    hookres_free(&r);
    hookres_init(&r);
    CHECK_INT(hooks_run(&h, HK_SESSION_START, "resume", "", &r), 0);   /* the matcher says no */
    hookres_free(&r);
    CHECK_INT(h.n_run, 5);
    cfg_free(&s);
}

/* ---- sessions ---- */

static void addz(cl_conv *c, int user, const char *s)
{
    conv_add(c, user, s, (long)strlen(s));
}

static void test_sessions(void)
{
    cl_session s, s2;
    cl_conv c, back;
    cl_sess_info l[8];
    char home[600], id1[16], id[16], slug[64];
    char *f;
    int n, i;
    static const char thinking[] =
        "[{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"Eq+/sig==\"},{\"type\":\"text\",\"text\":\"Hi \\u00e9\"}]";
    sess_slug("Work:Projects/x", slug, sizeof(slug));
    CHECK_STR(slug, "Work-Projects-x");
    sess_slug("DH0:a/very/long/path/to/some/project/called/foobar", slug, sizeof(slug));
    CHECK_INT((long)strlen(slug), 30);
    CHECK(strstr(slug, "called-foobar-") != 0);
    at(home, "home");
    sess_init(&s, &sys, home, "Work:proj");
    sess_new(&s, 0x12345678UL * 1000UL);
    CHECK_STR(s.id, "12345678");
    conv_init(&c);
    conv_add_user_text(&c, "first prompt", 12);
    conv_add(&c, 0, thinking, (long)strlen(thinking));
    CHECK_INT(sess_save(&s, &c), 0);
    CHECK_INT(sess_save(&s, &c), 0);           /* nothing new: no write */
    CHECK_INT(s.n_appends, 1);
    /* a tool_result message, then a prompt added to it: the message again */
    addz(&c, 1, "[{\"type\":\"tool_result\",\"tool_use_id\":\"t\",\"content\":\"x\"}]");
    CHECK_INT(sess_save(&s, &c), 0);
    conv_add_user_text(&c, "go on", 5);
    addz(&c, 0, "[{\"type\":\"text\",\"text\":\"done\"}]");
    CHECK_INT(sess_save(&s, &c), 0);
    CHECK_INT(s.n_appends, 3);
    sess_rename(&s, "My Work");
    cl_copy(id1, s.id, sizeof(id1));
    f = slurp("home/projects/Work-proj/12345678.jsonl");
    CHECK(f && !strncmp(f, "{\"type\":\"session\"", 17));
    /* written whole messages, one per line, the thinking block as it was */
    CHECK(f && strstr(f, thinking) != 0);
    CHECK(f && strstr(f, "{\"type\":\"user\",\"index\":2,") != 0);
    free(f);
    /* read back: byte for byte, the last line of index 2 winning */
    sess_init(&s2, &sys, home, "Work:proj");
    conv_init(&back);
    CHECK_INT(sess_load(&s2, id1, &back), 0);
    CHECK_INT(back.n, c.n);
    for (i = 0; i < c.n && i < back.n; i++) {
        CHECK_INT(back.m[i].n, c.m[i].n);
        CHECK(back.m[i].n == c.m[i].n && !memcmp(back.m[i].json, c.m[i].json, (size_t)c.m[i].n));
        CHECK_INT(back.m[i].user, c.m[i].user);
    }
    CHECK_STR(s2.title, "My Work");
    /* a rewind: the file gets one more line, the next load has the shorter one */
    {
        cl_mark mk;
        mk.n = 2;
        mk.last = c.m[1].n;
        conv_rollback(&c, mk);
        sess_truncate(&s, 2);
        conv_add_user_text(&c, "instead", 7);
        addz(&c, 0, "[{\"type\":\"text\",\"text\":\"ok\"}]");
        CHECK_INT(sess_save(&s, &c), 0);
        CHECK_INT(sess_load(&s2, id1, &back), 0);
        CHECK_INT(back.n, 4);
        CHECK(back.n == 4 && strstr(back.m[2].json, "instead") != 0 && strstr(back.m[2].json, "tool_result") == 0);
    }
    /* a branch, a second session; the index: newest first, names found */
    CHECK_INT(sess_branch(&s, &c, 0x12345679UL * 1000UL), 0);
    CHECK_STR(s.id, "12345679");
    CHECK_STR(s.title, "My Work (branch)");
    n = sess_list(&s, l, 8);
    CHECK_INT(n, 2);
    CHECK(n == 2 && !strcmp(l[0].id, "12345679") && !strcmp(l[1].id, "12345678"));
    CHECK(n == 2 && !strcmp(l[1].title, "My Work") && !strcmp(l[1].first, "first prompt"));
    CHECK_INT(sess_find(&s, "my work", id, sizeof(id)), 0);
    CHECK_STR(id, "12345678");
    CHECK_INT(sess_find(&s, "My", id, sizeof(id)), -2);
    CHECK_INT(sess_find(&s, "1234567", id, sizeof(id)), -2);
    CHECK_INT(sess_find(&s, "12345679", id, sizeof(id)), 0);
    CHECK_INT(sess_find(&s, "nothing", id, sizeof(id)), -1);
    /* a line cut by a crash is skipped */
    {
        char p[700];
        at(p, "home/projects/Work-proj/12345679.jsonl");
        sys.append(sys.u, p, "{\"type\":\"user\",\"index\":4,\"mess", 30);
        CHECK_INT(sess_load(&s2, "12345679", &back), 0);
        CHECK_INT(back.n, 4);
    }
    /* resuming lists it first */
    n = sess_list(&s, l, 8);
    CHECK(n == 2 && !strcmp(l[0].id, "12345679"));
    CHECK_INT(sess_load(&s2, "12345678", &back), 0);
    n = sess_list(&s, l, 8);
    CHECK(n == 2 && !strcmp(l[0].id, "12345678"));
    /* a new id never takes an existing file */
    sess_new(&s, 0x12345678UL * 1000UL);
    CHECK_STR(s.id, "1234567a");
    /* a first start: ENVARC:Claude itself is not there yet */
    at(home, "fresh-home");
    sess_init(&s, &sys, home, "Work:proj");
    sess_new(&s, 0x22222222UL * 1000UL);
    CHECK_INT(sess_save(&s, &c), 0);
    CHECK_INT(sys.kind(sys.u, s.file), 1);
    conv_free(&c);
    conv_free(&back);
}

/* ---- checkpoints ---- */

static void test_checkpoints(void)
{
    cl_checkpoints cp;
    char d[600], a[600], b[600], rep[600];
    char *f;
    int lost, k;
    static char big[CP_BYTES / 2 + 10];
    at(d, "t/cp");
    at(a, "proj/a.txt");
    at(b, "proj/new.txt");
    put("proj/a.txt", "one\n");
    cp_init(&cp, &sys, d);
    checkpoint_use(&cp);
    cp_turn(&cp, 0);
    CHECK_INT(checkpoint_before_write(a), 0);
    put("proj/a.txt", "two\n");
    CHECK_INT(cp_before_write(&cp, a), 0);      /* the same turn: the first one counts */
    CHECK_INT(cp.n, 1);
    cp_turn(&cp, 2);
    cp_before_write(&cp, a);
    put("proj/a.txt", "three\n");
    cp_before_write(&cp, b);
    put("proj/new.txt", "made\n");
    CHECK_INT(cp_files_since(&cp, 0), 2);
    CHECK_INT(cp_files_since(&cp, 2), 2);
    CHECK_INT(cp_files_since(&cp, 3), 0);
    CHECK_INT(cp.n_snaps, 3);
    /* back to before turn 2: a.txt "two", new.txt gone */
    k = cp_restore(&cp, 2, &lost, rep, sizeof(rep));
    CHECK_INT(k, 2);
    CHECK_INT(lost, 0);
    f = slurp("proj/a.txt");
    CHECK_STR(f ? f : "", "two\n");
    free(f);
    CHECK_INT(sys.kind(sys.u, b), 0);
    CHECK(strstr(rep, "restored ") && strstr(rep, "deleted "));
    /* back to before turn 0: "one" */
    k = cp_restore(&cp, 0, &lost, rep, sizeof(rep));
    CHECK_INT(k, 1);
    f = slurp("proj/a.txt");
    CHECK_STR(f ? f : "", "one\n");
    free(f);
    CHECK_INT(cp.n, 0);
    CHECK_INT(cp.bytes, 0);
    /* too large to keep: noted, and /rewind says it cannot put it back */
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;
    put("proj/a.txt", big);
    cp_turn(&cp, 5);
    CHECK_INT(cp_before_write(&cp, a), -1);
    k = cp_restore(&cp, 5, &lost, rep, sizeof(rep));
    CHECK_INT(k, 0);
    CHECK_INT(lost, 1);
    CHECK(strstr(rep, "could not restore") != 0);
    cp_free(&cp);
    CHECK_INT(checkpoint_before_write(a), 0);   /* none in use: nothing */
}

/* A4 gaps: the new settings keys, bypassPermissions kept out of a
 * project's files (H5), the auto-compact window's forms, --setting-sources'
 * skip, the agent and skill frontmatter keys (X3 X4), --agents JSON */
static void test_gaps(void)
{
    cl_settings s;
    cl_defs ds;
    cl_def d;
    char home[600], root[600], err[200];
    static const char agent[] = "---\nname: rev\ndescription: Reviews\ntools: Read, Bash\n"
                                "disallowedTools:\n  - Bash\nmaxTurns: 7\neffort: high\nskills: [a, b]\n"
                                "permissionMode: acceptEdits\n---\nPROMPT\n";
    static const char skill[] = "---\ndescription: A skill\nwhen_to_use: when asked\nuser-invocable: false\n"
                                "context: fork\nagent: Explore\narguments: [x]\n---\nBODY\n";
    at(home, "gh");
    at(root, "gp");
    mk("gh");
    mk("gp");
    mk("gp/.claude");
    put("gh/settings.json", "{\"disableAllHooks\":true,\"verbose\":true,\"agent\":\"rev\",\"autoCompactWindow\":\"500k\","
                            "\"autoMemoryEnabled\":false,\"statusLine\":{\"command\":\"x\",\"hideVimModeIndicator\":true},"
                            "\"claudeMdExcludes\":[\"**/other/CLAUDE.md\"],\"permissions\":{\"defaultMode\":\"plan\"}}");
    put("gp/.claude/settings.json", "{\"permissions\":{\"defaultMode\":\"bypassPermissions\"}}");
    put("gp/.claude/settings.local.json", "{\"permissions\":{\"defaultMode\":\"auto\"}}");
    cfg_init(&s);
    CHECK_INT(cfg_load(&s, &sys, home, root), 3);
    CHECK_INT(s.no_hooks, 1);
    CHECK_INT(s.verbose, 1);
    CHECK_STR(s.agent, "rev");
    CHECK_INT((int)(s.compact_window / 1000), 500);
    CHECK_INT(s.auto_memory, 0);
    CHECK_INT(s.hide_vim, 1);
    CHECK(s.nmdx == 1 && !strcmp(s.md_excludes[0], "**/other/CLAUDE.md"));
    CHECK_STR(s.default_mode, "plan");      /* the project's bypassPermissions and auto do not count */
    CHECK_INT(s.model_src, -1);
    cfg_free(&s);
    /* --setting-sources user: the project's files are not read */
    cfg_init(&s);
    s.skip = (1u << CFG_PROJECT) | (1u << CFG_LOCAL);
    CHECK_INT(cfg_load(&s, &sys, home, root), 1);
    cfg_free(&s);
    /* the auto-compact window's forms (Claude Code's) */
    CHECK_INT((int)cfg_window_parse("200000"), 200000);
    CHECK_INT((int)cfg_window_parse("500k"), 500000);
    CHECK_INT((int)cfg_window_parse("1M"), 1000000);
    CHECK_INT((int)cfg_window_parse("200"), 200000);
    CHECK_INT((int)cfg_window_parse("auto"), -1);
    CHECK_INT((int)cfg_window_parse("50"), 0);
    CHECK_INT((int)cfg_window_parse("2M"), 0);
    /* the agent's and the skill's keys */
    memset(&d, 0, sizeof(d));
    CHECK_INT(defs_parse(agent, (long)sizeof(agent) - 1, &d), 0);
    CHECK_STR(d.tools, "Read, Bash");
    CHECK_STR(d.deny_tools, "Bash");
    CHECK_INT(d.max_turns, 7);
    CHECK_STR(d.effort, "high");
    CHECK_STR(d.skills, "a, b");
    CHECK_STR(d.perm_mode, "acceptEdits");
    def_free(&d);
    memset(&d, 0, sizeof(d));
    CHECK_INT(defs_parse(skill, (long)sizeof(skill) - 1, &d), 0);
    CHECK_STR(d.when, "when asked");
    CHECK_INT(d.no_user, 1);
    CHECK_INT(d.fork, 1);
    CHECK_STR(d.agent, "Explore");
    CHECK_STR(d.arg_names, "x");
    def_free(&d);
    /* --agents: inline JSON, first in the list (it hides a file's agent of its name) */
    defs_init(&ds);
    CHECK_INT(defs_add_agents_json(&ds, "{\"j\":{\"description\":\"J agent\",\"prompt\":\"JP\",\"tools\":[\"Read\","
                                        "\"Grep\"],\"maxTurns\":3,\"permissionMode\":\"plan\"}}",
                                   err, sizeof(err)),
              0);
    CHECK(defs_find(&ds, DEF_AGENT, "j") != 0);
    if (defs_find(&ds, DEF_AGENT, "j")) {
        const cl_def *j = defs_find(&ds, DEF_AGENT, "j");
        CHECK_STR(j->body, "JP");
        CHECK_STR(j->tools, "Read, Grep");
        CHECK_INT(j->max_turns, 3);
        CHECK_STR(j->perm_mode, "plan");
        CHECK_INT(j->src, CFG_SESSION);
    }
    CHECK_INT(defs_add_agents_json(&ds, "{\"k\":{\"prompt\":\"no description\"}}", err, sizeof(err)), -1);
    CHECK(strstr(err, "needs a description and a prompt") != 0);
    CHECK_INT(defs_add_agents_json(&ds, "[1]", err, sizeof(err)), -1);
    defs_drop(&ds, DEF_AGENT);
    CHECK_INT(defs_count(&ds, DEF_AGENT), 0);
    defs_free(&ds);
    /* X6: a hook matcher "Agent" is Claude Code's newer name of Task */
    CHECK_INT(hooks_match("Agent", "Task"), 1);
    CHECK_INT(hooks_match("Bash|Agent", "Task"), 1);
    CHECK_INT(hooks_match("Agent", "Read"), 0);
    /* Claude Code's matcher rules: a plain list is exact names, anything
     * else a regular expression, unanchored */
    CHECK_INT(hooks_match("Bash", "BashOutput"), 0);
    CHECK_INT(hooks_match("Edit, Write", "Write"), 1);
    CHECK_INT(hooks_match("Edit.*", "MultiEdit"), 1);
    CHECK_INT(hooks_match("^Edit$", "MultiEdit"), 0);
    CHECK_INT(hooks_match("^Edit$", "Edit"), 1);
    CHECK_INT(hooks_match("^(Read|Grep)$", "Grep"), 1);
}

void suite_claude_config(void)
{
    char cmd[600];
    tree();
    test_settings();
    test_rules();
    test_models();
    test_memory();
    test_defs();
    test_hooks();
    test_sessions();
    test_checkpoints();
    test_gaps();
    strcpy(cmd, "rm -rf ");
    strcat(cmd, dir);
    if (system(cmd))
        printf("  [ERROR] could not remove %s\n", dir);
}
