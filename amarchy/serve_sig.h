#ifndef SERVE_SIG_H
#define SERVE_SIG_H
/* A menu process must survive its console closing until run_ui returns. ixemul sends SIGHUP to a task whose XCON
 * console closes; with no handler its default action prints "Hangup - <program>" into whatever window is current
 * and kills the process wherever it is. Both menu processes run the UI in a CLOSEINACTIVE console:
 * - the warm server outlives every console it was started from; unhandled, the hangup printed over the Packages
 *   row of the menuhost window and ended the server;
 * - the cold menu (`amarchy-menu menu`, the path ms_open takes when no server answers) loses its console the moment
 *   a leaf opens the UP-Term window; unhandled, the hangup killed it mid-launch, inside the DOS call, and the
 *   reply that came back to the dead process was a "Software Failure" (AN_AsyncPkt) requester.
 * SIGINT is routed to "close the menu" (menu.c on_int), but a hangup is not a request to close a menu: a closed
 * console only makes the next read see EOF, and run_ui ends there. Only the menu process: jobs and launches are
 * separate AmigaDOS processes (SystemTags) with their own ixemul state, so they keep the default action. */
#include <signal.h>

static void menu_ignore_hangup(void)
{
    signal(SIGHUP, SIG_IGN);
}
#endif
