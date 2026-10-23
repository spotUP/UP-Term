/* winspec: the XCON: window spec's size fields, resolved against the screen,
 * with no OS calls so the host tests cover it (tests/test_winspec.c).
 *
 * A width or height of 0 in "XCON:x/y/w/h/..." means "as wide / as high as
 * the screen, from the left / top edge on" (the Workbench icon's window).
 * Any other value is the size as given. Portable C89.
 */
#ifndef WINSPEC_H
#define WINSPEC_H

/* want: the spec's width or height; origin: its left or top edge; extent: the
 * screen's width or height. Returns the size to open the window with. */
int winspec_size(int want, int origin, int extent);

/* The default ground of a window whose spec chose no colours (DARK, LIGHT, FG,
 * BG): light grey 0xC0C0C0 on black, for every personality -- a CON: or RAW:
 * window (amiga escape dialect) looks like an XCON: one. Returns 1 and sets
 * *fg / *bg, or 0 and leaves them (the spec's colours stand). */
int winspec_ground(int colours, unsigned long *fg, unsigned long *bg);

/* The placement options of the spec (AM-OSD): CELLS <cols>x<rows>, CENTRE
 * (alias CENTER), FRONT, CLOSEINACTIVE. All zero = none given. */
typedef struct {
    int cols, rows;       /* CELLS: the text area in cells; 0 when not given */
    int centre;           /* CENTRE: centred on the screen, x / y fields ignored */
    int front;            /* FRONT: open on the front screen when it is public */
    int close_inactive;   /* CLOSEINACTIVE: losing focus acts as the close gadget */
} winspec_opts;

/* One option field of the spec ("CELLS 34x9", "centre", ...), matched without
 * regard to case. Returns 1 and updates *o when the field is one of these,
 * else 0 and *o is untouched. A malformed CELLS (no NxM, a zero) is not an
 * option: 0, so the width / height fields stand. */
int winspec_option(const char *field, winspec_opts *o);

/* The outer window box. With cells (o->cols and o->rows set) the box is the
 * text area (cols * cw by rows * ch) plus the borders bl/bt/br/bb and
 * overrides *w / *h; otherwise *w / *h stand as given. With o->centre it is
 * centred on a screen of sw x sh and overrides *x / *y. Either way the box is
 * then kept on the screen: shrunk to the screen when larger, moved in when it
 * hangs over an edge. Called once, at open: the window never moves after. */
void winspec_box(const winspec_opts *o, int cw, int ch, int bl, int bt, int br, int bb,
                 int sw, int sh, int *x, int *y, int *w, int *h);

#endif
