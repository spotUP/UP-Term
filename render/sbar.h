/* sbar: the scroll bar's knob from the scrollback view, and back -- the
 * arithmetic only, no OS calls, so it is host-tested (tests/test_sbar.c);
 * the gadget itself is the window owner's (handler/sbar_gad.c).
 *
 * The knob is in propgclass terms (PGA_Total, PGA_Visible, PGA_Top), whose
 * values are 16 bits: a scrollback longer than that is counted in units of
 * 2^shift lines. Top is the first line shown counted from the oldest
 * scrollback line; the view (vr_render.view) is how many lines back from
 * the live screen the window shows, so top = scrollback - view.
 *
 * Portable C89, as the engine. */
#ifndef SBAR_H
#define SBAR_H

typedef struct sbar_knob {
    unsigned short total, visible, top; /* PGA_Total, PGA_Visible, PGA_Top */
    unsigned char shift;                /* one knob unit is 2^shift lines */
    unsigned char live;                 /* 0: nothing to scroll (no scrollback, or the
                                         * alternate screen): the knob fills the track */
} sbar_knob;

/* The knob for sb scrollback lines, rows on screen, the view that many
 * lines back (0 = the live screen); alt: the alternate screen is shown,
 * which has no scrollback. */
void sbar_from_view(sbar_knob *k, long sb, int rows, int view, int alt);
/* The view a knob dragged to top shows (k as sbar_from_view made it for
 * the same sb): 0 at the bottom, sb at the top. */
int sbar_to_view(const sbar_knob *k, long sb, unsigned long top);
/* 1 when the two knobs look the same (no gadget update needed). */
int sbar_equal(const sbar_knob *a, const sbar_knob *b);

#endif
