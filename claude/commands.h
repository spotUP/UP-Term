/* commands -- Claude Code's definition files for C:Claude (A4 WP3, rows 3.4
 * 3.5, and the lists WP2's Task and Skill tools take): custom slash
 * commands, subagents, skills and output styles, each a Markdown file with
 * an optional YAML-like frontmatter between "---" lines.
 *
 *   commands/NAME.md            /NAME (a subdirectory's files too)
 *   agents/NAME.md              name, description, tools, model
 *   skills/NAME/SKILL.md        name, description, allowed-tools
 *   output-styles/NAME.md       name, description, keep-coding-instructions
 *
 * under the user's directory (ENVARC:Claude) and the project's
 * (<root>/.claude); a project's definition hides a user's of the same name.
 * Output styles also come built in: Default, Explanatory, Learning.
 *
 * A command expands as Claude Code's does: $ARGUMENTS (or, when the text
 * has none, the arguments appended), $1..$9, @file (the file's text
 * inlined), and !`command` (run first, its output inlined; only when the
 * command's allowed-tools has Bash).
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c). */
#ifndef CL_COMMANDS_H
#define CL_COMMANDS_H

#include "json.h"
#include "sys.h"

enum { DEF_COMMAND, DEF_AGENT, DEF_SKILL, DEF_STYLE, DEF_NTYPES };
#define DEF_BUILTIN (-1)

typedef struct cl_def {
    int type;                   /* DEF_* */
    int src;                    /* CFG_USER, CFG_PROJECT or DEF_BUILTIN */
    char name[64];
    char *description;          /* never 0 ("" none) */
    char *body;                 /* the text after the frontmatter */
    char *tools;                /* allowed-tools / tools, "" none */
    char *model;                /* "" the session's */
    char *hint;                 /* argument-hint */
    int keep_coding;            /* an output style keeps the coding instructions */
    int no_model;               /* disable-model-invocation */
    char path[300];
    /* A4 gaps: Claude Code's other frontmatter keys */
    char *deny_tools;           /* disallowedTools / disallowed-tools, "" none */
    char *skills;               /* an agent's skills to preload, "" none */
    char *when;                 /* a skill's when_to_use, "" none */
    int max_turns;              /* an agent's maxTurns, 0 the default */
    char effort[16];            /* effort, "" the session's */
    char perm_mode[24];         /* an agent's permissionMode, "" the session's */
    int no_user;                /* user-invocable: false (not in the / menu) */
    int fork;                   /* a skill's context: fork (runs in a subagent) */
    char agent[64];             /* ... with that agent ("" general-purpose) */
    char *arg_names;            /* arguments: the names of $name placeholders, "" none */
    char *initial;              /* an agent's initialPrompt (as the main thread), "" none */
} cl_def;

/* the ${CLAUDE_*} values of an expansion (any may be 0: "") */
typedef struct cl_cmd_vars {
    const char *session_id, *effort, *skill_dir, *project_dir;
} cl_cmd_vars;

typedef struct cl_defs {
    cl_def *d;
    int n, cap;
} cl_defs;

void defs_init(cl_defs *s);
void defs_free(cl_defs *s);
/* Everything under home (user) and root/.claude (project), and the
 * built-in output styles: the count. */
int defs_load(cl_defs *s, cl_sys *sys, const char *home, const char *root);
/* By type and name (case-insensitive); the project's first. 0 none. */
const cl_def *defs_find(const cl_defs *s, int type, const char *name);
/* the i-th of a type (0..), 0 past the end: the list WP2's Task / Skill
 * tools describe, /agents /skills /commands show */
const cl_def *defs_nth(const cl_defs *s, int type, int i);
int defs_count(const cl_defs *s, int type);

/* One file's text parsed into d (name from the frontmatter, else 0 kept):
 * 0, -1 out of memory. */
int defs_parse(const char *text, long n, cl_def *d);
void def_free(cl_def *d);

/* Every definition of a type dropped (type -1: every one that is not
 * built in): --bare, --safe-mode, --disable-slash-commands. */
void defs_drop(cl_defs *s, int type);
/* --agents: {"name": {"description", "prompt", "tools", "disallowedTools",
 * "model", "maxTurns", "effort", "skills", "permissionMode"}} added as
 * agents of the session, before the files' (they win). 0, -1 with err. */
int defs_add_agents_json(cl_defs *s, const char *json, char *err, long cap);
/* The extra keys of an agent given as JSON (--agents) into d. */
void def_extra_json(cl_def *d, jv a);

/* A command's prompt: 0, or -1 with err. root resolves @file. */
int cmd_expand(const cl_def *d, const char *args, cl_sys *sys, const char *root, jw *out, char *err, long cap);
/* ... with the ${CLAUDE_SESSION_ID} ${CLAUDE_EFFORT} ${CLAUDE_SKILL_DIR}
 * ${CLAUDE_PROJECT_DIR} values (Claude Code's skill substitutions; $N
 * and $ARGUMENTS[N] 0-based, an index with no argument left as it is,
 * \$ a dollar sign, $name from the arguments frontmatter) */
int cmd_expand_vars(const cl_def *d, const char *args, const cl_cmd_vars *v, cl_sys *sys, const char *root,
                    jw *out, char *err, long cap);
/* the ${CLAUDE_*} variables of in replaced (an allowed-tools rule): 0, -1 */
int cmd_subst_vars(const char *in, const cl_cmd_vars *v, jw *out);
/* Does a definition's tools list (allowed-tools) name this tool (Claude
 * Code's name, "Bash" also for "Bash(git:*)")? */
int def_has_tool(const cl_def *d, const char *tool);

#endif
