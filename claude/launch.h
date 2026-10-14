/* launch -- the question a plain `Claude` asks when it could run in two
 * places (ledger CL1): Claude on this Amiga with the API key, or Claude
 * Code on the NAS (ENVARC:Claude/remote, through uptelnet). The decision
 * table is cli_launch_mode (cli.h); this file is the question itself, the
 * file ENVARC:Claude/last-mode that makes the last choice the highlighted
 * one, and nothing else, so the host tests drive the real thing and
 * main_amiga.c only calls it.
 *
 * On an UP-Term console (io->read) the question is the selector of
 * tui_ext_menu (up, down, Enter; Esc and Ctrl+C cancel); on any other
 * console a numbered line question. Portable C89. */
#ifndef CL_LAUNCH_H
#define CL_LAUNCH_H

#include "cli.h"
#include "ui.h"

#define LAUNCH_LAST_FILE "ENVARC:Claude/last-mode"

/* the text of last-mode ("local" or "remote", anything else: local) as
 * LAUNCH_LOCAL / LAUNCH_REMOTE, and back */
int launch_last_parse(const char *text);
const char *launch_last_text(int mode);

/* LAUNCH_LOCAL or LAUNCH_REMOTE; -1 when cancelled (Esc, Ctrl+C, end of
 * input, three unusable answers). last = the choice highlighted at the
 * start (the line question's answer to a bare Enter). */
int launch_ask(cl_io *io, int last);

#endif
