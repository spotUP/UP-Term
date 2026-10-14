/* launch -- see launch.h */
#include <stdlib.h>
#include <string.h>
#include "launch.h"
#include "tui.h"

static const char *const opts[2] = {
    "Claude on the Amiga (API key)",
    "Claude Code on the NAS (subscription)"
};

int launch_last_parse(const char *text)
{
    return text && !strncmp(text, "remote", 6) ? LAUNCH_REMOTE : LAUNCH_LOCAL;
}

const char *launch_last_text(int mode)
{
    return mode == LAUNCH_REMOTE ? "remote\n" : "local\n";
}

static void put(cl_io *io, const char *s)
{
    io->write(io->u, s, (long)strlen(s));
}

static int ask_line(cl_io *io, int last)
{
    char line[64];
    int tries;
    for (tries = 0; tries < 3; tries++) {
        long n;
        put(io, "Where should Claude run?\n  1. ");
        put(io, opts[0]);
        put(io, "\n  2. ");
        put(io, opts[1]);
        put(io, last == LAUNCH_REMOTE ? "\nNumber [2]: " : "\nNumber [1]: ");
        n = io->read_line(io->u, line, sizeof(line));
        if (n < 0)
            return -1;
        if (n == 0 || line[0] == 0)
            return last;
        if (line[0] == '1' && !line[1])
            return LAUNCH_LOCAL;
        if (line[0] == '2' && !line[1])
            return LAUNCH_REMOTE;
        put(io, "Type 1 or 2.\n");
    }
    return -1;
}

static int ask_screen(cl_io *io, int last)
{
    cl_tui *t = (cl_tui *)malloc(sizeof(cl_tui));
    int pick;
    if (!t)
        return -2;
    if (tui_init(t, io)) {
        free(t);
        return -2;
    }
    if (tui_start(t)) {
        tui_free(t);
        free(t);
        return -2;                  /* the console refused raw mode: the line question */
    }
    pick = tui_ext_menu_sel(t, "Where should Claude run?", opts, 2, last == LAUNCH_REMOTE ? 1 : 0, 0);
    tui_stop(t);
    tui_free(t);
    free(t);
    return pick == 0 ? LAUNCH_LOCAL : pick == 1 ? LAUNCH_REMOTE : -1;
}

int launch_ask(cl_io *io, int last)
{
    if (io->read) {
        int r = ask_screen(io, last);
        if (r != -2)
            return r;
    }
    return ask_line(io, last);
}
