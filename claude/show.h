/* show -- the transcript as Claude Code writes it (ledger A3): the
 * user's turns dimmed after "> ", the answer behind a bullet with its
 * Markdown drawn block by block while it streams (view/md.c's stream,
 * code in colour through view/hl_lex), tool calls as "<bullet> Name(args)"
 * with the result under a corner -- long results folded to a few lines
 * (Ctrl+O's transcript viewer shows them whole), edits and writes as a numbered diff in red
 * and green, the todo list as a checklist -- and the program's own notes.
 * Everything goes to the screen through tui_lines, a batch at a time.
 * Portable C89, host-tested on the engine (tests/test_claude_tui.c). */
#ifndef CL_SHOW_H
#define CL_SHOW_H

#include "tui.h"
#include "../view/md.h"

#define SHOW_FOLD 3             /* result lines shown before "... +N lines" */

typedef struct cl_show {
    cl_tui *t;
    /* the answer's Markdown */
    md *md;
    vw_out vo;
    hl_theme th;
    int first;                  /* the next answer line is the first (it gets the bullet) */
    jw line;                    /* the line md is writing */
    jw batch;                   /* lines ready for the screen */
    /* the tool call in progress */
    int tool;
    const char *brief;          /* the tool's own summary of the next result (cl_tools.brief), 0 none */
    jw head;                    /* its header line, not yet shown */
    int head_out;
    int adds, dels;             /* its preview's counts */
    char path[256];
    /* the thinking block streaming in (shown when it is over) */
    jw think;
    unsigned long think_t0;
    /* how often each part ran (the reachability test's sentinel) */
    long n_text, n_blocks, n_tools, n_diffs, n_think;
    const int *verbose;         /* --verbose / "verbose": results and diffs unfolded (0: folded) */
    const char *name_sgr;       /* A4 gaps 2: the tool name's colour in the header (a subagent's), 0 none */
} cl_show;

void show_init(cl_show *s, cl_tui *t);
void show_free(cl_show *s);
/* the answer's renderer (cl_render) */
void show_render(cl_show *s, cl_render *r);
void show_welcome(cl_show *s, const char *model, const char *root);
void show_user(cl_show *s, const char *text);
/* a note of the program's (an error, a command's answer) */
void show_note(cl_show *s, const char *text);
/* a tool call: its header waits for the result, or for a question */
void show_tool(cl_show *s, int tool, const char *input, long inn, const char *what);
void show_head(cl_show *s);
void show_preview(cl_show *s, int tool, const char *path, const char *before, long bn, const char *after,
                  long an);
void show_result(cl_show *s, int tool, const char *input, long inn, int is_error, const char *text, long n);
/* thinking text as it streams (A4 1.9) */
void show_think(cl_show *s, const char *p, long n);
/* a server tool (web_search): its call's header, or its result's summary */
void show_server(cl_show *s, int call, const char *line);
/* the display name of a tool ("Read", "Update", ...) */
const char *show_name(int tool);

#endif
