/* C:Claude's command line (A4 WP4, claude/cli.c): Claude Code's flags and
 * the AmigaDOS keywords, each alone and mixed, the old A2 keywords kept,
 * ReadArgs' PROMPT/F rule, AmigaDOS quoting, and the errors Claude Code
 * gives. What the flags do to a session (cli_apply, print mode) is tested
 * through the REPL core in tests/test_claude_repl.c (test_wp4). */
#include <stdlib.h>
#include "harness.h"
#include "../claude/cli.h"

static cl_cli c;

static int parse(const char *line)
{
    cli_free(&c);
    cli_init(&c);
    return cli_parse_line(&c, line);
}

static const char *prompt(void)
{
    return c.prompt ? c.prompt : "(none)";
}

static void test_split(void)
{
    cl_args a;
    CHECK_INT(cli_split("  one \"two words\" PROMPT=\"a *\"q*\" b*N\" x\"y z\"w **\n", &a), 0);
    CHECK_INT(a.n, 5);
    if (a.n == 5) {
        CHECK_STR(a.v[0], "one");
        CHECK_INT(a.quoted[0], 0);
        CHECK_STR(a.v[1], "two words");
        CHECK_INT(a.quoted[1], 1);
        CHECK_STR(a.v[2], "PROMPT=a \"q\" b\n");
        CHECK_STR(a.v[3], "xy zw");
        CHECK_STR(a.v[4], "**");        /* a star outside quotes is itself */
    }
    cli_args_free(&a);
    CHECK_INT(cli_split("--tools \"\"", &a), 0);
    CHECK_INT(a.n, 2);
    if (a.n == 2)
        CHECK_STR(a.v[1], "");
    cli_args_free(&a);
    CHECK_INT(cli_split("", &a), 0);
    CHECK_INT(a.n, 0);
    cli_args_free(&a);
}

static void test_unix(void)
{
    CHECK_INT(parse("-p \"explain this\" --model haiku --output-format json --max-turns 3 --max-budget-usd 0.25 "
                    "--allowedTools \"Bash(make test *)\" Read,Grep --verbose --effort high"),
              0);
    CHECK_INT(c.print, 1);
    CHECK_STR(prompt(), "explain this");
    CHECK_STR(c.model, "haiku");
    CHECK_INT(c.out, CLI_JSON);
    CHECK_INT(c.max_turns, 3);
    CHECK_INT(c.has_budget, 1);
    CHECK_INT((long)c.budget_micro, 250000);
    CHECK_INT(c.allow.n, 3);
    if (c.allow.n == 3) {
        CHECK_STR(c.allow.v[0], "Bash(make test *)");
        CHECK_STR(c.allow.v[1], "Read");
        CHECK_STR(c.allow.v[2], "Grep");
    }
    CHECK_INT(c.verbose, 1);
    CHECK_STR(c.effort, "high");

    /* --flag=value, short groups, -r with and without a name, -n */
    CHECK_INT(parse("--model=sonnet -pc hello"), 0);
    CHECK_STR(c.model, "sonnet");
    CHECK_INT(c.print, 1);
    CHECK_INT(c.cont, 1);
    CHECK_STR(prompt(), "hello");
    CHECK_INT(parse("-r"), 0);
    CHECK_INT(c.resume, 1);
    CHECK_STR(c.resume_name, "");
    CHECK_INT(parse("-r auth-refactor \"Finish this PR\" -n work"), 0);
    CHECK_STR(c.resume_name, "auth-refactor");
    CHECK_STR(prompt(), "Finish this PR");
    CHECK_STR(c.name, "work");
    CHECK_INT(parse("--resume=abc --fork-session"), 0);
    CHECK_STR(c.resume_name, "abc");
    CHECK_INT(c.fork, 1);

    /* the rest of the flags */
    CHECK_INT(parse("-p --output-format stream-json --verbose --include-partial-messages --input-format stream-json "
                    "--no-session-persistence --permission-mode acceptEdits --tools \"\" --disallowedTools Edit "
                    "\"Bash(rm *)\" --add-dir RAM: Work:x --system-prompt \"Be brief\" --append-system-prompt-file "
                    "rules.txt --settings \"{}\" --agent Explore --fallback-model sonnet,haiku "
                    "--dangerously-skip-permissions"),
              0);
    CHECK_INT(c.out, CLI_STREAM);
    CHECK_INT(c.in, CLI_STREAM);
    CHECK_INT(c.partial, 1);
    CHECK_INT(c.no_persist, 1);
    CHECK_STR(c.perm, "acceptEdits");
    CHECK_INT(c.has_tools, 1);
    CHECK_STR(c.tools ? c.tools : "(none)", "");
    CHECK_INT(c.deny.n, 2);
    CHECK_INT(c.dirs.n, 2);
    CHECK_STR(c.sys_prompt ? c.sys_prompt : "", "Be brief");
    CHECK_STR(c.app_file ? c.app_file : "", "rules.txt");
    CHECK_STR(c.settings ? c.settings : "", "{}");
    CHECK_STR(c.agent, "Explore");
    CHECK_STR(c.fallback, "sonnet");    /* one fallback here: the chain's first */
    CHECK_INT(c.skip_perms, 1);
    CHECK_STR(prompt(), "(none)");

    /* -- ends the flags */
    CHECK_INT(parse("-p -- -5 is negative"), 0);
    CHECK_STR(prompt(), "-5 is negative");
    CHECK_INT(parse("--version"), 0);
    CHECK_INT(c.version, 1);
    CHECK_INT(parse("-h"), 0);
    CHECK_INT(c.help, 1);
    {
        int i, found = 0;
        for (i = 0; cli_usage(i); i++)
            found |= strstr(cli_usage(i), "--output-format") != 0;
        CHECK(found);
    }
}

static void test_amiga(void)
{
    /* the A2 keywords, as they always were */
    CHECK_INT(parse("MODEL=opus EFFORT=high URL=http://10.0.2.2:8080/v1/messages ROOT=RAM: CONTINUE FALLBACK=haiku "
                    "DEBUG PLAIN what is new"),
              0);
    CHECK_STR(c.model, "opus");
    CHECK_STR(c.effort, "high");
    CHECK_STR(c.url, "http://10.0.2.2:8080/v1/messages");
    CHECK_STR(c.root, "RAM:");
    CHECK_INT(c.cont, 1);
    CHECK_STR(c.fallback, "haiku");
    CHECK_INT(c.debug, 1);
    CHECK_INT(c.plain, 1);
    CHECK_STR(prompt(), "what is new");
    CHECK_INT(parse("PING"), 0);
    CHECK_INT(c.ping, 1);

    /* the new ones: KEY=value, KEY value, case and '-' as one likes */
    CHECK_INT(parse("print model haiku Output-Format=json MAXTURNS=2 max-budget-usd 1.5 NAME=x "
                    "ALLOWEDTOOLS=Read,Glob explain the startup"),
              0);
    CHECK_INT(c.print, 1);
    CHECK_STR(c.model, "haiku");
    CHECK_INT(c.out, CLI_JSON);
    CHECK_INT(c.max_turns, 2);
    CHECK_INT((long)c.budget_micro, 1500000);
    CHECK_STR(c.name, "x");
    CHECK_INT(c.allow.n, 2);
    CHECK_STR(prompt(), "explain the startup");
    CHECK_INT(parse("P C R=abc FORK-SESSION go on"), 0);
    CHECK_INT(c.print, 1);
    CHECK_INT(c.cont, 1);
    CHECK_STR(c.resume_name, "abc");
    CHECK_INT(c.fork, 1);
    CHECK_STR(prompt(), "go on");

    /* PROMPT/F: once the prompt starts, the rest is prompt; quoted words are never keywords */
    CHECK_INT(parse("tell me what PRINT does"), 0);
    CHECK_INT(c.print, 0);
    CHECK_STR(prompt(), "tell me what PRINT does");
    CHECK_INT(parse("\"PRINT\" it"), 0);
    CHECK_INT(c.print, 0);
    CHECK_STR(prompt(), "PRINT it");
    CHECK_INT(parse("PROMPT=hello MODEL=haiku"), 0);
    CHECK_STR(prompt(), "hello MODEL=haiku");
    CHECK_STR(c.model, "");
    CHECK_INT(parse("PROMPT \"hi there\""), 0);
    CHECK_STR(prompt(), "hi there");

    /* both forms mixed */
    CHECK_INT(parse("-p MODEL=haiku --output-format json hello"), 0);
    CHECK_INT(c.print, 1);
    CHECK_STR(c.model, "haiku");
    CHECK_INT(c.out, CLI_JSON);
    CHECK_STR(prompt(), "hello");

    /* ReadArgs' question */
    CHECK_INT(parse("?"), 0);
    CHECK_INT(c.ask_template, 1);
    CHECK(!strncmp(CLI_TEMPLATE, "PROMPT/F,PRINT=P/S,MODEL/K", 26));
}

static void test_errors(void)
{
    CHECK_INT(parse("--frobnicate"), -1);
    CHECK_STR(c.err, "error: unknown option '--frobnicate'");
    CHECK_INT(parse("-x"), -1);
    CHECK_STR(c.err, "error: unknown option '-x'");
    CHECK_INT(parse("-p --output-format yaml hi"), -1);
    CHECK_STR(c.err, "error: option '--output-format' argument 'yaml' is invalid. Allowed choices are text, json, "
                     "stream-json.");
    CHECK_INT(parse("PRINT OUTPUT-FORMAT=yaml"), -1);
    CHECK(strstr(c.err, "'OUTPUT-FORMAT' argument 'yaml' is invalid") != 0);
    CHECK_INT(parse("-p --output-format stream-json hi"), -1);
    CHECK_STR(c.err, "Error: When using --print, --output-format=stream-json requires --verbose");
    CHECK_INT(parse("-p --include-partial-messages hi"), -1);
    CHECK_INT(parse("--fork-session"), -1);
    CHECK_STR(c.err, "Error: --fork-session requires --continue or --resume");
    CHECK_INT(parse("--max-turns 0 -p hi"), -1);
    CHECK_INT(parse("--max-budget-usd lots -p hi"), -1);
    CHECK_INT(parse("--permission-mode auto"), -1);
    CHECK(strstr(c.err, "classifier") != 0);
    CHECK_INT(parse("--permission-mode sometimes"), -1);
    CHECK_INT(parse("--model"), -1);
    CHECK_STR(c.err, "error: option '--model' argument missing");
    CHECK_INT(parse("MODEL"), -1);
    CHECK_STR(c.err, "Claude: MODEL needs a value");
    CHECK_INT(parse("PRINT=yes"), -1);
    CHECK_INT(parse("--print=yes"), -1);
    CHECK_INT(parse("-p -r"), -1);
    CHECK_STR(c.err, "Error: --resume needs a session ID or name in print mode");
    CHECK_INT(parse("--system-prompt a --system-prompt-file b"), -1);
    CHECK_INT(parse("--input-format stream-json"), -1);
}

/* ENVARC:Claude/remote: plain "Claude" with no key goes to Claude Code on
 * the NAS (W49) -- the file as Install writes it, and as a user edits it */
static void test_remote(void)
{
    char host[64];
    long port = 0;
    CHECK_INT(cli_remote_parse("; Claude Code on the NAS\n192.0.2.10 2323\n", host, sizeof(host), &port), 1);
    CHECK_STR(host, "192.0.2.10");
    CHECK_INT(port, 2323);
    CHECK_INT(cli_remote_parse("\n# comment\n  nas.local\r\n", host, sizeof(host), &port), 1);
    CHECK_STR(host, "nas.local");
    CHECK_INT(port, 2323);                        /* the default port */
    CHECK_INT(cli_remote_parse("megadrive 23\n", host, sizeof(host), &port), 1);
    CHECK_INT(port, 23);
    CHECK_INT(cli_remote_parse("; only comments\n\n", host, sizeof(host), &port), 0);
    CHECK_INT(cli_remote_parse("", host, sizeof(host), &port), 0);
    CHECK_INT(cli_remote_parse("host 70000\n", host, sizeof(host), &port), 0);
    CHECK_INT(cli_remote_parse("host 0\n", host, sizeof(host), &port), 0);
    CHECK_INT(cli_remote_parse("a-very-long-host-name-that-does-not-fit\n", host, 8, &port), 0);
}

void suite_claude_cli(void)
{
    cli_init(&c);
    test_split();
    test_unix();
    test_amiga();
    test_errors();
    test_remote();
    cli_free(&c);
}
