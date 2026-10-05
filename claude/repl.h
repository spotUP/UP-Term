/* repl -- the Claude client's core: the prompt loop, slash commands, one
 * turn (request, streamed answer, tool rounds until Claude is done), the
 * transport with keep-alive, retries and Ctrl+C. Everything machine-
 * specific comes in through cl_io, cl_net and cl_sys, so the host tests
 * drive this exact code against recorded streams (tests/test_claude_repl.c
 * is the reachability test); claude/main_amiga.c is only the wiring.
 *
 * A turn that does not complete -- Ctrl+C, a refusal, an error, a cut
 * tool call -- leaves the history as it was before the prompt (or before
 * the unanswered request), so the conversation always stays valid. */
#ifndef CL_REPL_H
#define CL_REPL_H

#include "net.h"
#include "sys.h"
#include "http.h"
#include "sse.h"
#include "stream.h"
#include "conv.h"
#include "tools.h"
#include "ui.h"
#include "config.h"
#include "memory.h"
#include "commands.h"
#include "hooks.h"
#include "session.h"
#include "checkpoint.h"

#define CL_DEFAULT_URL    "https://api.anthropic.com/v1/messages"
#define CL_DEFAULT_MODEL  "claude-opus-5-5"
#define CL_DEFAULT_EFFORT "medium"
#define CL_MAX_TOKENS     64000L
#define CL_TRIES          5
#define CL_HOME           "ENVARC:Claude"   /* Claude Code's ~/.claude; CLAUDE_CONFIG_DIR overrides */
#define CL_TMP            "T:"              /* CLAUDE_CODE_TMPDIR overrides */
#define CL_COMPACT_PCT    92                /* auto-compact when this much of the window is used */

/* A4 WP4: an observer of the turns (print mode's output, claude/print.c).
 * Each may be 0. */
typedef struct cl_feed {
    void *u;
    /* a message added to the conversation by a turn: an answer (user 0;
     * r->st still holds its stream) or a round's tool results (user 1) */
    void (*message)(void *u, int user, const char *json, long n);
    /* a raw stream event of the conversation's own requests */
    void (*event)(void *u, const char *ev, const char *data, long n);
    /* a tool call refused without a question (a deny rule, no one to ask) */
    void (*denied)(void *u, const char *tool, const char *id, const char *input, long n);
    /* A4 gaps: a subagent's message (its prompt, each answer, each round's
     * tool results); parent the id of the Task call that runs it, st the
     * answer's stream (0 for a user message) */
    void (*sub)(void *u, const char *parent, int user, const char *json, long n, const cl_stream *st);
    /* A4 gaps: a request failed and is tried again (system/api_retry):
     * attempt from 1, the delay, the HTTP status (0 none), the error kind */
    void (*retry)(void *u, int attempt, int max, long delay_ms, int status, const char *error);
    /* A4 gaps: a hook started (done 0) or finished (its exit code and
     * output): --include-hook-events */
    void (*hook)(void *u, const char *event, const char *cmd, int done, long rc, const char *out, long n);
} cl_feed;

/* who answers a permission question (A4 WP4) */
enum {
    ASKP_ASK,                   /* the user */
    ASKP_DENY,                  /* nobody: denied (dontAsk; print mode); reads in the root still run */
    ASKP_BYPASS                 /* yes to all (bypassPermissions); explicit ask rules still ask */
};
/* how the last turn ended */
enum { TURN_OK, TURN_FAIL, TURN_CANCEL, TURN_MAX_TURNS, TURN_BUDGET };

typedef struct cl_repl {
    cl_io *io;
    cl_net *net;
    cl_sys *sys;
    cl_ui ui;
    cl_render render;           /* the answer's renderer (ui_plain by default) */
    http_url url;
    const char *key;            /* never shown, never logged */
    char model[64];
    char effort[16];
    long max_tokens;
    char *system;
    cl_conv conv;
    cl_tools tools;
    int connected;
    int debug;
    /* the request in flight */
    http_resp resp;
    sse sse;
    cl_stream st;
    jw errbody;
    int shown;                  /* answer text went to the screen */
    /* the screen of its own (ledger A3), 0 in the line mode */
    struct cl_tui *tui;
    struct cl_show *show;
    long ctx_used;              /* the last request's tokens: the context in use */
    long turn_out;              /* output tokens of the turn's finished requests */
    long chars;                 /* answer bytes of the request in flight */
    char session[256];          /* A2's single saved conversation: read by /resume when there
                                 * is no session yet (the migration), no longer written */
    /* A4 WP3: settings, memory, definitions, hooks, sessions, checkpoints */
    char home[256];             /* ENVARC:Claude */
    char tmp[256];              /* T: */
    cl_settings cfg;
    cl_memory mem;
    cl_defs defs;
    cl_hooks hooks;
    cl_session sess;
    cl_checkpoints cp;
    struct cl_cmd *menu;              /* the slash menu: the built-in commands and the custom ones */
    int nmenu;
    char style[64];             /* the output style, "" Default */
    char fallback[64];          /* the fallback model, "" none */
    int auto_compact;           /* 1 on (the default) */
    int busy_fail;              /* the last request failed as overloaded */
    int rule_now;               /* RULE_* for the call being run: ALLOW answers the question */
    const char *turn_tools;     /* a custom command's allowed-tools for its turn, 0 none */
    jw pending;                 /* context for the next prompt (SessionStart and prompt hooks) */
    char *todos;                /* the last todo_write input (/todos) */
    char keybuf[512];           /* a key from /login */
    int await_key;              /* the next line typed is the key (/login) */
    char status_text[512];      /* the statusLine command's last output (its lines, SGR kept):
                                 * the footer's own row(s) above the status line */
    unsigned long status_ms;    /* when it last ran (io->ms), 0 never */
    int status_due;             /* an event asked for a run the 300 ms throttle held back */
    int status_mode, status_vim;    /* the permission mode and vim state it last saw */
    long n_status_runs;         /* the tests' sentinel */
    /* the extensions WP2's Task / Skill / SlashCommand tools see (ext.h),
     * built from defs at each repl_load (policy.c) */
    cl_ext ext;
    cl_agent *x_agents;
    cl_skill *x_skills;
    cl_command *x_cmds;
    int nx_agents, nx_skills, nx_cmds;
    cl_tools *at;               /* the tools whose call runs now: the conversation's or a subagent's */
    long n_rule_allow, n_rule_deny, n_cmds_run;     /* the tests' sentinels */
    long n_skills_run;          /* the tests' sentinel: skills expanded for a turn */
    char turn_buf[512];         /* turn_tools with its ${CLAUDE_*} put in */
    /* A4 WP4: the command line (cli.c) and print mode (print.c) */
    const cl_feed *feed;
    int ask_policy;             /* ASKP_* */
    int no_person;              /* print mode: no one to ask (questions denied, choices declined) */
    int max_turns;              /* responses a turn may have before it stops, 0 no limit */
    unsigned long budget_micro; /* spend allowed (US dollars * 1e6), 0 no limit; ... */
    unsigned long budget_base;  /* ... counted from this cost_micro */
    int turn_rc;                /* TURN_*: how the last turn ended */
    long n_responses;           /* answers received (print mode's num_turns) */
    unsigned long api_ms;       /* time spent in requests */
    int quiet_req;              /* a tool's own request is in flight (no feed events) */
    const char *cur_id;         /* the tool_use id being run */
    char *sys_replace;          /* --system-prompt: replaces the default text, 0 none */
    char *sys_append;           /* --append-system-prompt: added at the end, 0 none */
    char *layer[2];             /* --settings and the flags as settings JSON (CFG_SESSION), 0 none */
    const char *first;          /* the prompt the session starts with (Claude "prompt"), 0 none */
    /* the A4 gaps (thoughts/shared/plans/2026-10-05-a4-gaps-progress.md) */
    int bare;                   /* --bare: no CLAUDE.md, hooks, commands, skills, agents; Bash, Read, Edit */
    int safe;                   /* --safe-mode: no customisation (memory, hooks, defs, styles, status line) */
    int no_slash;               /* --disable-slash-commands: no skills, no custom commands */
    unsigned sources;           /* --setting-sources: bit per CFG_USER/PROJECT/LOCAL (0: all) */
    char *agents_json;          /* --agents: {"name": {description, prompt, tools, model}} */
    char *schema;               /* --json-schema: the StructuredOutput tool's input schema */
    char *structured;           /* the StructuredOutput call's input (the structured output) */
    long compact_window;        /* --autocompact: tokens, -1 auto (the model's), 0 not given */
    int start_due;              /* SessionStart "startup" not run yet (after the flags are applied) */
    const char *blocks;         /* the next turn's prompt as content blocks (comma-separated), 0 text */
    long blocks_n;
    char betas[256];            /* --betas, joined with commas */
    int verbose;                /* "verbose": results unfolded at the screen; 2 --verbose (wins) */
    int prompts_none;           /* --permission-prompts none */
    unsigned long t_start;      /* the session's start (io->ms), for the durations */
    const char *log_path;       /* where the debug log goes (main sets it; /debug names it), 0 unknown */
    char btw[512];              /* the last /btw answer (/btw alone shows it) */
    char launch_root[256];      /* the start directory at launch (status line's project_dir; /cd moves the root) */
    char agent_name[64];        /* --agent / the "agent" setting in force, "" none */
    long lines_added, lines_removed;    /* Write/Edit lines changed this session (status line's cost) */
    long pend_add, pend_del;    /* the change being asked about (its preview's counts) */
    const char *api_failed;     /* the turn's request failed: StopFailure's error kind, 0 none */
    const char *end_reason;     /* SessionEnd's reason: prompt_input_exit after /exit, else other */
    unsigned long idle_from;    /* when Claude last finished (io->ms): Notification idle_prompt */
    int idle_told;
    long cfg_mtime[3];          /* the settings files' times when read (ConfigChange) */
    int began;                  /* the session has started (SessionStart ran) */
    char goal[400];             /* /goal: the condition Claude works toward, "" none */
    char *first_msg;            /* a SessionStart hook's initialUserMessage (print mode's first turn), 0 none */
    char nested[8][300];        /* the directories whose .claude/skills were loaded on the way */
    int n_nested;
    int goal_rounds;            /* the turns a goal has added (capped) */
    int no_dynamic;             /* --exclude-dynamic-system-prompt-sections: auto memory's place in the
                                 * first prompt, not the system prompt */
    long n_copies;              /* the tests' sentinel: /copy runs that reached the clipboard */
    /* A4 gaps 2 (thoughts/shared/plans/2026-10-05-a4-gaps2-progress.md) */
    int in_turn;                /* a turn runs (the idle tick does not start one) */
    int woke;                   /* the line came from sched_wake, not the keyboard */
    long n_cron_fired;          /* the tests' sentinel: cron jobs fired */
    int todo_optin;             /* --allowedTools / --tools named a task tool */
    struct cl_watch *watch;     /* FileChanged's watched files (watch.c) */
    int untrusted;              /* the workspace trust question not (yet) answered yes: held back --
                                 * interactive: every settings file's hooks; both: the project's allow
                                 * rules and additionalDirectories */
    int trusted_dir;            /* the start directory is trusted (remembered, or answered now) */
    long n_file_changed;        /* the tests' sentinel: FileChanged events */
    unsigned long t_open, t_first;  /* ping: connect and first-byte times */
    char head[1024];
    char buf[4096];
} cl_repl;

/* 0, or -1 with the reason shown */
int repl_init(cl_repl *r, cl_io *io, cl_net *net, cl_sys *sys, const char *url, const char *key,
              const char *root);
void repl_free(cl_repl *r);
/* one line typed at the prompt: 1 when it was /exit */
int repl_line(cl_repl *r, const char *line);
/* the loop: until /exit or the end of input */
void repl_run(cl_repl *r);
/* The screen of its own (Claude Code's look, ledger A3): raw mode, the
 * input box, the status line. 0 when it started; -1 (out of memory, no
 * io->read, or the console refused raw mode): the line mode stays. */
int repl_screen(cl_repl *r);
/* the model's context window in tokens */
long repl_window(const char *model);
/* the transport check: a one-token request, its status and times; 0 when
 * the API answered 200 */
int repl_ping(cl_repl *r);
/* A4 WP3. The settings, memory, custom commands, agents, skills and output
 * styles (re)read for the root, the system prompt built again, the slash
 * menu rebuilt: 0, -1 out of memory. repl_init calls it. */
int repl_load(cl_repl *r);
/* the most recent session of this root resumed (--continue): 0, -1 none */
int repl_continue(cl_repl *r);
/* A session by id or title resumed: 0, -1 */
int repl_resume_session(cl_repl *r, const char *name);
/* Rewind to before the user message at index msg (a prompt that starts a
 * user message): the files written since put back (code), the
 * conversation cut there (conv). 0, -1 not a prompt. The Esc Esc menu
 * (WP1 1.7) calls this with the message the user picked. */
int repl_rewind(cl_repl *r, int msg, int code, int conv);
/* The prompts the conversation can be rewound to: message indexes, the
 * newest first; their count. */
int repl_prompts(const cl_repl *r, int *msg, int max);

/* A4 gaps: a prompt given as content blocks (stream-json input with an
 * image or a document: the blocks go to the API as sent, base64 and all);
 * text is its text part, for the UserPromptSubmit hook. 0. */
int repl_blocks(cl_repl *r, const char *text, long tn, const char *blocks, long bn);

/* Setup hooks of a trigger ("init", "maintenance"; 0 none), and with start
 * SessionStart as at the first line (--init-only) */
void repl_setup(cl_repl *r, const char *trigger, int start);
/* A4 WP4: an https endpoint and no key yet (the start goes to /login) */
int repl_need_key(const cl_repl *r);

/* the start of a key read from a file or a variable, cleaned in place:
 * white space trimmed; 0 when it is usable */
int cl_key_clean(char *key);

#endif
