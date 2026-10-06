/* rowpen.h -- what a planar screen's text rows are known to hold, so a run
 * writes only the bitplanes that change (ledger S1, race R5: the exact
 * plane mask). Portable: amiga_render.c keeps one record per screen row,
 * the host tests check the rules against a cell-by-cell model.
 *
 * A record says: the row's cells from column `from` to its end hold pen
 * `pen` in every plane (a scroll or a fill put it there; nothing drew
 * since). Left of `from` nothing is known. VRP_NONE: nothing known.
 *
 * A run of cells in pens fg / bg that starts at or right of `from` needs
 * only the planes where fg or bg differ from `pen` -- plain text on a
 * line that a scroll has just cleared writes the one plane its pen has,
 * whatever colours were on screen before; ANSI colour 0-7 runs write
 * one to three planes instead of every plane in use. Elsewhere the run
 * writes every plane in use (the window's mask), as before. */
#ifndef VT_ROWPEN_H
#define VT_ROWPEN_H

#define VRP_NONE 0x7FFF

typedef struct vrp_row {
    short from;          /* cells [from, end) hold pen; VRP_NONE: none known */
    unsigned char pen;
    unsigned char pad;
} vrp_row;

/* The planes a run of cells starting at column x in pens fg / bg must
 * write: those where a pen differs from what the cells hold, or the
 * window's mask when the cells are not known. */
#define VRP_MASK(row, x, fg, bg, mask) \
    ((x) >= (row)->from ? (((fg) ^ (row)->pen) | ((bg) ^ (row)->pen)) & 0xFF : (mask))

/* Cells up to column x1 (exclusive) were drawn: whatever is right of them
 * is still known. */
#define VRP_DRAWN(row, x1) \
    do { if ((row)->from < (x1)) (row)->from = (short)(x1); } while (0)

/* Does a fill of cells [x0, ...) in pen p change anything? 0 when they
 * hold p already. */
#define VRP_FILL_NEEDED(row, x0, p) (!((x0) >= (row)->from && (p) == (row)->pen))

/* Cells [x0, x1) of a row of `cols` columns were filled in pen p (every
 * plane: the fill wrote p's planes, the planes outside the window's
 * mask hold zeros). */
void vrp_filled(vrp_row *row, int x0, int x1, int cols, int p);

/* Rows [0, n): nothing known (a drawing the records do not follow). */
void vrp_forget(vrp_row *rows, int n);

/* Rows [0, n) hold pen p from column 0 (the whole text area filled). */
void vrp_clear(vrp_row *rows, int n, int p);

/* The pixels of rows [top, bottom) moved up by k rows (down when k < 0);
 * the rows vacated hold pen p. */
void vrp_scroll(vrp_row *rows, int top, int bottom, int k, int p);

#endif
