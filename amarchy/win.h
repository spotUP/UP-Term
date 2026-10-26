/* The win: action kind on the Amiga side (plan 2026-10-25-amarchy-window-groups.md, section 1.3): the OS calls
 * behind the pure geometry of win_core.c. Only the commodity task calls these; the terminal handler never does. */
#ifndef AMARCHY_WIN_H
#define AMARCHY_WIN_H

#define WIN_RC_OK      0
#define WIN_RC_REFUSED 5    /* no usable target window (or the OS call was not possible) */
#define WIN_RC_UNKNOWN 10   /* not one of the closed verb forms */

/* Run `win:<arg>` from a hotkey: the live active window is the target, any menu ticket is dropped. 1 when the
 * verb ran, 0 when it was refused (DisplayBeep). */
int act_win(const char *arg);

/* The same verb from the ARexx port (the drawn menu's row): when the active window is an Amarchy window the
 * ticket taken at menu launch is the target, if that window is still listed and its box is unchanged since the
 * ticket was taken (else refused, DisplayBeep). Returns WIN_RC_*. */
int win_port(const char *arg);

/* Record the window the owner was in when a menu or viewer opens (the menu then becomes the active window). */
void win_ticket_take(void);

/* Whether the active window is an UP-Term terminal (*in_term: the "UPTERM.active" focus count is above 0 and an
 * Intuition window is active) and a full-screen one (*full: backdrop, borderless, the screen's size). */
void win_active_terminal(int *in_term, int *full);

/* Verbs executed so far (the STATUS `wins=` sentinel). */
long win_count(void);

#endif
