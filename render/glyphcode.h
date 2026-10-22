/* render/glyphcode: how a glyph engine is told which glyph to draw, kept
 * apart from the engine so a host test can drive it with a fake one.
 *
 * A glyph engine (bullet API) takes the code in OT_GlyphCode (16 bits) or,
 * beyond the BMP, OT_GlyphCode_32. diskfonttag.h says an engine without the
 * 32-bit tag answers an error. ttf.library 0.8.5 does not: its SetInfo skips
 * a tag it does not know and answers OTERR_Success, so OT_GlyphMap then
 * draws the code set before (a plane-15 Nerd Font icon came out as the
 * previous glyph, and was cached as that). Whether an engine takes 32-bit
 * codes is therefore measured, never assumed from the answer: at open, a
 * BMP code is selected through the 32-bit tag after another one through the
 * 16-bit tag; the engine takes 32-bit codes only when the second one draws.
 * One that does not: every code above U+FFFF is missing (the replacement
 * box), never a stale glyph.
 *
 * Header only (static functions): the worker in render/outline.c and
 * tests/test_glyphcode.c include it. C89.
 */
#ifndef GLYPHCODE_H
#define GLYPHCODE_H

/* select code as the engine's current glyph: wide = 1 through the 32-bit
 * tag. 0 when the engine accepted it. */
typedef int (*vt_gc_set_fn)(void *engine, int wide, unsigned long code);
/* draw the current glyph: a signature of its bitmap, 0 when it draws
 * nothing (or the engine fails) */
typedef unsigned long (*vt_gc_draw_fn)(void *engine);

/* two BMP codes every text font draws, unlike each other */
#define VT_GC_PROBE_A 'M'
#define VT_GC_PROBE_B '0'

/* 1 when the engine selects a glyph by a code given through the 32-bit tag */
static int vt_gc_probe32(void *engine, vt_gc_set_fn set, vt_gc_draw_fn draw)
{
    unsigned long a, b, w;
    if (set(engine, 0, VT_GC_PROBE_B) != 0)
        return 0;
    b = draw(engine);
    if (set(engine, 0, VT_GC_PROBE_A) != 0)
        return 0;
    a = draw(engine);
    if (!a || !b || a == b) /* the font cannot tell them apart: no proof */
        return 0;
    if (set(engine, 1, VT_GC_PROBE_B) != 0)
        return 0;
    w = draw(engine);
    return w == b;
}

/* make cp the engine's current glyph; 0 when it is, and the glyph may be
 * drawn. codes32: what vt_gc_probe32 measured. */
static int vt_gc_select(void *engine, vt_gc_set_fn set, int codes32, unsigned long cp)
{
    if (cp > 0xFFFFUL && !codes32)
        return -1;
    return set(engine, cp > 0xFFFFUL, cp);
}

#endif
