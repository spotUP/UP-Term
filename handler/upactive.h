/* The public semaphore "UPTERM.active": how many UP-Term windows have keyboard
 * focus. The XCON: handler maintains it (ACTIVEWINDOW / INACTIVEWINDOW), the
 * Amarchy commodity reads it to leave a key to the terminal when a window of
 * ours is active (plan 2026-10-22-amarchy-commodity.md section 2, Coexistence). */
#ifndef UPACTIVE_H
#define UPACTIVE_H

#define UPACTIVE_NAME "UPTERM.active"

/* Writer (the handler): create the semaphore if absent. 1 when it exists. */
int upactive_publish(void);
/* Writer: one window gained (on = 1) or lost (on = 0) focus. */
void upactive_note(int on);
/* Reader: the focus count, 0 when the semaphore does not exist. */
long upactive_count(void);


/* The window whose request port is `self` (its id, upreq.h) gained focus: it goes to the front
 * of the focus record (upmru_*, keymap_act.h). The quick menu is itself an UP-Term window, so
 * the terminal it acts on is the one focused before it, however many windows closed since. */
void upactive_focus(unsigned long self);
/* The window `self` goes: taken out of the record, the others keep their order. */
void upactive_forget(unsigned long self);
/* The previous window's port name into out; 1 when there is one. */
int upactive_prev(char out[24]);

#endif
