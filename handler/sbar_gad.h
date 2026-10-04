/* sbar_gad: the scroll bar in a window's right border (SB1) -- a BOOPSI
 * propgclass knob and the up and down arrows (buttongclass with the
 * screen's sysiclass images), as on a Workbench window. The knob's numbers
 * come from render/sbar (sbar_knob); the gadgets report through
 * IDCMP_IDCMPUPDATE (sbar_gad_event).
 *
 * Only for a window with a sizing gadget in its right border
 * (WFLG_SIZEGADGET | WFLG_SIZEBRIGHT): that border is already as wide as
 * the gadgets, so the text area (vr_layout, from BorderRight) never
 * reaches under them. A borderless window (FULLSCREEN, BACKDROP) has no
 * border to put them in. */
#ifndef SBAR_GAD_H
#define SBAR_GAD_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>
#include "../render/sbar.h"

#define SBAR_GID_PROP 0x5B10
#define SBAR_GID_UP   0x5B11
#define SBAR_GID_DOWN 0x5B12

typedef struct sbar_gad {
    struct Window *win;          /* 0: not on a window */
    struct DrawInfo *dri;        /* the images draw with it: kept while they live */
    Object *up_img, *down_img;
    struct Gadget *prop, *up, *down;
    sbar_knob shown;             /* what the knob shows now */
    int shown_valid;
} sbar_gad;

/* 1 when the window has a border for it (see above). */
int sbar_gad_fits(const struct Window *win);
/* Made and added to win, drawn with a full knob. 0: no border for it, or
 * no memory (nothing is left on the window). */
int sbar_gad_open(sbar_gad *g, struct Window *win);
/* Off the window and freed (before the window closes); a no-op when not open. */
void sbar_gad_close(sbar_gad *g);
/* The knob as k says, redrawn only when it changed. */
void sbar_gad_set(sbar_gad *g, const sbar_knob *k);
/* An IDCMP_IDCMPUPDATE from one of ours: 1, with its gadget's id and, for
 * the knob, its PGA_Top (else ~0). 0: not ours. Read before ReplyMsg. */
int sbar_gad_event(const struct IntuiMessage *im, ULONG *id, ULONG *top);

#endif
