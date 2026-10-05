/* hist -- the prompt history kept across sessions (A4 1.1), as Claude
 * Code keeps it: every submitted prompt with the project (start
 * directory) it was typed in, one JSON object a line in
 * ENVARC:Claude/history ({"display":"...","project":"..."}), newest last,
 * at most HIST_MAX lines. Up/Down recall the project's own prompts (the
 * editor's list is filled from here); the Ctrl+R search runs over every
 * project's, newest first, a prompt seen twice found once. The file is
 * rewritten whole when a prompt is added (cl_sys has no append), never
 * per key. Portable C89 over sys.h, host-tested (tests/test_claude_tui.c). */
#ifndef CL_HIST_H
#define CL_HIST_H

#include "sys.h"
#include "edit.h"

#define HIST_MAX 500

typedef struct cl_hist {
    char **d;           /* the prompts */
    char **p;           /* their projects */
    int n, cap;
} cl_hist;

void hist_init(cl_hist *h);
void hist_free(cl_hist *h);
/* the file read (a missing or unreadable one is an empty history): the
 * number of entries */
int hist_load(cl_hist *h, cl_sys *sys, const char *path);
/* the editor's Up/Down list: the project's last ED_HIST prompts */
void hist_fill(const cl_hist *h, cl_edit *e, const char *project);
/* a submitted prompt: kept (an older equal one of the project dropped),
 * the file written when path is set; 0, -1 when the write failed */
int hist_add(cl_hist *h, cl_sys *sys, const char *path, const char *display, const char *project);
/* The newest entry before index `before` (h->n: from the newest) whose
 * prompt contains q and that is not repeated by a newer one; -1 none. */
int hist_find(const cl_hist *h, const char *q, int before);

#endif
