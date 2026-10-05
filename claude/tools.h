/* tools -- C:Claude's tools, by Claude Code's names and input schemas
 * (ledger A4 WP2): Read, Write, Edit, MultiEdit, Glob, Grep, Bash,
 * BashOutput, KillShell, WebFetch, TodoWrite, AskUserQuestion,
 * ExitPlanMode, EnterPlanMode, Task, Skill, SlashCommand -- client tools
 * with strict JSON schemas -- and the API's web_search server tool, which
 * is only declared (the API runs it; its blocks are shown).
 *
 * Every input is checked against the schema the tool declares (schema.h:
 * the declared JSON is the one source of truth) before anything runs;
 * each call is shown and, by the permission rules, confirmed by the user;
 * each ends in one tool_result block (is_error on failure).
 *
 * Permissions (Claude Code's defaults): the read-only tools (Read, Glob,
 * Grep) run without a question inside the working directories -- the
 * start directory and the added ones (t->added); a write, an edit, a
 * command, a fetch, a skill or a slash command asks every time unless the
 * user allowed that tool for the session. A path outside the working
 * directories asks always. The permission rules (settings.json) and the
 * PreToolUse hooks decide before any of this (t->call, the REPL's
 * policy.c). No tool runs before the user has answered.
 * TodoWrite, BashOutput, KillShell and Task never ask (a subagent's own
 * tool calls do); AskUserQuestion and the plan-mode tools are questions
 * themselves.
 * Portable C89 over sys.h, host-tested (tests/test_claude_tools.c). */
#ifndef CL_TOOLS_H
#define CL_TOOLS_H

#include "json.h"
#include "sys.h"
#include "net.h"
#include "ext.h"

enum {
    T_READ, T_WRITE, T_EDIT, T_MULTIEDIT, T_GLOB, T_GREP, T_BASH, T_BASH_OUTPUT, T_KILL_SHELL,
    T_WEB_FETCH, T_TODO_WRITE, T_ASK_USER, T_EXIT_PLAN, T_ENTER_PLAN, T_TASK, T_SKILL, T_SLASH,
    T_COUNT
};
/* the API's server tool: shown, never run here */
#define T_WEB_SEARCH T_COUNT

/* The user's answer to a permission question. ASK_STOP: no, and the user
 * will tell Claude what to do instead -- this call and the rest of its
 * round are not run, and the turn ends after their results (stop is set). */
enum { ASK_NO, ASK_ONCE, ASK_SESSION, ASK_STOP };

/* The permission mode (Shift+Tab in the screen, ledger A3): the A2 rules;
 * accept edits -- Write, Edit and MultiEdit inside the start directory run
 * without a question; plan -- only what changes nothing runs, the others
 * are refused with a result that says so. */
enum { PERM_DEFAULT, PERM_ACCEPT, PERM_PLAN };

typedef struct cl_perm {
    unsigned long session;      /* bit per tool: allowed for the session */
    int mode;                   /* PERM_* */
} cl_perm;

int perm_read_only(int tool);
/* must the user be asked? */
int perm_must_ask(const cl_perm *p, int tool, int outside);
/* is the call refused by the mode (plan)? */
int perm_refused(const cl_perm *p, int tool);
/* the user chose "always this session" */
void perm_grant(cl_perm *p, int tool);

/* choose() flags */
#define CH_MULTI 1              /* several options may be picked */
#define CH_OTHER 2              /* the user may type an answer of their own */

struct cl_stream;
struct cl_shells;
struct cl_readset;

/* One request to the Messages API over the program's own transport, for
 * WebFetch's small model call and Task's subagent: body as conv_body
 * writes it; nothing of the answer reaches the screen. 0 with the answer
 * in *st (the caller stream_free()s it), -1 failed (the reason shown),
 * -2 stopped by the user. */
typedef struct cl_api {
    void *u;
    int (*send)(void *u, const char *body, long bn, struct cl_stream *st);
} cl_api;

typedef struct cl_tools {
    cl_sys *sys;
    char root[256];             /* the start directory, canonical */
    cl_perm perm;
    int timeout_s;              /* Bash's default limit */
    /* shared by a subagent's tools and its parent's (pointers, so both see one) */
    struct cl_readset *rs;      /* the files read: Write and Edit need a Read first */
    struct cl_shells *sh;       /* the background shells */
    cl_net *web;                /* WebFetch's connection (0: no WebFetch) */
    cl_api api;                 /* (send 0: no WebFetch prompt, no Task) */
    const cl_ext *ext;          /* agents, skills, commands (WP3's loader; 0: built-ins only) */
    const char *model;          /* the conversation's model (Task's default) */
    int web_search;             /* declare the web_search server tool (1 by default) */
    unsigned long allowed;      /* bit per tool declared and allowed (a subagent's subset) */
    int depth;                  /* 0 the conversation, 1 inside a subagent (no Task there) */
    char *json;                 /* the declared tools, built by tools_json */
    int json_haiku;             /* ... for a Haiku model (the older web_search) */
    void *u;
    /* the call, shown before anything happens; what is a one-line summary */
    void (*show)(void *u, const char *tool, const char *what);
    /* the permission question: ASK_NO / ASK_ONCE / ASK_SESSION / ASK_STOP */
    int (*ask)(void *u, const char *tool, const char *what, int outside);
    /* optional: a write or an edit before the question, the file's text
     * before (0, 0 when it is new) and after, both UTF-8 for an edit */
    void (*preview)(void *u, int tool, const char *path, const char *before, long bn, const char *after,
                    long an);
    /* optional: the call's result as Claude gets it, with its input */
    void (*result)(void *u, int tool, const char *input, long inn, int is_error, const char *text, long n);
    /* A question with options (AskUserQuestion, the plan-mode tools):
     * the option picked, n when the user typed an answer of their own (in
     * other), -1 declined (Esc). CH_MULTI: *picked gets a bit per option
     * and the result is 0. Absent: the tools that need it answer is_error. */
    int (*choose)(void *u, const char *header, const char *question, const char *const *labels,
                  const char *const *descs, int n, int flags, unsigned *picked, char *other, long cap);
    /* optional: a plan (ExitPlanMode), Markdown, shown whole */
    void (*plan)(void *u, const char *text, long n);
    /* optional: is full (canonical) inside a directory added to the working
     * ones (--add-dir, /add-dir, permissions.additionalDirectories)? Such a
     * path is not "outside". */
    int (*added)(void *u, const char *full);
    /* optional: one call with the policy around it (permission rules,
     * hooks, checkpoints -- the REPL's): a subagent's calls go through it
     * as the conversation's do; text for Claude after the round's results
     * goes to extra. Absent: tools_run alone. */
    void (*call)(void *u, struct cl_tools *t, const char *id, const char *name, int input_ok, const char *raw,
                 long rawn, jw *out, jw *extra);
    /* optional: a subagent is done (the SubagentStop hook): 1 when it is to
     * go on, with what to tell it in reason */
    int (*agent_stop)(void *u, const char *agent, int active, jw *reason);
    /* optional: a subagent's message as it is added to its conversation
     * (stream-json's subagent messages): parent the Task call's id, user 1
     * for its prompt and its tool results, st the answer's stream (0 for
     * a user message) */
    void (*agent_msg)(void *u, const char *parent, int user, const char *json, long n, struct cl_stream *st);
    char *sub_append;           /* --append-subagent-system-prompt: added to every subagent's prompt (owned) */
    const char *memory;         /* the memory files' text (CLAUDE.md ...): subagents get it too, 0 none */
    int ask_policy;             /* a subagent's permissionMode dontAsk / bypassPermissions: the
                                 * REPL's ASKP_* + 1; 0 the session's */
    const char *parent_id;      /* inside a subagent: the Task call's id (stream-json), 0 outside */
    int nobody;                 /* print mode: 1 a denial was nobody's answer; 2 --permission-prompts none */
    /* the call's one-line summary for the screen when its result's text is
     * for Claude only (WebFetch: "Received 12.3KB (200 OK)", Claude Code's
     * line); "" none. Set by the tool, cleared at each call. */
    char brief[96];
    int stop;                   /* ASK_STOP was answered this round (reset by the caller) */
    int cur;                    /* the tool being run (the result hook's) */
    const char *cur_in;
    long cur_inn;
} cl_tools;

/* the shared state (read set, shells) made; 0, -1 out of memory */
int tools_init(cl_tools *t);
/* background shells killed, everything freed */
void tools_free(cl_tools *t);

/* The "tools" array of a request body for that model (web_search's
 * version depends on it); built once, kept in t->json. */
const char *tools_json(cl_tools *t, const char *model);
/* The background shells (Bash run_in_background), one line each --
 * "bash_1  running  Wait 2" -- into out ("" none): /tasks's list. */
void tools_shells(cl_tools *t, char *out, long cap);
/* T_*, or -1 */
int tools_id(const char *name);
/* the tool's API name */
const char *tools_name(int tool);
/* Is input a valid call of that tool? 0, or -1 with the reason in err. */
int tools_validate(int tool, jv input, char *err, long cap);
/* One tool_use block: validated, shown, confirmed, run; its tool_result
 * block appended to out. input_ok 0: the streamed input was not valid
 * JSON (raw is what came). */
void tools_run(cl_tools *t, const char *id, const char *name, int input_ok,
               const char *raw, long rawn, jw *out);

/* A tools list ("Read, Grep", "Bash(make:*) Edit"; "" or "*" all) as a
 * bit set of T_*, bit T_COUNT for WebSearch; "Agent" names Task. */
unsigned long tools_mask(const char *list);
/* An agent by name, case-insensitive: the built-in ones (general-purpose,
 * Explore, Plan) and the provider's (ext.h). 0 none. */
const struct cl_agent *tools_agent(const cl_tools *t, const char *name);

/* ---- what the screen shows of a call (show.c, ui.c) ---- */

/* the name Claude Code shows ("Read", "Update", "Search", "Bash", ...) */
const char *tools_title(int tool);
/* the call's arguments for its header, plain text, malloc'ed ("" when
 * none): `src/a.c`, `pattern: "x", path: "S:"`, the command, ... */
char *tools_args(int tool, const char *in, long inn);
/* A result's one-line summary into out ("Read 12 lines", "Found 3
 * files"): 1, or 0 when the result's own text is shown instead. */
int tools_summary(int tool, const char *in, long inn, const char *text, long n, char *out, long cap);
/* A server tool block (server_tool_use / web_search_tool_result, its
 * JSON): its header ("Web Search(\"amiga\")") or summary ("Did 1 search",
 * "5 results") into out; 0, -1 when it is neither. */
int tools_server_line(const char *block, long n, char *out, long cap);

#endif
