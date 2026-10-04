/* A window for C:Claude's screen in the host tests (ledger A3): the
 * engine as the terminal (what the program writes goes into a vt_term,
 * its answers -- the DSR report -- come back as input), a script of
 * typed chunks, a clock, a size. */
#ifndef CLAUDE_SCREEN_H
#define CLAUDE_SCREEN_H

#include "harness.h"
#include "../claude/ui.h"
#include "../claude/json.h"

typedef struct cscreen {
    vt_term *vt;
    int cols, rows;
    /* typed input, one chunk a read; a chunk starting with '!' (dropped)
     * also goes to a read that does not wait (typed while Claude works) */
    const char **script;
    int next;
    jw sent;                    /* every byte written */
    long writes;
    unsigned long clock;
    int raw_on, raw_calls;
    int brk;                    /* io->brk's answer once */
} cscreen;

extern cscreen cs;

void cs_open(int cols, int rows, const char **script);
void cs_close(void);
/* the window's functions into io (read_line 0: the screen's mode) */
void cs_io(cl_io *io);
/* row r (0-based) as UTF-8, trailing blanks cut */
const char *cs_row(int r);
/* the first row holding text, -1 none */
int cs_find(const char *text);
/* the window resized (the next size query sees it) */
void cs_resize(int cols, int rows);

#endif
