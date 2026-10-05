/* hooks -- Claude Code's hooks for C:Claude (A4 WP3, row 3.6).
 *
 * The settings' "hooks" (config.h) name AmigaDOS commands per event:
 * PreToolUse, PostToolUse (matched on the tool's name, Claude Code's or
 * our own; "Edit|Write", "*" or "" for all), UserPromptSubmit, Stop,
 * SubagentStop, SessionStart (matcher startup / resume / clear / compact),
 * SessionEnd, PreCompact (manual / auto), Notification. Each runs as
 * `<command> < <file>` through sys->run (vsh on the Amiga), the event's
 * JSON in the file -- session_id, transcript_path, cwd, hook_event_name and
 * the event's own fields, as Claude Code sends them.
 *
 * Claude Code's exit-code protocol: 0 is fine (for UserPromptSubmit and
 * SessionStart the output is added to the conversation); 2 blocks -- a
 * PreToolUse call is not run and the output goes to Claude as the reason,
 * a prompt is not sent, a Stop makes Claude go on with the reason; any
 * other code is an error shown to the user, and nothing is blocked. A
 * JSON object on the output is read as Claude Code's: "decision":"block"
 * with "reason", "continue":false, hookSpecificOutput.permissionDecision
 * (allow / deny / ask), .additionalContext, .updatedInput, systemMessage
 * (shown to the user), and PermissionRequest's decision.behavior; JSON is
 * read whatever the exit code (exit 2 still blocks). A hook's "if" (a
 * permission rule, "Bash(git *)") limits a tool event's hook to the calls
 * it matches. The commands get CLAUDE_PROJECT_DIR (also ${...}-substituted
 * in the command line, for the AmigaShell). AmigaDOS gives one output
 * stream, so stdout and stderr are the same text here.
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c). */
#ifndef CL_HOOKS_H
#define CL_HOOKS_H

#include "config.h"

typedef struct cl_hooks {
    const cl_settings *cfg;
    cl_sys *sys;
    const char *session_id;
    const char *transcript;     /* the session's JSONL file */
    const char *cwd;
    const char *tmp;            /* where the event file goes (T:) */
    long n_run;                 /* commands run (the tests' sentinel) */
    /* A4 gaps: the tool call a hook's "if" rule is matched against (set
     * around a tool event's hooks_run, 0 otherwise) */
    const char *tool, *input;
    long input_n;
    const char *project_dir;    /* CLAUDE_PROJECT_DIR for the commands (0: cwd) */
    void *u;
    /* optional: a prompt hook's question to a model (the text has the
     * event's JSON in it): 0 with the answer's text in answer, -1 */
    int (*ask_model)(void *u, const char *model, const char *prompt, jw *answer);
    /* optional: a hook's statusMessage while it runs (0 when it is done) */
    void (*status)(void *u, const char *msg);
    /* optional: each hook run, for --include-hook-events: started (rc -1)
     * and done (its exit code and output) */
    void (*seen)(void *u, int event, const char *cmd, int done, long rc, const char *out, long n);
    unsigned long once_done[16];    /* the "once" hooks run this session (their hashes) */
    int nonce;
} cl_hooks;

typedef struct cl_hookres {
    int ran;                    /* how many hooks matched and ran */
    int blocked;                /* exit 2, or decision block / permissionDecision deny */
    int decision;               /* RULE_* from permissionDecision, RULE_NONE none */
    int stop;                   /* "continue": false */
    jw reason;                  /* for Claude: why it was blocked */
    jw context;                 /* for Claude: text to add to the conversation */
    jw shown;                   /* for the user: errors, a block's reason where Claude gets none,
                                 * systemMessage */
    jw updated;                 /* PreToolUse / PermissionRequest updatedInput (a JSON object), "" none */
    int behavior;               /* PermissionRequest decision.behavior: RULE_ALLOW / RULE_DENY, RULE_NONE */
} cl_hookres;

void hookres_init(cl_hookres *r);
void hookres_free(cl_hookres *r);
/* Does a hook matcher cover this name? */
int hooks_match(const char *matcher, const char *name);
/* Is there a hook for this event and name at all? (no file is written
 * when there is none) */
int hooks_any(const cl_hooks *h, int event, const char *name);
/* The event's hooks run: extra is the event's own members, raw JSON
 * starting with a comma (",\"tool_name\":\"Bash\",...") or "". Returns
 * res->ran. */
int hooks_run(cl_hooks *h, int event, const char *name, const char *extra, cl_hookres *res);

#endif
