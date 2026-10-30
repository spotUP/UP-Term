#ifndef IPREFS_WAIT_H
#define IPREFS_WAIT_H

/* seconds a wedged IPrefs may keep the waiters */
#define DESK_IPREFS_SECS 6

/* The wait mask of IPrefs while it waits for a notification (0: no IPrefs or never seen waiting) */
unsigned long desk_iprefs_idle_mask(void);
/* Returns once IPrefs is back in its idle Wait() with nothing pending (or after DESK_IPREFS_SECS) */
void desk_iprefs_wait(unsigned long idle_mask);

/* The same with a caller-chosen bound: the boot tool waits for a starved IPrefs far longer than the prefs apply does */
unsigned long desk_iprefs_idle_mask_for(int secs);
void desk_iprefs_wait_for(unsigned long idle_mask, int secs);
#endif
