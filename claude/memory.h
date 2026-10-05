/* memory -- CLAUDE.md, Claude Code's memory files, for C:Claude (A4 WP3,
 * row 3.1).
 *
 * Read at the start, in Claude Code's order: the user's
 * (ENVARC:Claude/CLAUDE.md), then the root's ancestors from the volume
 * down, then the root itself -- CLAUDE.md, AMIGA.md (what /init writes on
 * the Amiga), AGENTS.md, .claude/CLAUDE.md -- and CLAUDE.local.md (the
 * private one). A file may import others with @path (relative to it,
 * ~/ the user's directory, depth 5, each file once); imports outside code
 * blocks and spans only, and only names of files that exist.
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
#define MEM_FILE_MAX (64L * 1024)
#define MEM_DEPTH 5

enum { MEM_USER, MEM_PROJECT, MEM_LOCAL, MEM_IMPORT, MEM_NESTED };

typedef struct cl_memfile {
    char path[300];
    int kind;                   /* MEM_* */
    long size;
} cl_memfile;

typedef struct cl_memory {
    cl_memfile f[MEM_FILES];
    int n;
    jw text;                    /* everything read at the start, for the system prompt */
    const char *home;           /* for ~/ in imports */
} cl_memory;

void mem_init(cl_memory *m);
void mem_free(cl_memory *m);
/* The files read at the start into m->text: their count. */
int mem_load(cl_memory *m, cl_sys *sys, const char *home, const char *root);
/* A file below the root was read (path resolved): the memory of the
 * directories between root and it not yet seen, appended to out as text
 * for the conversation. Its count of files. */
int mem_nested(cl_memory *m, cl_sys *sys, const char *root, const char *path, jw *out);
/* the memory files to edit (/memory): user, project, local, by kind */
void mem_file_of(int kind, const char *home, const char *root, char *out, long cap);

#endif
