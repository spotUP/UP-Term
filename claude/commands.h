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
} cl_def;

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

/* A command's prompt: 0, or -1 with err. root resolves @file. */
int cmd_expand(const cl_def *d, const char *args, cl_sys *sys, const char *root, jw *out, char *err, long cap);
/* Does a definition's tools list (allowed-tools) name this tool (Claude
 * Code's name, "Bash" also for "Bash(git:*)")? */
int def_has_tool(const cl_def *d, const char *tool);

#endif
