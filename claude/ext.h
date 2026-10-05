/* ext -- what the user and the project add to C:Claude, as Claude Code
 * reads it from ~/.claude and <project>/.claude: subagents (.claude/agents/
 * NAME.md), skills (.claude/skills/NAME/SKILL.md), custom slash commands
 * (.claude/commands/NAME.md). The loader is WP3's (config.c); the tools
 * (Task, Skill, SlashCommand -- ledger A4 WP2) see only this interface.
 * Every pointer may be 0: no such things. The strings stay valid while
 * the program runs (or until the loader reloads them between turns). */
#ifndef CL_EXT_H
#define CL_EXT_H

#include "json.h"

typedef struct cl_agent {
    const char *name;           /* subagent_type, "code-reviewer" */
    const char *description;    /* when to use it (the Task tool lists it) */
    const char *tools;          /* "Read, Grep, Glob" -- 0 or "" for all but Task */
    const char *model;          /* "sonnet", "opus", "haiku", an id, "inherit", or 0 */
    const char *prompt;         /* its system prompt (the file's body) */
    /* A4 gaps: Claude Code's other agent keys (0 / "" none) */
    const char *deny_tools;     /* disallowedTools: taken from the tools it has */
    int max_turns;              /* maxTurns: its rounds, 0 the default */
    const char *effort;         /* effort for its requests */
    const char *skills;         /* skills whose text it starts with */
    const char *perm_mode;      /* permissionMode: default, acceptEdits, plan, dontAsk, bypassPermissions */
    const char *initial;        /* initialPrompt: sent first when it runs the session (--agent) */
} cl_agent;

typedef struct cl_skill {
    const char *name;
    const char *description;
    const char *path;           /* its SKILL.md (AmigaOS path) */
    const char *when;           /* when_to_use, 0 none (listed after the description) */
} cl_skill;

typedef struct cl_command {
    const char *name;           /* without the slash: "review" */
    const char *description;    /* only commands with one are offered to Claude */
} cl_command;

typedef struct cl_ext {
    void *u;
    /* the count, *list the array */
    int (*agents)(void *u, const cl_agent **list);
    int (*skills)(void *u, const cl_skill **list);
    int (*commands)(void *u, const cl_command **list);
    /* A command's prompt with its arguments put in ($ARGUMENTS, $1, the
     * !`cmd` and @file parts done): 0 with the text appended to out, -1
     * with the reason in err (no such command, ...). */
    int (*expand)(void *u, const char *name, const char *args, jw *out, char *err, long cap);
    /* A4 gaps: a skill's text with its arguments put in, as a command's
     * ($ARGUMENTS, $0.., !`cmd`, ${CLAUDE_SKILL_DIR} ...), its
     * allowed-tools and model in force for the rest of the turn (activate
     * 1; 0: only its text, an agent's preloaded skill): 0 with the text
     * appended to out, -1 with err. *fork 1 when it runs in a subagent
     * (context: fork), agent then its type ("" general-purpose). */
    int (*skill)(void *u, const char *name, const char *args, int activate, jw *out, int *fork, char *agent,
                 long acap, char *err, long cap);
} cl_ext;

#endif
