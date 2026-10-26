/* MENU-TOGGLE: the open menu (amarchy-gui or amarchy-menu) owns the public port MO_PORT; a menu hotkey
 * pressed while it exists closes that menu (Ctrl-C to its task) instead of opening a second one.
 * The port carries no messages: it is only the registry name -> task. */
#ifndef AMARCHY_MENU_OPEN_H
#define AMARCHY_MENU_OPEN_H

#define MO_PORT "AMARCHY.MENU"

/* The menu program, first thing in main: clear a stale Ctrl-C, add the port (unless one exists),
 * remove it again at exit (atexit). */
void mo_register(void);
/* RemPort (in Forbid) and free; safe to call twice. */
void mo_unregister(void);
/* The commodity: if a menu is open, tell it to close. 1 if one was found. */
int mo_close_open(void);

#endif
