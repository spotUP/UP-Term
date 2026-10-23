#ifndef AMARCHY_TERMENV_H
#define AMARCHY_TERMENV_H
/* A program started from a commodity or menu (no shell window above it) has no TERM in ENV:.
 * ncurses programs such as tmux then fail with "open terminal failed: not a terminal". A
 * shell window's startup gives TERM; this gives the same to a program launched without one.
 * Sets the global variable only when it is unset or empty. */
void termenv_ensure(void);
#endif
