#ifndef SERVE_SIG_H
#define SERVE_SIG_H
/* The warm menu server outlives every console it was started from. ixemul sends SIGHUP to a task whose XCON
 * console closes; with no handler its default action prints "Hangup - <program>" into whatever window is
 * current (the menuhost window, over the Packages row) and kills the server. SIGINT is routed to "close the
 * menu" (menu.c on_int), but a hangup is not a request to close a menu: the console that went away is not the
 * window the menu draws in, so the server ignores it. Only the server process: jobs and launches are separate
 * AmigaDOS processes (SystemTags) with their own ixemul state, so they keep the default action. */
#include <signal.h>

static void serve_ignore_hangup(void)
{
    signal(SIGHUP, SIG_IGN);
}
#endif
