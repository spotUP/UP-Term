/* glyph_pen -- which pen a cell's glyphs are drawn in on a screen of few bitplanes (render/amiga_render.c pen_for and
 * cell_style), apart from AmigaOS so the host tests it (tests/test_pens.c). */
#ifndef GLYPH_PEN_H
#define GLYPH_PEN_H

/* An Amiga-personality colour (SGR 30-37 / 40-47 name screen pens) as the pen a screen of `depth` bitplanes draws:
 * graphics keeps only the low `depth` bits of a pen number, so pen 4 on a 4-colour screen is pen 0. The renderer
 * compares this number, not the one asked for: pen 4 text on pen 0 passed the "text in the background's pen" check
 * and drew pen 0 on pen 0, invisible (owner 2026-10-24, black on black on a low-colour screen). */
static unsigned vr_screen_pen(unsigned pen, int depth)
{
    return depth >= 8 ? pen & 0xFFu : pen & ((1u << depth) - 1u);
}

/* The pen a cell's glyphs are drawn in: its text pen, unless that is the background's pen while the cell asked for
 * two different colours (a screen of few pens gave both the same one). The plain text pen then, or the plain
 * background pen when the background is the text pen. */
static unsigned long vr_glyph_pen(unsigned long fg, unsigned long bg, int asked_differ, unsigned long dfg,
                                  unsigned long dbg)
{
    if (fg != bg || !asked_differ)
        return fg;
    return bg == dfg ? dbg : dfg;
}

#endif
