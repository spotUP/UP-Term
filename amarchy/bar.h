/* The Amarchy bar (bar_core.h is its portable half): a borderless window along the bottom of the Workbench public
 * screen that never takes the focus. The commodity opens it after cx_install and serves it in its Wait loop (no
 * task of its own, no timer: a segment that changes text calls bar_set and the bar redraws that segment only).
 * Amiga only. */
#ifndef AMARCHY_BAR_H
#define AMARCHY_BAR_H

/* Opens the bar; dir is where amarchy-gui lives (the agents panel is its drawn menu). 1 when the window is up,
 * 0 when the screen, a pen or the window is not available (the commodity runs without a bar). */
int bar_open(const char *dir);
void bar_close(void);
/* The signal bit mask of the bar's window port, 0 when the bar is not open. */
unsigned long bar_sigmask(void);
/* Handles the window's messages: clicks, a refresh. */
void bar_serve(void);

/* The commodity's input filter calls this (the input task) for a right button press: 1 when the pointer is over a
 * segment the right button acts on, whatever window is active. bar_right_click then (the main task) does it. */
int bar_takes_right_press(void);
void bar_right_click(void);

/* Counters for the STATUS reply: segments drawn, panels opened, agents launched; bar_is_open is 1 or 0. */
long bar_draws(void);
long bar_panels(void);
long bar_launches(void);
int bar_is_open(void);

#endif
