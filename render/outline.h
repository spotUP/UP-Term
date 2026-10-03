/* outline: glyphs the bitmap font cannot show, from an outline font through
 * the glyph engine API (bullet.library's, which ttf.library 0.8.5 speaks
 * for TrueType fonts). Ledger F1, plan
 * thoughts/shared/plans/2026-10-03-outline-fonts.md.
 *
 * The engine runs in a worker process: the handler may not make DOS calls
 * (vtcon_handler.c, the profile reader's note), and an engine reads its
 * font file. The caller asks synchronously and each code point once; the
 * answer is kept (a mask in chip RAM, for BltTemplate) or remembered as
 * missing.
 *
 * Every call comes from the process that called vo_open (its reply port's
 * signal is that process's). AmigaOS 3.0+ (OpenEngine on the library the
 * .otag names, 16-bit glyph codes), any 68k.
 */
#ifndef OUTLINE_H
#define OUTLINE_H

#include <exec/types.h>

typedef struct vo_font vo_font;

/* The installed outline font `name` (FONTS:<name>.otag; a ".otag" or
 * ".font" suffix is taken off), its glyphs sized to cells cw x ch with the
 * bitmap font's baseline `base` rows down. 0 when there is no such font,
 * no engine for it, or no memory: the caller draws as without one. */
vo_font *vo_open(const char *name, WORD cw, WORD ch, WORD base);
void vo_close(vo_font *f);

/* The bitmap font changed size: glyphs are made again at the new cell. */
void vo_set_cell(vo_font *f, WORD cw, WORD ch, WORD base);

/* The glyph of cp over `cells` cells (1, or 2 for a wide character): ch
 * rows of *bpr bytes (cells * cw bits used), set bits the ink. 0 when the
 * font has no such glyph. */
const UBYTE *vo_glyph(vo_font *f, ULONG cp, int cells, WORD *bpr);

/* The name it was opened with (for a profile change: same font or not). */
const char *vo_name(const vo_font *f);

#endif
