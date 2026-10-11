/* histfile -- which file the console's line history lives in.
 *
 * A shell that arms the console (vsh) names its own file (HISTFILE, sent with
 * ACTION_VTCON_HISTORY VTCON_HIST_FILE): that file. Otherwise the console's
 * default, by the one rule for growing files (claude/datadir.h): with the kit,
 * UP-Term:var/vtcon/history, an ENVARC:vtcon.history of an older UP-Term moved
 * there on first use (ENVARC: is copied into RAM: at every boot on 3.1);
 * without the kit, ENVARC:vtcon.history as before.
 *
 * Portable C89 over cl_sys, host-tested (tests/test_histfile.c); the handler
 * runs it in its completion worker over sys_amiga. */
#ifndef HISTFILE_H
#define HISTFILE_H

#include "../claude/sys.h"

#define HF_PROG "vtcon"                   /* UP-Term:var/vtcon */
#define HF_NAME "history"                 /* the file in it */
#define HF_OLD  "ENVARC:vtcon.history"    /* where it was, and the place without the kit */
#define HF_MAX  256                       /* a history file's name, NUL included */

/* The history file for shell_file (0 or "": the console's default) into out
 * (cap bytes), the default's drawer made and an old file moved into it.
 * 0, -1 when it does not fit. */
int hf_path(cl_sys *sys, const char *shell_file, char *out, long cap);

#endif
