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

#endif
