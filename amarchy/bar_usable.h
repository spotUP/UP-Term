/* The one strut rule (roadmap R2.9): the area of a screen that is not the title bar and not the Amarchy bar. win.c,
 * AmTile and the notice window ask here; nothing else looks the bar up by title. Amiga only; the arithmetic is
 * win_core.c (win_strut, win_usable), host-tested. */
#ifndef AMARCHY_BAR_USABLE_H
#define AMARCHY_BAR_USABLE_H

#include "win_core.h"

struct Screen;

/* IBase locked (walks the screen's window list). Fills *s with the screen's size, title bar height and the bar's strut
 * (0 when the bar is off or on another screen), and *box (when not null) with the usable area. Returns the strut. */
int bar_usable(const struct Screen *scr, win_screen *s, win_box *box);

#endif
