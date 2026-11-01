/* borderblank_core: the pure part of AM-BORDERBLANK (borderblank_patch.c). Which display modes are native (the
 * chipset draws the border, so VTAG_BORDERBLANK_SET applies) and the table of the screens Amarchy blanked, so Off
 * and quit clear exactly those and leave a screen its own program blanked alone. Host-tested (tests/test_borderblank.c). */
#ifndef AMARCHY_BORDERBLANK_CORE_H
#define AMARCHY_BORDERBLANK_CORE_H

#define BB_MAX 16

/* 1 when modeid is a native chipset mode: monitor IDs DEFAULT (0) .. DBLPAL (0x000A1000). RTG modes (0x40000000
 * and up on Picasso96 / CyberGraphX) and INVALID_ID are not. */
int bb_native_mode(unsigned long modeid);

typedef struct bb_table {
    const void *scr[BB_MAX];
    int n;
} bb_table;

void bb_init(bb_table *t);
/* 1 = added, 0 = already there, -1 = full (the screen stays blanked but Off will not clear it). */
int bb_add(bb_table *t, const void *scr);
/* 1 = dropped, 0 = not there. */
int bb_drop(bb_table *t, const void *scr);
int bb_has(const bb_table *t, const void *scr);

#endif
