/* memory -- CLAUDE.md, Claude Code's memory files, for C:Claude (A4 WP3,
 * row 3.1).
 *
 * Read at the start, in Claude Code's order: the user's
 * (ENVARC:Claude/CLAUDE.md), then the root's ancestors from the volume
 * down, then the root itself -- CLAUDE.md, AMIGA.md (what /init writes on
 * the Amiga), AGENTS.md, .claude/CLAUDE.md -- and CLAUDE.local.md (the
 * private one). A file may import others with @path (relative to it,
 * ~/ the user's directory, four hops deep, each file once; "\ " is a space
 * in a name, a quoted name is not imported); imports outside code
 * blocks and spans only, and only names of files that exist.
 *
 * A4 gaps: block-level HTML comments (<!-- -->, outside code blocks) are
 * left out; claudeMdExcludes' globs leave files out; the rules of
 * .claude/rules/ and ENVARC:Claude/rules/ (*.md, subdirectories too) come
 * after the CLAUDE.md files -- those with "paths:" in their frontmatter
 * only once Claude reads a file they match (mem_rules); and auto memory:
 * the first 200 lines of <home>/projects/<project>/memory/MEMORY.md.
 *
 * The text goes into the system prompt (it is stable for the session, so
 * it stays inside the cached prefix). Memory in directories below the
 * root comes later, when Claude reads a file there (mem_nested): its text
 * is added to the conversation after that tool result.
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c). */
#ifndef CL_MEMORY_H
#define CL_MEMORY_H

#include "json.h"
#include "sys.h"

#define MEM_FILES 32
#define MEM_RULES 32
#define MEM_AUTO_LINES 200
#define MEM_FILE_MAX (64L * 1024)
#define MEM_DEPTH 4                 /* Claude Code: imports four hops deep */

enum { MEM_USER, MEM_PROJECT, MEM_LOCAL, MEM_IMPORT, MEM_NESTED, MEM_RULE, MEM_AUTO };

/* a rule with "paths:": loaded when a file it matches is read */
typedef struct cl_memrule {
    char path[300];
    char globs[200];            /* the globs, comma-separated: "src/x.c, *.h" */
    int done;
} cl_memrule;

typedef struct cl_memsrc {
    char path[300];
    int kind;                   /* MEM_* */
    long size;
} cl_memsrc;

typedef struct cl_memory {
    cl_memsrc f[MEM_FILES];
    int n;
    jw text;                    /* everything read at the start, for the system prompt */
    const char *home;           /* for ~/ in imports */
    char *const *excl;          /* claudeMdExcludes: globs of paths not read (set before mem_load) */
    int nexcl;
    cl_memrule *rules;          /* the path-scoped rules not yet loaded */
    int nrules;
    char auto_dir[300];         /* auto memory's directory ("" off) */
    /* A4 gaps 2: external imports -- an @import in a project's memory file
     * that resolves outside the start directory loads only once approved
     * (Claude Code's dialog); the user's own files import freely */
    int ext_ok;                 /* 1 approved; 0 not asked, 2 declined: held back (set before mem_load) */
    char ext_list[600];         /* the external imports held back, a line each */
    int next;                   /* their count */
    int in_user;                /* loading a user-scope file (its imports are not external) */
    char root[300];
} cl_memory;

void mem_init(cl_memory *m);
void mem_free(cl_memory *m);
/* The files read at the start into m->text: their count. */
int mem_load(cl_memory *m, cl_sys *sys, const char *home, const char *root);
/* A file below the root was read (path resolved): the memory of the
 * directories between root and it not yet seen, appended to out as text
 * for the conversation. Its count of files. */
int mem_nested(cl_memory *m, cl_sys *sys, const char *root, const char *path, jw *out);
/* A file was read (path resolved): the path-scoped rules it matches, not
 * loaded yet, appended to out. Their count. */
int mem_rules(cl_memory *m, cl_sys *sys, const char *root, const char *path, jw *out);
/* Auto memory: dir's MEMORY.md (its first 200 lines) into m->text, the
 * directory kept in m->auto_dir. 1 when the file was there. */
int mem_auto(cl_memory *m, cl_sys *sys, const char *dir);
/* the memory files to edit (/memory): user, project, local, by kind */
void mem_file_of(int kind, const char *home, const char *root, char *out, long cap);

#endif
