/* icon_core -- the drawn menu's app icons, the portable part (plan 2026-10-23-amarchy-app-index.md,
 * AM-IDX-ICON I1). No OS calls, so tests/test_keymap.c covers it. amarchy/icon_gfx.c loads the .info image
 * of an index row and keeps one scaled bitmap per cache slot; this file decides which slot a path owns and
 * how big the icon is drawn. A path whose icon failed to load keeps its slot too, so a row is not tried
 * again on every redraw. */
#ifndef AMARCHY_ICON_CORE_H
#define AMARCHY_ICON_CORE_H

#include "index_core.h"

#define IC_SLOTS 24             /* the least slots ic_init is asked for (a level shows at most about 20 rows) */

#define IC_EMPTY  0
#define IC_READY  1             /* the slot's image is loaded */
#define IC_FAILED 2             /* it could not be loaded: the row draws the generic glyph */

typedef struct ic_cache {
    int   slots;                /* how many entries the arrays below hold */
    char (*path)[IX_PATH];
    int  *state;
    long *used;                 /* the tick of the last find or claim */
    long  tick;
} ic_cache;

/* Allocates the arrays for `slots` icons (at least IC_SLOTS) and empties them. 0 when memory is short. */
int ic_init(ic_cache *c, int slots);
void ic_free(ic_cache *c);

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

/* The persistent cache file (IG_CACHE_PATH): a header, then one entry per ready icon.
 *   header: "AMIC", version (1 byte), box width, box height (16 bit big endian each), screen depth (1 byte)
 *   entry : path (NUL terminated, under IX_PATH), dw, dh, dx, dy (16 bit big endian each, as ic_fit gives
 *           them), dw*dh pen numbers, dw*dh mask bytes (0 transparent, 1 opaque)
 * Encoders and the decoder work on byte buffers; the Amiga side does the file I/O and the pixel reads. */
#define IC_FILE_VERSION 1
#define IC_FILE_HEAD    10
#define IC_FILE_MAXDIM  64      /* dw and dh at most */
#define IC_FILE_MAXPIX  4096    /* dw * dh at most */

typedef struct ic_dec {         /* pointers into the decoded buffer */
    const char *path;
    int dw, dh, dx, dy;
    const unsigned char *pens;
    const unsigned char *mask;
} ic_dec;

/* Bytes written, or -1 when cap is too small or an argument is out of bounds. */
long ic_file_head(unsigned char *buf, long cap, int bw, int bh, int depth);
long ic_file_entry(unsigned char *buf, long cap, const char *path, int dw, int dh, int dx, int dy,
                   const unsigned char *pens, const unsigned char *mask);
/* Decodes up to max entries into out; returns how many. A header that differs from bw, bh, depth, or any
 * entry that is bad or cut short, yields 0 (the whole file is ignored). */
int ic_file_decode(const unsigned char *buf, long len, int bw, int bh, int depth, ic_dec *out, int max);

#endif
