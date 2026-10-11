/* pen_rule -- how the renderer asks a screen's colour map for a palette colour (render/amiga_render.c obtain()),
 * apart from AmigaOS so the host tests it (tests/test_pens.c).
 *
 * Exact first: while the map has a free entry, the colour gets an entry of its own with exactly its value. Only a
 * full map gives the nearest pen it has (the screen's own colours shared). Asking for the nearest first (graphics'
 * PRECISION_IMAGE) took a Workbench pen for any colour near one even with 242 entries free: on a clean 3.1 or 3.2.2
 * Workbench of 256 colours 55 of the xterm cube's 240 colours drew as Workbench colours ((238,153,0) for
 * (215,135,0); owner 2026-10-11). Each answer is one pen to release, as before. */
#ifndef PEN_RULE_H
#define PEN_RULE_H

/* best(u, 0xRRGGBB, exact): the screen's ObtainBestPen -- exact 1: that colour or -1 (OBP_FailIfBad), exact 0:
 * the nearest pen; -1 when none */
typedef long (*vr_best_fn)(void *u, unsigned long rgb, int exact);

static long vr_obtain_rule(vr_best_fn best, void *u, unsigned long rgb)
{
    long p = best(u, rgb, 1);
    return p >= 0 ? p : best(u, rgb, 0);
}

#endif
