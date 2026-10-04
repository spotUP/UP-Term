/* tools -- the Claude client's tools, as Claude Code has them: read_file,
 * list_dir, grep, write_file, edit_file (an exact, unique string
 * replaced), run_command (through vsh). Each is a client tool with a
 * strict JSON schema; every input is validated before anything runs; each
 * call is shown and, by the permission rules, confirmed by the user; each
 * ends in one tool_result block (is_error on failure).
 *
 * Permissions: the read-only tools (read_file, list_dir, grep) may be
 * allowed for the session with one answer, which covers all three; a
 * write, an edit or a command asks every time unless the user allowed
 * that tool for the session. A path outside the start directory asks
 * always. No tool runs before the user has answered.
 * Portable C89 over sys.h, host-tested (tests/test_claude_tools.c). */
#ifndef CL_TOOLS_H
#define CL_TOOLS_H

#include "json.h"
#include "sys.h"

enum { T_READ_FILE, T_LIST_DIR, T_GREP, T_WRITE_FILE, T_EDIT_FILE, T_RUN_COMMAND, T_COUNT };

/* the user's answer to a permission question */
enum { ASK_NO, ASK_ONCE, ASK_SESSION };

typedef struct cl_perm {
    unsigned session;           /* bit per tool: allowed for the session */
} cl_perm;

int perm_read_only(int tool);
/* must the user be asked? */
int perm_must_ask(const cl_perm *p, int tool, int outside);
/* the user chose "always this session" */
void perm_grant(cl_perm *p, int tool);

typedef struct cl_tools {
    cl_sys *sys;
    char root[256];             /* the start directory, canonical */
    cl_perm perm;
    int timeout_s;              /* run_command */
    void *u;
    /* the call, shown before anything happens; what is a one-line summary */
    void (*show)(void *u, const char *tool, const char *what);
    /* the permission question: ASK_NO / ASK_ONCE / ASK_SESSION */
    int (*ask)(void *u, const char *tool, const char *what, int outside);
} cl_tools;

/* the "tools" array of the request body */
const char *tools_json(void);
/* T_*, or -1 */
int tools_id(const char *name);
/* Is input a valid call of that tool? 0, or -1 with the reason in err. */
int tools_validate(int tool, jv input, char *err, long cap);
/* One tool_use block: validated, shown, confirmed, run; its tool_result
 * block appended to out. input_ok 0: the streamed input was not valid
 * JSON (raw is what came). */
void tools_run(cl_tools *t, const char *id, const char *name, int input_ok,
               const char *raw, long rawn, jw *out);

#endif
