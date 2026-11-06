/* pubscr -- the one guarded lock of a public screen by name, shared by the handler and every Amarchy program.
 * Amiga only. Moved out of amarchy/iconpen_dos.c (TE-10.1) so handler/frontscr.c (FRONTSCR-PUB-LOCK) can use it. */
#ifndef PUBSCR_H
#define PUBSCR_H

#include <exec/lists.h>

struct Screen;
struct PubScreenNode;

/* The node called `name` in the public screen list (the caller holds its lock). 0 when no such screen is listed.
 * Names compare with strcmp (case-sensitive), as Intuition's own public screen names are. */
struct PubScreenNode *pubscr_node_in(struct List *pl, const char *name);

/* The public screen `name`, locked (the caller UnlockPubScreen(0, scr)), or 0 when it is not listed, still private, or the
 * name is null or empty. The ONLY place in handler/ and amarchy/ that calls LockPubScreen with a name
 * (tests/test_wb_lock_guard.py): Intuition reopens a Workbench it does not find, and that deadlocks
 * (BOOT-31-WB-DEADLOCK, S57-FREEZE-2IN9, FRONTSCR-PUB-LOCK). Needs IntuitionBase open and iconpen_core.c linked. */
struct Screen *ipd_lock_pub_screen(const char *name);

/* The default public screen, locked (the caller UnlockPubScreen(0, scr)), or 0 when it is closed, still opening or private.
 * Use it where LockPubScreen(0) was: that call reopens a closed default Workbench on OS 3.1 (BOOT-31-WB-DEADLOCK). */
struct Screen *ipd_lock_default_pub_screen(void);

#endif
