/* The Amarchy demo tour (design: thoughts/shared/research/s61-evidence/amarchy-demo-research.md). The ARexx verb
 * DEMO [STOP|STATUS|<n>] only arms a state (demo_port); every action runs from demo_tick on the main task, one scene
 * at a time, from the 1 s commodity tick. A key press or click ends it (cx_watch_input), and the desktop is put back. */
#ifndef AMARCHY_DEMO_H
#define AMARCHY_DEMO_H

/* Verb text after "DEMO". Returns 0 done (out holds the STATUS text), 10 for any other word. */
int demo_port(const char *verb, char *out, int cap);
void demo_tick(long secs);        /* tick hook (tick.h) */
void demo_shutdown(void);         /* the commodity quits: a running demo is stopped and the desktop restored first */
/* The BAR verb of Amarchy (bar_port, static in amarchy.c): the demo calls the same function, never rexx_send to its
 * own port (that would wait on the task that is calling). args is the text after "BAR". */
void demo_set_bar(long (*bar)(const char *args, char *out));

#endif
