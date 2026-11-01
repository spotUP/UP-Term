/* histline -- one line of the prompt history file, {"display":"...","project":"..."}, as Claude Code keeps it
 * (hist.h). The one encoder and decoder: hist.c (the raw-mode editor's list) and the console handler (the line
 * mode's list, handler/complete.c) both go through it. No allocation here; portable C89, host-tested
 * (tests/test_histfile.c, tests/test_claude_tui.c).
 *
 * A history file named for the console (VTCON_HIST_FILE) is "path" (plain lines, the Shell's) or
 * "path\nproject" (JSON lines of that project; the project may be empty). */
#ifndef CL_HISTLINE_H
#define CL_HISTLINE_H

#include "json.h"

/* line[0..n) is a history line: 1 with display and project (project empty when it has none), 0 not */
int hl_parse(const char *line, long n, jv *display, jv *project);
/* the line for display and project, newline included, onto w */
void hl_put(jw *w, const char *display, const char *project);

/* the project of a console file spec ("path\nproject"): 0 for a plain "path" */
const char *hl_spec_project(const char *spec);
/* the path of a spec into out (cap bytes): its length, -1 when it does not fit */
long hl_spec_path(const char *spec, char *out, long cap);
/* data[0..n) JSON lines to plain lines in place (display, newline), only those of project; the new length */
long hl_to_plain(char *data, long n, const char *project);
/* the longest line hl_put can make for a display of dn bytes and a project of pn bytes */
#define HL_LINE_MAX(dn, pn) (64L + 6L * (long)(dn) + 6L * (long)(pn))

#endif
