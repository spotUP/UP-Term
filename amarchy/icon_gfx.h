/* icon_gfx -- the drawn menu's app icons on the Amiga (AM-IDX-ICON I1): an index row's .info image,
 * colour-mapped to the screen by icon.library V44, scaled to the row's icon box, cached per path
 * (amarchy/icon_core.h decides the slots). */
#ifndef AMARCHY_ICON_GFX_H
#define AMARCHY_ICON_GFX_H

#include <graphics/rastport.h>
#include <intuition/screens.h>

/* Opens icon.library (V44 or newer: the screen remap and DrawIconStateA) for screen scr. 1 when icons
 * can be drawn; 0 leaves every row on the generic glyph. */
int ig_open(struct Screen *scr);
/* Frees every cached bitmap, closes the library. Safe to call when ig_open failed. */
void ig_close(void);
/* The icon of the tool or project at path (no .info) drawn into rp: centred in the box bw x bh whose top
 * left is (x, y). The image is loaded on the first call and kept; 1 when it was drawn, 0 when the icon is
 * unavailable (the caller draws the glyph). */
int ig_draw(struct RastPort *rp, const char *path, int x, int y, int bw, int bh);
/* How many icons have been loaded from disk since ig_open (the rig's sentinel and the cache check). */
long ig_loads(void);

#endif
