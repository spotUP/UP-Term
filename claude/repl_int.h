/* repl_int -- what repl.c, policy.c and slash.c share inside the REPL
 * core (A4 WP3). Not an interface for anyone else: repl.h is. */
#ifndef CL_REPL_INT_H
#define CL_REPL_INT_H

#include "repl.h"
#include "tui.h"

/* repl.c */
void repl_turn(cl_repl *r, const char *prompt, long n);
void repl_compact(cl_repl *r, const char *focus, int automatic);
void repl_ctx(cl_repl *r);
/* a note: a then b */
void repl_say(cl_repl *r, const char *a, const char *b);
/* the conversation's text shown again (after a resume) */
void repl_replay(cl_repl *r);
void repl_cost(cl_repl *r);
void repl_context(cl_repl *r);
int repl_pick(cl_repl *r, const char *title, const char *const *opt, int n, const char *cur);
/* the conversation saved (its session file) after a change */
void repl_saved(cl_repl *r);
/* the system prompt built again (an output style chosen) */
int repl_system(cl_repl *r);
/* the memory files read again, and the system prompt (/memory, /cd) */
int repl_load_memory(cl_repl *r);
/* A4 WP4: a permission question as the ask policy says (the user, no
 * one, yes to all); rule 1: an explicit ask rule asks. ASK_* */
int repl_ask(cl_repl *r, int tid, const char *tool, const char *what, int outside, int rule);
/* a call refused without a question, for print mode's permission_denials */
void repl_denied(cl_repl *r, const char *tool, const char *input, long n);
/* the A2 JSON-array conversation file loaded: 0, -1 (shown) */
int repl_load_json(cl_repl *r, const char *full);

/* policy.c: permission rules, hooks, checkpoints, nested memory around a
 * tool call, and the session's hook events */
/* Before tools_run: 1 when the call was answered here (denied by a rule,
 * blocked by a PreToolUse hook): its tool_result is in out. */
int pol_pre(cl_repl *r, const char *id, const char *name, int input_ok, const char *raw, long rawn, jw *out);
/* After it: the result block tools_run appended (blk, n); text for
 * Claude to see after the results goes to extra. */
void pol_post(cl_repl *r, const char *name, int input_ok, const char *raw, long rawn, const char *blk, long n,
              jw *extra);
void pol_session(cl_repl *r, int event, const char *source);
/* UserPromptSubmit: -1 blocked (shown), 0 go on (context into r->pending) */
int pol_prompt(cl_repl *r, const char *prompt, long n);
/* Stop: 1 when a hook asks Claude to go on (the reason in reason) */
int pol_stop(cl_repl *r, int active, jw *reason);
void pol_notify(cl_repl *r, const char *message);
void pol_precompact(cl_repl *r, int automatic, const char *focus);
/* the screen's settings, memory files and rewind points (ui.h, WP1) */
void pol_attach_ui(cl_repl *r);
/* the statusLine command run, its first line into r->status_text */
void pol_statusline(cl_repl *r);

/* slash.c: the command table and the A4 commands */
extern const struct cl_cmd slash_builtin[];
extern const int slash_nbuiltin;
/* 1 when word (e.g. "/status") is one of slash.c's and was run */
int slash_run(cl_repl *r, const char *word, const char *arg);
/* a custom command (/name args): 1 when there is one and it ran */
int slash_custom(cl_repl *r, const char *word, const char *arg);

#endif
