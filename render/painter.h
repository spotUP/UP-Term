/* painter.h -- text straight into planar bitplanes at any bit phase (ledger
 * S1, creep's CCON 1.2.8 "CPU planar painter"). Portable: the C here is the
 * reference the 68k version (render/painter_68k.s) is checked against, and
 * what the host tests run.
 *
 * vp_span: n cells of an 8-pixel-wide font, all in one pair of pens, at
 * pixel x of row y (top of the cell) in a bitmap of `depth` planes of `bpr`
 * bytes a row. glyphs[c * h + r] is row r of character c. In each plane
 * whose bit is set in `mask` the cells' bits become the glyph where only
 * the foreground pen has that plane's bit, its inverse where only the
 * background has it, all ones or zeros where both or neither do. Pixels
 * outside the cells are left as they were, whatever x's bit phase. */
#ifndef VT_PAINTER_H
#define VT_PAINTER_H

typedef unsigned char vp_u8;
typedef unsigned int vp_u32; /* 32 bits on the host and with vbcc */

/* The long-writing painter (render/painter.c, CTerm's design): same result
 * as vp_span, whole longs per plane. 0 and nothing drawn when bpr or a
 * plane is not long aligned (vp_span_fast then uses the byte routines).
 * vp_long_spans counts the spans it drew (the sentinel tests read). */
int vp_span_long(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
                 const vp_u8 *chars, int n, int fg, int bg, int mask);
extern unsigned long vp_long_spans;

void vp_span(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
             const vp_u8 *chars, int n, int fg, int bg, int mask);

/* vp_span through the 68k plane loop where VP_ASM is defined (the
 * handler), else vp_span itself. engbench checks the two agree. */
void vp_span_fast(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
                  const vp_u8 *chars, int n, int fg, int bg, int mask);

#endif
