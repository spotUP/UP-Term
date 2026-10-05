/* cli -- C:Claude's command line (A4 WP4, rows 4.1-4.5): Claude Code's
 * flags and the AmigaDOS keywords, in one parser, and what they do to the
 * REPL core.
 *
 * Both forms, mixed as one likes:
 *
 *   Claude -p "explain" --model haiku --output-format json
 *   Claude PRINT MODEL=haiku OUTPUT-FORMAT=json explain
 *
 * A token starting with - is a Unix flag (--name value, --name=value, -p,
 * -pc); otherwise a keyword of the template below, case-insensitive with
 * any '-' ignored (MODEL=x, MODEL x, OUTPUTFORMAT=json, OUTPUT-FORMAT json);
 * otherwise the prompt. As ReadArgs' PROMPT/F: once the prompt has
 * started, keywords are part of it (flags are not; `--` ends the flags).
 * A quoted word is never a keyword. `Claude ?` shows the template and
 * reads the arguments from the next line, as ReadArgs does.
 *
 * The line is split as AmigaDOS splits it: blanks separate, "..." quotes,
 * and inside quotes *" is a quote, *N a newline, *E an escape, ** a star.
 *
 * What a flag does is Claude Code's (cli-reference, headless; fetched
 * 2026-10-05). The settings-like ones (--model --effort --fallback-model
 * --permission-mode --allowedTools --disallowedTools --add-dir) and
 * --settings become one more settings layer (CFG_SESSION), merged after
 * the files every time they are read, so they win and survive /cd.
 * Portable C89, host-tested (tests/test_claude_cli.c). */
#ifndef CL_CLI_H
#define CL_CLI_H

#include "repl.h"

#define CLI_TEMPLATE                                                                                           \
    "PROMPT/F,PRINT=P/S,MODEL/K,EFFORT/K,URL/K,ROOT/K,PING/S,DEBUG/S,PLAIN/S,CONTINUE=C/S,RESUME=R/K,"         \
    "NAME=N/K,FORK-SESSION/S,NO-SESSION-PERSISTENCE/S,FALLBACK=FALLBACK-MODEL/K,OUTPUT-FORMAT/K,"               \
    "INPUT-FORMAT/K,INCLUDE-PARTIAL-MESSAGES/S,PERMISSION-MODE/K,DANGEROUSLY-SKIP-PERMISSIONS/S,"               \
    "ALLOWED-TOOLS/K,DISALLOWED-TOOLS/K,TOOLS/K,ADD-DIR/K,SYSTEM-PROMPT/K,SYSTEM-PROMPT-FILE/K,"                \
    "APPEND-SYSTEM-PROMPT/K,APPEND-SYSTEM-PROMPT-FILE/K,SETTINGS/K,MAX-TURNS/K/N,MAX-BUDGET-USD/K,VERBOSE/S,"   \
    "AGENT/K,VERSION/S,HELP/S"
/* the template's second half (C89 caps a string literal at 509 characters):
 * `Claude ?` prints CLI_TEMPLATE then this */
#define CLI_TEMPLATE_MORE                                                                                      \
    ",SESSION-ID/K,JSON-SCHEMA/K,REPLAY-USER-MESSAGES/S,BARE/S,SAFE-MODE/S,AGENTS/K,"                          \
    "APPEND-SUBAGENT-SYSTEM-PROMPT/K,APPEND-SUBAGENT-SYSTEM-PROMPT-FILE/K,DISABLE-SLASH-COMMANDS/S,"            \
    "SETTING-SOURCES/K,BETAS/K,AUTOCOMPACT/K,FORWARD-SUBAGENT-TEXT/S,DEBUG-FILE/K,PERMISSION-PROMPTS/K,"        \
    "INIT/S,INIT-ONLY/S,MAINTENANCE/S,INCLUDE-HOOK-EVENTS/S,PROMPT-SUGGESTIONS/S,"                              \
    "EXCLUDE-DYNAMIC-SYSTEM-PROMPT-SECTIONS/S"

enum { CLI_TEXT, CLI_JSON, CLI_STREAM };
/* Claude Code's subcommands that exist here */
enum { SUB_NONE, SUB_DOCTOR, SUB_AUTH_STATUS, SUB_AUTH_LOGIN, SUB_AUTH_LOGOUT, SUB_PURGE };

typedef struct cl_strs {
    char **v;
    int n, cap;
} cl_strs;

typedef struct cl_cli {
    int print, ping, debug, plain, verbose, partial, version, help, ask_template;
    int cont, fork, no_persist, resume;     /* resume: -r given (resume_name "" = the picker) */
    int skip_perms;                         /* --dangerously-skip-permissions */
    int out, in;                            /* CLI_TEXT / CLI_JSON / CLI_STREAM */
    int max_turns;                          /* 0 none */
    int has_budget;
    unsigned long budget_micro;             /* --max-budget-usd in US dollars * 1e6 */
    int has_tools;                          /* --tools given ("" = none) */
    char *prompt;                           /* the words of the prompt, joined; 0 none */
    char resume_name[128];
    char name[96];
    char model[64], effort[16], fallback[64], perm[24], agent[64];
    char url[256], root[256];
    char *tools, *sys_prompt, *sys_file, *app_prompt, *app_file, *settings;
    cl_strs allow, deny, dirs;
    char key_source[24];                    /* print mode's apiKeySource (main sets it), "" none */
    /* the A4 gaps flags */
    char session_id[40];                    /* --session-id: a UUID */
    char *schema;                           /* --json-schema: the schema's JSON */
    int replay;                             /* --replay-user-messages */
    int bare, safe;                         /* --bare, --safe-mode */
    char *agents;                           /* --agents: JSON (or a file) of agents */
    char *sub_app, *sub_app_file;           /* --append-subagent-system-prompt(-file) */
    int no_slash;                           /* --disable-slash-commands */
    int has_sources;
    unsigned sources;                       /* --setting-sources: bit per CFG_USER/PROJECT/LOCAL */
    cl_strs betas;                          /* --betas */
    long autocompact;                       /* --autocompact: tokens, -1 auto, 0 not given */
    int fwd_sub;                            /* --forward-subagent-text */
    char debug_file[256];                   /* --debug-file */
    int prompts_none;                       /* --permission-prompts none */
    int init, init_only, maintenance;       /* Setup hooks: --init, --init-only, --maintenance */
    int hook_events;                        /* --include-hook-events */
    int suggestions;                        /* --prompt-suggestions */
    int no_dynamic;                         /* --exclude-dynamic-system-prompt-sections */
    char advisor[64];                       /* --advisor MODEL (A4 gaps 2), "" none */
    int snapshot;                           /* --system-prompt-snapshot: 1 on, 0 off, -1 not given */
    int sub;                                /* a subcommand: SUB_* (Claude doctor, auth ..., purge) */
    char sub_arg[256];                      /* its argument (purge's path) */
    int text;                               /* auth status --text */
    char err[300];
} cl_cli;

/* a command line split into words (AmigaDOS quoting) */
typedef struct cl_args {
    char **v;
    char *quoted;               /* 1 when word i was (partly) quoted */
    int n;
    char *buf;
} cl_args;

/* 0, -1 out of memory */
int cli_split(const char *line, cl_args *a);
void cli_args_free(cl_args *a);

void cli_init(cl_cli *c);
void cli_free(cl_cli *c);
/* The words parsed (quoted may be 0: nothing quoted): 0, or -1 with the
 * reason in c->err, Claude Code's wording where it has one. */
int cli_parse(cl_cli *c, int argc, char **argv, const char *quoted);
/* a whole line: cli_split + cli_parse */
int cli_parse_line(cl_cli *c, const char *line);

/* --help's text, line i (with its newline; 0 past the end), and --version's */
const char *cli_usage(int i);
const char *cli_version(void);

/* After repl_init: everything but the session flags applied to r (the
 * settings layer, the system prompt flags, --tools and --disallowedTools,
 * --agent, the permission mode, --max-turns / --max-budget-usd in print
 * mode, --no-session-persistence). 0, or -1 with the reason in c->err
 * (an --add-dir that is no directory, a file that cannot be read, an
 * agent that does not exist, an unusable --settings). */
int cli_apply(cl_cli *c, cl_repl *r);
/* The session flags: --continue, --resume NAME (or the picker, in the
 * screen), --fork-session, --name. 0, or -1 with the reason in c->err
 * (nothing to continue, no such session). */
int cli_session(cl_cli *c, cl_repl *r);

#endif
