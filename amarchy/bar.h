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
/* The signal mask of the re-raise poll timer (0 when the bar is not open) and its handler: every
 * BAR_RAISE_POLL_MICROS (bar_core.h) a window of another program in front of the bar and over it brings the bar
 * back to the front; Amarchy's own windows (title "Amarchy") are left above it. */
unsigned long bar_timer_sigmask(void);
void bar_timer_serve(unsigned long sigs);
/* Handles the window's messages: clicks, a refresh. */
void bar_serve(void);

/* The commodity's input filter calls this (the input task) for a right button press: 1 when the pointer is over a
 * segment the right button acts on, whatever window is active. bar_right_click then (the main task) does it. */
int bar_takes_right_press(void);
void bar_right_click(void);

/* Counters for the STATUS reply: segments drawn, panels opened, agents launched, re-raises; bar_is_open is 1 or 0. */
long bar_draws(void);
long bar_panels(void);
long bar_launches(void);
long bar_raises(void);
int bar_is_open(void);

#endif
