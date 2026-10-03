---
date: 2026-10-03
topic: Outline fonts and full Unicode (ledger F1) -- glyphs the bitmap font lacks, from a TrueType font through the glyph engine API
tags: [vtcon, render, fonts, unicode, f1]
status: implemented
---

# F1: outline fallback glyphs

Owner 2026-10-03: "amigaos 3.2 supports vector fonts?", "i think it supports truetype", the
ttf.library links (Aminet util/libs/ttflib680x0), "we stay 3.1 compatible", "go ahead and build
F1". Why: Neovim configs (the owner's friend's tomviljo/dotfiles) use Nerd Font icons, which
UP-Term draws as '?'.

## Design (decided)

- **What draws from the outline font**: a cell whose code point the bitmap font cannot show
  as itself -- render/glyphmap's replacement '?' and its ASCII stand-ins (arrows, quotes,
  ...). The bitmap font keeps ASCII, Latin-1 (CP437 for IBM fonts), and the drawn box,
  block and DEC line glyphs (they join at any size). No outline font set, or the outline
  font lacks the glyph: today's drawing, unchanged.
- **The font**: profile key `font_fallback = <name>`, the name of an installed outline font
  (FONTS:<name>.otag, as ttf.library's ttfinstall / ttfmanager write it; ".otag" and ".font"
  suffixes accepted). Empty or absent: none. Prefs: a string field under Font.
- **The engine runs in a worker process** (render/outline.c). The handler may not make DOS
  calls (handler/vtcon_handler.c:540), and an engine reads its font file: ttf.library opens
  the .ttf. The worker owns the engine for the window's life: it reads the .otag, opens the
  engine library the file names (OT_Engine, e.g. "ttf"), sets the face, and rasterises
  on request. The renderer asks synchronously (one message, waits on its own reply port);
  each glyph is asked once and kept.
- **Size**: OT_DeviceDPI and OT_PointHeight chosen so a glyph fits the bitmap font's cell:
  point height = cell height at 72 dpi vertically; horizontal dpi so that the font's advance
  (glm_Width of a probe glyph, a fraction of the em) is one cell. A double-width cell
  (wcwidth 2, CJK) gets a two-cell mask.
- **Placement**: the glyph's origin (glm_X0, glm_Y0) on the cell's left edge and the bitmap
  font's baseline; clipped to the cell(s).
- **Missing glyphs**: an engine draws .notdef for a code the font lacks. At open the worker
  renders U+0001 (unmapped in every text font) and keeps it; a glyph identical to it is
  "missing" and the cell falls back to '?'. An empty bitmap for a non-space code is missing too.
- **Cache**: per renderer, open hashing on the code point, 1024 slots; a mask is cell-height
  rows of WORD-aligned bits in chip RAM (BltTemplate's source goes through the blitter).
  Full: cleared and refilled. A font size change (vr_set_font) recalibrates and clears it.
- **Drawing**: background fill, then BltTemplate in the foreground pen (JAM1); bold again
  one pixel right, as Text() bold; underline and the other lines through decorate().
- **3.1 floor**: OpenEngine on the library the .otag names, never through diskfont's 3.5+
  paths; glyph codes with OT_GlyphCode (16 bits; ttf.library: "codes above 0xFFFF not").
  Nerd Fonts' classic icons are U+E000-F8FF, inside 16 bits.
- **Not in scope**: the console.device replacement (device/) links the same renderer but
  never names a fallback font (it has no profile), so it keeps bitmap glyphs; double-width / double-height DEC rows keep '?'; the cursor cell over an outline
  glyph shows the inverted cell without the glyph (drawn again when the cursor leaves).

## Checklist

- [x] F1.1 render/otag.[ch]: the .otag tag list read portably (big-endian, indirect values
      made pointers, OT_Engine found); tests/test_otag.c.
- [x] F1.2 render/glyphmap: `vt_glyph_native(cp, enc)` -- 1 when the bitmap font or the
      drawn glyphs show cp as itself; tests in tests/test_glyph.c.
- [x] F1.3 render/outline.[ch]: the worker, the engine, calibration, the missing test, the
      cache, open / glyph / close.
- [x] F1.4 renderer: draw_rows draws outline glyphs (single and double width, bold, lines),
      the right half of a wide outline glyph is not erased, partial damage starting on a right
      half widens left.
- [x] F1.5 vtwin + handler: `font_fallback` from the profile (apply_profile, profile switch,
      L1 live change), the outline font opened with the window and on a font change.
- [x] F1.6 Prefs: the Fallback font field (prefs_core + upprefs), host test.
- [x] F1.7 rig check tools/rig/outline_rig.py: ttf.library 0.8.5.020 + Symbols Nerd Font Mono
      installed on the rig (ttfinstall), a line of Nerd icons drawn as glyphs (not '?'), a
      code the font lacks still '?', no outline font set: unchanged; the time of the first
      screen of icons.
- [x] F1.8 ledger + RULES/README notes (where to get ttf.library and a Nerd font).

## Result (2026-10-03)

Owner on the rig: "pass".

8 of 8. Host: otag 11 checks, glyph 295, prefs 83 (font_fallback survives a Prefs save: the
stage rewrites a profile from its fields, so a key without a field was dropped -- the test
fails with the field's write taken out). Rig (3.1, FS-UAE, ttf.library 0.8.5.020, Symbols
Nerd Font Mono installed by ttfinstall into RAM:, LIBS:/FONTS: assigned ADD):
tools/rig/outline_rig.py 4 of 4 -- the eight icons drawn from the font and nothing else on
their line changed; CJK (not in that font) keeps '?', the em dash its '-'.

At topaz 8 the icons are about 6 pixels tall (an 8x8 cell); a bigger font gives them room.
Not covered by the rig check: a double-width outline glyph (the Nerd Mono font has none;
CJK needs a font that has it), the cursor over an outline glyph, bold. Not measured: the
time per glyph on a 68020 (the line was drawn inside the check's 6 s wait; the A1200 run is
the measurement).
