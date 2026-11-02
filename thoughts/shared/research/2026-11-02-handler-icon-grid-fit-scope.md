---
date: 2026-11-02
topic: HANDLER-ICON-GRID-FIT scope - which handler paths downsample art to non-square pixels
status: complete (read-only survey, grep + ranges; amarchy/ not covered)
---

# Result

Only ONE handler path rasterises vector art at a non-square aspect: render/ttf.c (TrueType outlines), driven by render/outline.c. No stroke-segment icon rasteriser exists in render/, engine/, handler/ or pic/. The gui_iconbake technique (segments) does not port directly. The only plausible port is an outline equivalent (stem/edge snapping), and it matters for icon-font / box-like glyphs only.

| Path | Rasterises | Non-square handling | Hints / grid-fit today | Grid fit applies? |
|---|---|---|---|---|
| render/ttf.c:786 ttf_fit, :811 ttf_place; render/outline.c:95-108 (ttf_place then ttf_render), vo_set_aspect :458 (rx, ry = pixel in ticks) | TTF glyf outlines (quadratic curves), 26.6 scanline area-coverage accumulator (SUB 64, ttf.c:5-8) | Yes: xscale/yscale computed per axis from rx:ry so an em stays in shape (ttf_fit: ys = h, xs = h*ry/(rx*em)); TTF_FILL scales x to cell width, y to line height independently (:811-822). Exact area coverage, so no supersample | None. Scale is applied to raw font units (ttf.c:591-592 scale(fx, xs)); no glyf instructions run, no stem snap, baseline/ox only rounded to whole pixels | Partly: segment fitting as such no (curves, arbitrary glyphs); an outline equivalent yes, see below |
| render/outline.c:134 fills_cell (U+2500-259F, U+1FB00-1FBFF, U+E0B0-E0D7) -> TTF_FILL | Box/block/legacy-computing/powerline glyphs, when drawn from the TTF | Same as above (independent xs/ys filling cell) | None; a 1-unit-wide stem lands wherever the scale puts it, thin lines straddle rows in tall-pixel modes | Yes, the best candidate: these are axis-aligned rectangles/lines, so stem snapping == gui_iconbake fit_axis |
| render/amiga_render.c:1226 draw_box, :1250 draw_block (code tables, glyph_tables.inc) | Synthesised box drawing (Move/Draw lines), block elements (RectFill, half = w/2) | Not aspect aware but pixel-exact by construction: arms at cx = cw/2, cy = ch/2, 1 px light, 2 px heavy, integer pixels | Already pixel-aligned (integer), so nothing to snap; light line is 1 px in both axes (visually thinner horizontally in 1:2) | No: already integer-exact. Only a policy question (heavy = 2 px horizontal is 4 ticks thick vs 2) |
| render/aa_ink.c (whole file) | Not a rasteriser: maps TTF coverage masks to shade pens/dither | n/a | n/a | No (consumer of ttf coverage) |
| render/unifont.c:66 uf_make_mask | Bitmap 8x16 / 16x16 Unifont glyphs | Cell is cw x 16 or 8 rows; across 1:1, or pixel pairs OR'd when 2x too wide (hs=2), centred/cropped; down 1:1 (ch 16) or row pairs OR'd (ch 8). Integer decimation only, no filtering | Integer, no resampling blur; OR-pair merge is the only reduction | No: not vector, no sub-pixel positions; OR-merge already avoids dim rows. Aspect is never consulted (only cw, ch) |
| render/emoji.c:153 ce_set_cell, :175 ce_paint | Pre-baked colour emoji bitmaps (16 px wide, two heights per record: 16 and 8 rows, gen_emoji.py) | No scaling at runtime; picks the baked image by cell; refuses cells under 2*CE_W | Baked offline (tools, not handler) | No in handler; the offline generator (gen_emoji.py, not surveyed) is where a fit would go |
| render/amiga_render.c:1506 draw_script, :2286 draw_double_row (BitMapScale at :1550, :2348) | Text glyphs masked then BitMapScale: 2/3 scale (super/subscript), 2x wide / 2x tall (DEC double rows) | Not aspect aware | None; OS BitMapScale (nearest-ish) | No: bitmap font glyphs, integer or fixed ratios, no stroke metadata |
| engine/kitty.c:144 kit_make (nearest pixel + dither) ; pic/icat.c:246 (fit to window, cell box :388) | Raster images (kitty graphics PNG/RGB) | icat keeps fw/w == fh/h in pixels; kit_make nearest-pixel to tw x th | None | No: photographs/arbitrary rasters, no axis strokes known |
| engine/png.c, engine/ansimusic.c, handler/ | PNG decode, music; handler/ has no glyph code found in the greps | n/a | n/a | No |

# Why segment fitting does not port to TTF

gui_iconbake.c:310-425 works because its input is a list of segments with known axis alignment (is_axis/is_dot), so it can snap centres and force a 1 px stroke. A TTF glyph is a contour list; strokes are not tagged.

Cheap outline equivalent (design sketch, not measured, not built):
1. In ttf.c at the point coordinates are scaled (:591-592), detect axis-aligned edges: consecutive on-curve points with equal fx (vertical) or equal fy (horizontal).
2. Pair opposite edges of one contour into stems (left+right edge, bottom+top edge); snap the stem centre to a pixel centre per axis (same fit_axis rule, v<n/2 ? floor+.5 : ceil-.5) and set stem width to round(width*scale) clamped to >= 1 px. Move every point that shares that fx/fy value with it (the value-keyed fit_has idea).
3. Apply only when rx != ry (square modes unchanged, matches the ledger rule), and only to TTF_FILL box/block/powerline glyphs first (pure axis-aligned rectangles, so the pairing is trivial), then icon fonts (Nerd Font / Font Awesome PUA) as a second step. Curved text glyphs: leave alone.
Size: roughly 100-150 lines in ttf.c plus a test through vo/ttf_render at ry:rx = 2:1.

# Verdict on the ledger premise

The row said "unverified that any do": confirmed - the handler has exactly one vector scaler (ttf.c). The segment-based reference applies to none; stem snapping applies to the TTF_FILL box/block/powerline set. Everything else is either integer-exact already or raster/bitmap with no stroke information. Amarchy-side icon code is out of scope here.
