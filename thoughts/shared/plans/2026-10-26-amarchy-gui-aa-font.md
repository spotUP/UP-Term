---
date: 2026-10-26
topic: Antialiased, user-chosen font in the drawn Amarchy menu (amarchy-gui)
status: plan, not started
ledger: thoughts/spot/todos.md row AM-GUI-AA-FONT
---

# Antialiased, user-chosen font in the drawn Amarchy menu

## Goal

Owner wish (2026-10-26): the drawn menu (amarchy-gui) lets the user pick its font, and its text is
antialiased so it looks modern, like Omarchy. Reuse UP-Term's antialias engine; no second rasterizer.

## Owner decisions

- D1 TrueType and Amiga bitmap fonts are both offered. TrueType text is drawn antialiased through the
  terminal's engine; bitmap fonts are drawn as today (menu_font.c, Text()).
- D2 Order: after AM-GTHEME-SAVE-A1200, before prefs-inline M4.

## What exists (origin/main 113ad019)

- Font choice: amconf key AM_FONT "name:size", read in amarchy/gui.c pick_font (~1077-1088) through
  prefs_font_parse, opened by menu_font_open (amarchy/menu_font.c, OpenDiskFont, bitmap only).
- Engine: render/ttf.h (ttf_open, ttf_glyph_index, ttf_advance, ttf_render coverage, ttf_fit/ttf_place),
  render/outline.h (vo_font: cell-based glyph masks + vo_coverage), render/amiga_render.c antialiased draw
  (aa_tab = fg over bg per coverage via ce_mix, aa_shade[3] inks for 1/4 1/2 3/4, aa_use, n_aa sentinel).
- Menu text sites: gui.c draws labels with Move/Text after SetFont (~195, ~223); measure and ellipsis use
  the TextFont (grep TextLength / mg_font in gui.c).

## Design

- Proportional text: vo_font is monospaced cells, menu labels are proportional. The menu uses ttf_* directly
  for layout (ttf_advance scaled to the pixel size) and coverage (ttf_render per glyph, cached per code point
  for the open size).
- One blend path: the coverage-to-ink code (aa_tab build, shade inks and their pens, the masked blit of one
  coverage pass) moves out of amiga_render.c into a shared render module (name it after what it does, for
  example render/aa_ink.c) that amiga_render and amarchy-gui both call. amiga_render's behaviour and its
  tests stay byte-identical (n_aa sentinel unchanged). No copy of that code in amarchy/.
- Menu text API in amarchy (pure layout host-tested): mt_open(spec) picks TrueType when the name resolves to
  a .ttf in the font drawers the terminal searches (reuse that search, grep the terminal's font lookup), else
  bitmap; mt_width(text), mt_draw(rp, x, y, text, fg, bg). Every gui.c text draw and measure goes through it,
  so ellipsis/truncation uses the same metrics as drawing.
- Background: rows draw on a known pen (theme background, selection colour); fg/bg pairs per row state give
  the aa_tab key. Colour chips and icons are not text and are untouched.
- Font picker: the Amarchy font row (SD_AMARCHY AM_FONT) opens the ASL font requester, plus TrueType names;
  this is prefs-inline M6. Until M6 lands, the font is set in Prefs > Amarchy (already exists) and the menu
  must honour a TrueType name there.

## Checklist

- [x] A1 Move the blend path to a shared render module; amiga_render calls it; terminal tests unchanged. -- DONE 76970477 on am-aa-font (render 525 ok, pens 24 ok incl. aa_ink_rules; revert proof d_pen<=d_fg fails test_pens.c:107; make amiga rc 0)
- [x] A2 amarchy menu text module (TrueType or bitmap), pure layout tests: width, ellipsis, glyph cache. DONE 095031a4 (am-aa-font): menu_text: 30 checks, 0 failed; revert-proof: cache, fit and width breaks each fail; make build/amiga/amarchy-gui and vtcon-handler rc 0; ttf 375 ok.
- [x] A3 gui.c draws and measures every label through it; sentinel: count of antialiased glyphs drawn.
- [x] A4 Prefs > Amarchy font accepts a TrueType font; the menu reloads it on the amconf notify.
- [x] A5 Rig 7 check with icons on (also covers the icon path missed before): screenshot via haiku, sentinel > 0.
- [x] A6 A1200 deploy with backups, drift 0, owner test steps.

## Open for the implementing agent to measure first

- How amiga_render gets pens for the shade inks on a pen-mapped screen versus direct RGB (RTG), and whether
  amarchy-gui's screen (Workbench or its own) gives the same; report before A1.
- Speed of first draw on the 060 and 68020 with a glyph cache per size.
