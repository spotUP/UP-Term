/* outline: glyphs the bitmap font cannot show, from a TrueType font drawn
 * by UP-Term's own engine (render/ttf.c; no font library). Ledger F1, plans
 * thoughts/shared/plans/2026-10-03-outline-fonts.md and
 * 2026-10-22-handler-rasterizer.md.
 *
 * The engine runs in a worker process: the handler may not make DOS calls
 * (vtcon_handler.c, the profile reader's note), and the engine reads its
 * font file. The caller asks synchronously and each code point once; the
 * answer is kept (a mask in chip RAM, for BltTemplate) or remembered as
 * missing.
 *
 * Every call comes from the process that called vo_open (its reply port's
 * signal is that process's). Every code point (cmap formats 4 and 12),
 * AmigaOS 3.0+, any 68k.
 */
#ifndef OUTLINE_H
#define OUTLINE_H

#include <exec/types.h>

typedef struct vo_font vo_font;

/* The TrueType font `name` (FONTS:<name>.otag naming the .ttf, as
 * ttfinstall writes it, else FONTS:<name>.ttf; a ".otag" or ".font" suffix
 * is taken off), its glyphs sized to cells cw x ch with the bitmap font's
 * baseline `base` rows down. 0 when there is no such font, it is not
 * TrueType, or no memory: the caller draws as without one.
 * name "" opens the worker alone, for vo_read (its glyphs all missing). */
/* The file spec of the TrueType font `name` (the lookup vo_open uses: .otag naming the .ttf, else .ttf), into spec
 * (n bytes); 1 when that file opens, else 0. The file may still not be TrueType: ttf_open decides. */
int vo_ttf_spec(const char *name, char *spec, int n);
vo_font *vo_open(const char *name, WORD cw, WORD ch, WORD base);
void vo_close(vo_font *f);

/* The bitmap font changed size: glyphs are made again at the new cell. */
void vo_set_cell(vo_font *f, WORD cw, WORD ch, WORD base);

/* The glyph of cp over `cells` cells (1, or 2 for a wide character): ch
 * rows of *bpr bytes (cells * cw bits used), set bits the ink. 0 when the
 * font has no such glyph. */
const UBYTE *vo_glyph(vo_font *f, ULONG cp, int cells, WORD *bpr);

/* A combining mark's glyph to draw over a character `cells` wide: the
 * font's zero-advance mark placed over the cell (or, a spacing one, from
 * its left edge). 0 when the font has no such glyph. */
const UBYTE *vo_mark(vo_font *f, ULONG cp, int cells, WORD *bpr);

/* The coverage of the glyph whose mask vo_glyph or vo_mark returned last
 * (mask that pointer): ch rows of cells * cw bytes, 0..255 per pixel, the
 * antialiased glyph (plan 2026-10-22-handler-rasterizer.md R4). 0 for any
 * other mask, and on the library's path (only the mask there). */
const UBYTE *vo_coverage(const vo_font *f, const UBYTE *mask);

/* The name it was opened with (for a profile change: same font or not). */
const char *vo_name(const vo_font *f);

/* 1 when it has a font (not opened with ""). */
int vo_has_engine(const vo_font *f);

/* The screen's pixel is rx x ry ticks (DisplayInfo Resolution; 1:1 until
 * told): glyphs keep their designed shape on it, except those made to fill
 * the cell (box drawing, blocks, powerline separators) and combining marks,
 * which take the cell's shape (plan 2026-10-22-handler-rasterizer.md R6.1). */
void vo_set_aspect(vo_font *f, WORD rx, WORD ry);

/* Glyphs UP-Term's own TrueType engine (render/ttf.c) drew for this font so far:
 * the reachability sentinel /state outline-own reads. 0 on the library's path. */
ULONG vo_own(const vo_font *f);

/* The file at path (under 64 characters) read by the worker, which may make
 * the DOS calls the caller may not (ledger U2: the Unifont pages): at most
 * max bytes into buf. The bytes read; -1 when there is no such file. */
LONG vo_read(vo_font *f, const char *path, UBYTE *buf, LONG max);

#endif
