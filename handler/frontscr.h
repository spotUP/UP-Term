/* frontscr -- the front screen as a locked public screen, shared by the handler (a window opened with FRONT)
 * and amarchy-gui. Amiga only. */
#ifndef FRONTSCR_H
#define FRONTSCR_H

struct Screen;

/* The public name of the front screen into out (cap bytes, always terminated): 1 when it has one, 0 and ""
 * when the front screen is not public (no SCREEN option then: the default public screen). Needs
 * IntuitionBase open. */
int front_screen_name(char *out, int cap);

/* The front screen locked by its public name (UnlockPubScreen owed), 0 when it is not public: the caller
 * falls back to the default public screen. */
struct Screen *lock_front_screen(void);

#endif
