/* tools_int -- what the tool modules share (tools.c, search.c, shells.c,
 * webfetch.c, subagent.c); not for the rest of the program. */
#ifndef CL_TOOLS_INT_H
#define CL_TOOLS_INT_H

#include "tools.h"

#define TL_OUT_MAX (30L * 1024)     /* a tool's text result, as Claude Code's 30000 characters */

/* the tool_result block (and the result hook) */
void tl_result(cl_tools *t, jw *out, const char *id, const char *text, long n, int is_error);
/* an is_error result: a then b */
void tl_error(cl_tools *t, jw *out, const char *id, const char *a, const char *b);
/* a string property, malloc'ed ("" when absent); 0 out of memory */
char *tl_prop(jv in, const char *key, long *len);
long tl_num(jv in, const char *key, long def);
int tl_bool(jv in, const char *key);
/* The full path for a path argument, and whether it lies outside the
 * start directory: 0, or -1 when the name is no path. */
int tl_resolve(cl_tools *t, const char *arg, char *full, long cap, int *outside);
int tl_is_binary(const char *s, long n);
/* The call shown, the mode and the user asked as the rules say: 0 go on,
 * -1 refused (its result already written). show 0: already shown. */
int tl_gate(cl_tools *t, jw *out, const char *id, int tool, const char *what, int outside, int show);
/* a one-line summary for the screen, controls as '?' */
void tl_summary(char *out, long cap, const char *a, const char *b);

/* search.c */
void search_glob(cl_tools *t, jw *out, const char *id, jv in);
void search_grep(cl_tools *t, jw *out, const char *id, jv in);
/* the newest-first order of a list of paths by their times */
void search_sort_newest(char **paths, long *times, long n);

/* shells.c: the background tasks -- Bash run_in_background, a command
 * moved there at its time limit, Monitor; TaskStop (KillShell), BashOutput */
struct cl_shells *shells_new(void);
void shells_free(cl_sys *sys, struct cl_shells *sh);
/* limit_ms: its background time limit (0 none) */
void shells_start(cl_tools *t, jw *out, const char *id, const char *cmd, const char *desc, long limit_ms);
void shells_output(cl_tools *t, jw *out, const char *id, jv in);
/* TaskStop and KillShell (t->cur tells which) */
void shells_kill(cl_tools *t, jw *out, const char *id, jv in);
void monitor_start(cl_tools *t, jw *out, const char *id, jv in);
/* can a foreground command run as a polled job (sys.h has what it takes)? */
int shells_can_run(cl_tools *t);
/* A foreground command run as a job: shown is the command as Claude wrote
 * it, cmd what runs (a "cd" in front); the result's text (the return code,
 * the output within Claude Code's limits, or the news that it moved to the
 * background) appended to res. 0 ended, SHELL_MOVED moved to the
 * background (at its time limit, or the screen's Ctrl+B through t->wait),
 * SYS_TIMEOUT / SYS_BREAK stopped, -1 did not start. */
int shells_run_fg(cl_tools *t, const char *cmd, const char *shown, const char *desc, int secs, jw *res, long *rc,
                  int *is_err);
/* is path a task's output file (Read needs no question for it)? */
int shells_owns(cl_tools *t, const char *path);
/* What happened to the tasks since the last look (ended, a monitor's new
 * lines, a deadline) as notices for Claude appended to w: their count. */
int shells_poll(cl_tools *t, jw *w);
/* tasks still running (or monitors not yet reported ended) */
int shells_busy(cl_tools *t);
/* a foreground subagent's run ended: the tasks it started stop with it */
void shells_end_owner(cl_tools *t, int owner);
/* Stop's background_tasks: the tasks in flight, a JSON array */
void shells_json(cl_tools *t, jw *w);

/* tasks.c: the task list (TaskCreate / TaskGet / TaskList / TaskUpdate)
 * and the session's cron jobs (CronCreate / CronDelete / CronList) */
struct cl_tasks *tasks_new(void);
void tasks_free(struct cl_tasks *k);
void tasks_run(cl_tools *t, int tool, jw *out, const char *id, jv in);
/* the list as TodoWrite's input ({"todos":[...]}), for the screen's task
 * list and /todos: malloc'ed, 0 when there are no tasks */
char *tasks_todos(const struct cl_tasks *k);

/* webfetch.c */
void webfetch_run(cl_tools *t, jw *out, const char *id, jv in);
/* WebSearch: the server tool in a request of its own */
void websearch_run(cl_tools *t, jw *out, const char *id, jv in);
/* is the host one of Claude Code's preapproved documentation hosts? */
int webfetch_preapproved(const char *host);
/* A POST of a JSON body over the transport (an http hook): headers are
 * whole lines ("Name: value\r\n"); 0 with the status and the body, -1 with
 * err (no connection, no answer within timeout_s) */
int web_post(cl_net *net, const char *url, const char *headers, const char *body, long bn, int timeout_s,
             int *status, jw *resp, char *err, long cap);
/* subagent.c: an agent run on a prompt, its final text into answer (an
 * agent hook): 0, -1 */
int agent_answer(cl_tools *t, const struct cl_agent *a, const char *prompt, long pn, jw *answer);
/* WebFetch's cache freed (tools_free) */
void webfetch_cache_free(cl_tools *t);

/* subagent.c: Task, and one quiet model call */
void agent_task(cl_tools *t, jw *out, const char *id, jv in);
/* a forked skill (context: fork): the prompt run by agent type ("" or 0:
 * general-purpose), its report the result of the call id */
void agent_fork(cl_tools *t, jw *out, const char *id, const char *type, const char *prompt, long pn);
/* The built-in agents and the provider's, in order: the count; *a the i-th. */
int agent_count(const cl_tools *t);
const struct cl_agent *agent_get(const cl_tools *t, int i);
/* one request without tools: the answer's text into answer; 0, -1 (err), -2 stopped */
int agent_query(cl_tools *t, const char *model, const char *system, const char *prompt, long pn, jw *answer,
                char *err, long cap);
/* "sonnet", "opus", "haiku", "inherit", an id: the model id (0 for inherit) */
const char *agent_model_id(const char *alias);

#endif
