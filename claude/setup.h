/* setup -- the first-run wizard of C:Claude (todo CLAUDE-SETUP-WIZARD).
 *
 * Shown by the first start of the screen when there is no key
 * (ENV:ANTHROPIC_API_KEY, ENVARC:Claude/key), no ENVARC:Claude/remote and no
 * ENVARC:Claude/setup-done; reachable later as /setup and `Claude SETUP`.
 * A state machine fed one typed line at a time (repl_line hands the lines
 * to setup_line while a step is open), so it works in the screen and in
 * line mode alike and is host-tested through the REPL.
 *
 *   1 mode: Claude Code on another computer / an API key here / later
 *   2 checks: TCP/IP stack, AmiSSL 5 (key), C:uptelnet (remote), the
 *     computer reached (named errors) or the key's test request
 *   3 optional: amimcp / amiagent
 *   4 the settings written, a summary, ENVARC:Claude/setup-done
 *
 * The key goes through /login's own path (slash.c) and is never echoed or
 * logged. ENVARC:Claude/remote is written by setup_write_remote only; the
 * Installer's remote page writes the same file in the same format.
 * Portable C89. */
#ifndef CL_SETUP_H
#define CL_SETUP_H

#include "repl.h"

enum {
    SETUP_OFF = 0, SETUP_MODE, SETUP_HOST, SETUP_KEEP, SETUP_KEY, SETUP_INFO
};

/* the file's text: two comment lines, then "host port" */
int setup_remote_text(const char *host, long port, char *out, long cap);
/* <home>/remote written (the directory made): 0, -1 */
int setup_write_remote(cl_sys *sys, const char *home, const char *host, long port);
/* <home>/setup-done written: 0, -1 */
int setup_mark_done(cl_sys *sys, const char *home);
/* the first-run condition (see above) */
int setup_due(cl_repl *r);
/* opens the wizard at step 1 */
void setup_begin(cl_repl *r);
/* one typed line while a step is open: 1 consumed, 0 not (a slash command
 * ends the wizard and runs as usual) */
int setup_line(cl_repl *r, const char *line);

#endif
