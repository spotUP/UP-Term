/* icon_core -- the drawn menu's app icons, the portable part (plan 2026-10-23-amarchy-app-index.md,
 * AM-IDX-ICON I1). No OS calls, so tests/test_keymap.c covers it. amarchy/icon_gfx.c loads the .info image
 * of an index row and keeps one scaled bitmap per cache slot; this file decides which slot a path owns and
 * how big the icon is drawn. A path whose icon failed to load keeps its slot too, so a row is not tried
 * again on every redraw. */
#ifndef AMARCHY_ICON_CORE_H
#define AMARCHY_ICON_CORE_H

#include "index_core.h"

#define IC_SLOTS 24             /* icons kept at once: more than a level shows rows (topaz 8 on 480 lines: 20) */

#define IC_EMPTY  0
#define IC_READY  1             /* the slot's image is loaded */
#define IC_FAILED 2             /* it could not be loaded: the row draws the generic glyph */

typedef struct ic_cache {
    char path[IC_SLOTS][IX_PATH];
    int  state[IC_SLOTS];
    long used[IC_SLOTS];        /* the tick of the last find or claim */
    long tick;
} ic_cache;

/* The slot that holds path, its use refreshed; -1 when there is none. */
int ic_find(ic_cache *c, const char *path);
/* A slot for path, now IC_EMPTY: a free one, else the one used longest ago. The caller frees what the
 * slot held (its state was IC_READY) before it fills the slot and calls ic_set. -1 when the path does
 * not fit IX_PATH. */
int ic_claim(ic_cache *c, const char *path);
void ic_set(ic_cache *c, int slot, int state);

/* The row's icon in a box of bw x bh (the menu's icon column): the image sw x sh scaled to fit the box
 * whole, aspect kept (up or down). dw, dh are the size drawn (at least 1), dx, dy its offset in the box
 * so it sits centred. 0 for a bad size. */
int ic_fit(int sw, int sh, int bw, int bh, int *dw, int *dh, int *dx, int *dy);

#endif
