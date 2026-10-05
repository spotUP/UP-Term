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

/* shells.c: Bash run_in_background, BashOutput, KillShell */
struct cl_shells *shells_new(void);
void shells_free(cl_sys *sys, struct cl_shells *sh);
void shells_start(cl_tools *t, jw *out, const char *id, const char *cmd);
void shells_output(cl_tools *t, jw *out, const char *id, jv in);
void shells_kill(cl_tools *t, jw *out, const char *id, jv in);

/* webfetch.c */
void webfetch_run(cl_tools *t, jw *out, const char *id, jv in);

/* subagent.c: Task, and one quiet model call */
void agent_task(cl_tools *t, jw *out, const char *id, jv in);
/* The built-in agents and the provider's, in order: the count; *a the i-th. */
int agent_count(const cl_tools *t);
const struct cl_agent *agent_get(const cl_tools *t, int i);
/* one request without tools: the answer's text into answer; 0, -1 (err), -2 stopped */
int agent_query(cl_tools *t, const char *model, const char *system, const char *prompt, long pn, jw *answer,
                char *err, long cap);
/* "sonnet", "opus", "haiku", "inherit", an id: the model id (0 for inherit) */
const char *agent_model_id(const char *alias);

#endif
