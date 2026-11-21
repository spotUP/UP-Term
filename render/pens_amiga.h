/* pens_amiga -- a screen colour map's pen for an 0xRRGGBB colour, shared by the renderer and the programs that
 * draw into a window of their own (patina-gui). The rule is render/pen_rule.h: exact while the map has room,
 * the nearest pen when it is full. */
#ifndef PENS_AMIGA_H
#define PENS_AMIGA_H

struct ColorMap;

/* The pen for rgb (0xRRGGBB) on cm: its own entry or the nearest one; -1 when none. Every answer >= 0 is one
 * ReleasePen(cm, pen) owed. */
long vr_pen_obtain(struct ColorMap *cm, unsigned long rgb);

/* One step of the rule, for the renderer's few-pens paths: exact 1 is that colour or -1 (OBP_FailIfBad), exact 0
 * the nearest pen. cm is the struct ColorMap. Each pen >= 0 is one ReleasePen owed. */
long vr_pen_best(void *cm, unsigned long rgb, int exact);

/* S92-PENS-BOOT-ORDER: vr_pen_best's shape, but FindColor: the nearest pen the screen already shows (exact ignored), never
 * allocated, so NO ReleasePen is owed. For screens where patina-flat's holder owns the pens (desk_dos_holder_owns). */
long vr_pen_find(void *cm, unsigned long rgb, int exact);

#endif
