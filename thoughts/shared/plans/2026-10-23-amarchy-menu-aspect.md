---
date: 2026-10-23
topic: Amarchy drawn menu follows the screen's pixel aspect, font pair and a 2x size
status: open
follows: thoughts/shared/plans/2026-10-23-amarchy-drawn-menu.md (G8 owner check)
---

# Amarchy drawn menu: pixel aspect, font pair, size

## Owner report (G8, A1200, 640x256 PAL hires)

- "it's running with wrong aspect for 640x256", "everything feels a bit small",
  "it doesnt look close to the real omarchy".
- "it needs to be resolution aware square or tall pixels and use the correct font and icon size".
- "we have the topaz font for [tall pixel] screens and we have my topaz pro font for square pixel
  resolutions".
- Size choice (AskUserQuestion 2026-10-23): "2x, square icons": on 640x256 topaz 8, rows about
  30 px, icons 16 lines tall and 32 px wide, about 5 rows before the scroll cue.

## Violated invariant

A length on screen must look the same across and down. menu_geom.c treats a pixel as square
(icons are an s x s pixel square, horizontal pads are Omarchy px * w / 1920) and scales every
size by the screen's share of 1080 lines. On 640x256 hires a pixel is twice as tall as wide
(DisplayInfo Resolution 22 x 44 ticks), so icons are squashed to half width and the window is
narrow and small. The font is the screen's font, never the pair the terminal already uses
(render/fontpair.c: topaz 8 on tall pixels, TopazPro 16 on square ones).

Root level: the geometry's input model (mg_screen has no pixel aspect; sizes are not tied to the
font) plus gui.c's font pick. Not a drawing fix.

## Design

1. `mg_screen` gains `rx, ry`: DisplayInfo Resolution ticks of one pixel (0, 0 = square). New
   `int mg_cols(const mg_screen *s, int lines)` = `(lines * ry + rx / 2) / rx` (lines when either
   is 0): the columns that look as long as `lines` lines.
2. Sizes come from the font, not the screen share. Row height `R = ysize * 15 / 4` (topaz 8: 30;
   TopazPro 16: 60, so the menu has the same physical size on 640x256 and 640x512). Omarchy's
   px (its rows are 50 px) become `V(px) = (px * R + 25) / 50` down and `mg_cols(V(px))` across:
   - by = max(2, V(2)), bx = mg_cols(by); vpad = V(18); head_h = max(V(34), ysize + 2 * V(6));
     gap = V(6); row_gap = max(1, V(3)); hpad = max(cw, mg_cols(V(18)));
     row_pad, icon_gap, chev_gap = mg_cols(V(12)), mg_cols(V(10)), mg_cols(V(12)), each at least
     cw / 2; min_w = mg_cols(V(300)); margin_y = V(10), margin_x = mg_cols(margin_y).
   - Worked for topaz 8 on 640x256 (rx 22, ry 44): row 30, pitch 32, vpad 11, head 20, gap 4,
     hpad 22, row_pad 14, icon_gap 12, min_w 360; root (3 rows) window 360 x 144; 5 rows fit
     in the 70 % room, more scroll with the peek.
   - mg_layout keeps MG_MAX_ROWS 8, the 70 % room and the 55 % peek.
3. Icons: height `mi_size = R * 8 / 15` (16 for topaz 8, 32 for TopazPro 16), MI_MAX 32;
   width `mg_cols(height)` up to MI_MAX_W 64. `mi_render(glyph, w, h, out)` box-filters the
   16 x 16 glyph to w x h. gui.c keeps `icon_w` (width, for mg_width and the label offset) and
   adds `icon_h` (centring in the row). tools/rig/amarchy_geom.c prints both.
4. Font: gui.c reads the screen's DisplayInfo (as vtwin_set_screen, render/vtwin.c:797), fills
   ms.rx / ms.ry, and opens `fontpair_choose("topaz.font", 8, square, square_fits)` with
   OpenDiskFont (diskfont.library; TopazPro ships in dist/fonts). Falls back to the screen font
   when the open fails. `square_fits` moves into fontpair as
   `int fontpair_square_fits(int screen_h, int bar_h)` and vtwin_set_screen calls it too (one
   rule). Close the font at exit.
5. Docs: menu_geom.h header comment (the size rule changes from the screen share to the font
   row), the drawn-menu plan's G8 note pointing here.

## Tests

- tests/test_keymap.c `drawn_menu_geometry_follows_the_font_and_screen`: rewrite the expected
  numbers from the rule above, by hand. New case named after the symptom:
  `drawn_menu_icons_look_square_on_tall_pixels`: on {640, 256, 22, 44} with topaz 8 the icon is
  16 high and 32 wide, hpad is twice vpad's line count; on {640, 512, 22, 22} with TopazPro 16
  (16, 8... use the real TopazPro cw) the icon is 32 x 32. Revert-proof: with mg_cols returning
  `lines` the tall case fails.
- tests/test_fontpair.c: fontpair_square_fits on 256 and 512 lines.
- Reachability: tools/rig/amarchy_gui_rig.py on a rig (as G7) compares the window and icon cell
  with amarchy_geom's numbers; update it for icon w x h and the opened font.
- Owner check on the A1200: `RAM:amarchy-gui menu`, then `RAM:amarchy-gui viewer`.

## Checklist

- [x] A1 mg_screen rx/ry + mg_cols
- [x] A2 font-row metrics (mg_metrics_for)
- [x] A3 icon w x h (mi_size, mi_render, gui.c draw_icon, amarchy_geom.c)
- [x] A4 fontpair_square_fits, vtwin uses it
- [x] A5 gui.c: DisplayInfo, fontpair font via OpenDiskFont, fallback, close
- [x] A6 host tests rewritten + symptom test + revert-proof
- [x] A7 rig test updated and run (rig 4, Kickstart 40.68: PASS; scrprobe now shares amarchy/menu_font.c with the gui)
- [x] A8 docs
- [~] A9 owner A1200 check 2026-10-25: aspect PASS (icons square, window wide enough); owner: border colours look wrong, and asked whether the user can theme Amarchy (menu_theme.c: border = palette index 14, Bright Cyan, remappable through the upconf palette). Colour question open.
