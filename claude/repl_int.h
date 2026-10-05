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
/* A side request (/btw, /recap, /rename, /rewind's summaries): messages
 * from..to-1 of the conversation and the question ask, the tools listed
 * but none called; the answer's text into answer, nothing kept in the
 * conversation (the cost is counted). 0, -1 failed (shown), -2 stopped. */
int repl_side(cl_repl *r, int from, int to, const char *ask, jw *answer);
/* /rewind's "Summarize from here" (up_to 0: the prompt at msg and all
 * after it become one summary) and "Summarize up to here" (up_to 1:
 * everything before it does): 0, -1, -2 stopped */
int repl_summarize(cl_repl *r, int msg, int up_to);
/* the custom definitions read again (/reload-skills, part of repl_load) */
int repl_load_defs(cl_repl *r);
/* the effort a request sends ("" for /effort auto: the model's own) */
const char *repl_effort(const cl_repl *r);
/* slash.c: the i-th of Claude Code's commands that are not on the Amiga, 0 past the end */
const char *slash_na_list(int i);
/* the context size at which the conversation compacts by itself, tokens */
long repl_compact_at(cl_repl *r);
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
/* One tool call of tl (the conversation's tools or a subagent's) with the
 * policy around it: the rules and the PreToolUse hooks first (a deny or a
 * block answers it here), then tools_run, then the PostToolUse hooks and
 * the nested memory (text for Claude after the round's results into
 * extra). Its tool_result block is appended to out. cl_tools.call. */
void pol_call(void *u, cl_tools *tl, const char *id, const char *name, int input_ok, const char *raw, long rawn,
              jw *out, jw *extra);
/* The REPL's hooks into the tools (the policy for subagents' calls, the
 * added directories, SubagentStop, the extensions): once, at repl_init. */
void pol_attach_tools(cl_repl *r);
/* After the settings or the definitions changed: the extensions' lists
 * built again from r->defs, the WebSearch switch from the settings, the
 * tools JSON dropped so the next request declares it anew. 0, -1 out of
 * memory. */
int pol_tools(cl_repl *r);
void pol_ext_free(cl_repl *r);
/* a command's or a skill's text with the ${CLAUDE_*} values of this
 * session (cmd_expand_vars): 0, -1 with err */
int pol_expand(cl_repl *r, const cl_def *d, const char *args, jw *out, char *err, long cap);
/* d's allowed-tools in force for the rest of the turn (r->turn_tools) */
void pol_turn_tools(cl_repl *r, const cl_def *d);
void pol_session(cl_repl *r, int event, const char *source);
/* UserPromptSubmit: -1 blocked (shown), 0 go on (context into r->pending) */
int pol_prompt(cl_repl *r, const char *prompt, long n);
/* Stop: 1 when a hook asks Claude to go on (the reason in reason) */
int pol_stop(cl_repl *r, int active, jw *reason);
void pol_notify(cl_repl *r, const char *message);
/* PreCompact: -1 a hook blocked the compaction (shown), 0 go on */
int pol_precompact(cl_repl *r, int automatic, const char *focus);
/* A4 gaps: Claude Code's other hook events */
void pol_postcompact(cl_repl *r, int automatic, const char *summary, long n);
void pol_stop_failure(cl_repl *r, const char *error);
void pol_cwd_changed(cl_repl *r, const char *old_cwd, const char *new_cwd);
void pol_dir_added(cl_repl *r, const char *dir);
/* UserPromptExpansion before a typed /command or /skill runs: -1 blocked */
int pol_expansion(cl_repl *r, const cl_def *d, const char *args, const char *prompt, long n);
/* PermissionRequest before a question: RULE_ALLOW / RULE_DENY from a
 * hook's decision.behavior, RULE_NONE ask as usual */
int pol_permission_request(cl_repl *r, const char *tool, const char *input, long n);
/* the screen's settings, memory files and rewind points (ui.h, WP1) */
void pol_attach_ui(cl_repl *r);
/* the statusLine command run now, its output into r->status_text (blank
 * when it fails or says nothing, as Claude Code's), the footer redrawn
 * where it changed */
void pol_statusline(cl_repl *r);
/* One of Claude Code's status line events (the session started, an
 * assistant message, /compact done): the command runs, or -- within
 * 300 ms of its last run -- at the next tick. */
void pol_status_event(cl_repl *r);
/* The idle tick (the screen waiting for keys): a held-back event, a
 * permission mode or vim change, statusLine.refreshInterval. */
void pol_status_tick(void *u);

/* slash.c: the command table and the A4 commands */
extern const struct cl_cmd slash_builtin[];
extern const int slash_nbuiltin;
/* 1 when word (e.g. "/status") is one of slash.c's and was run */
int slash_run(cl_repl *r, const char *word, const char *arg);
/* a custom command (/name args): 1 when there is one and it ran */
int slash_custom(cl_repl *r, const char *word, const char *arg);

#endif
