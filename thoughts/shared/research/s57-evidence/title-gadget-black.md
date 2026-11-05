---
date: 2026-11-05
topic: S57-LOCALE-GADGET-BLACK / S57-GADGET-LINE-MISSING / S57-TITLE-BLACK-BOX on rig 143
status: not reproduced, cause not found (budget of about 40 tool calls spent on rig setup)
---
# Title bar gadget black, line missing, title box (rig 143, main 53d89817)

## Runs
1. RTG default boot (800x600x24, Workbench). amarchy-flat from build/amiga started by hand (STATUS flat=on),
   palette-flat-pen8.prefs USE, SYS:Prefs/Locale. Locale at x0 y14 552x285, zoom gadget x506..529, depth x529..552.
   - Inactive and active: both gadgets draw glyph lines (13a907 on the band), bottom border line y27 runs straight
     across x496..552 in both states, title text drawn. Active band 6688bb, inactive 55dd55.
   - So no hole, no stepped line, no black title box at depth 24. NOT reproduced.
2. Required mode (640x256x8) via `rig.py aga256` (S 640x256 depth=8). Setup repeated; the rig then went down
   (rig.py status "down") before Locale could be read. The first RTG boot also hit a CPU exception 3 (address error,
   10031f6a) right after `Palette FROM ... USE` + the flat start; not analysed. Run 2 gave no pixels.

## Code read (no change made)
- Title text: flat_tx_c (amarchy/flat.c:~833) fills band rect bx0..bx1 x 0..BorderTop-1 with the frame pen under every
  matching Text() call. bx0..bx1 come from flat_title_span (flat_core.c:698) from the window's SYS gadgets (first 8 only).
  Candidate: a gadget it does not list (the OS 3.2 iconify/zoom gadget when not GTYP_SYSGADGET, or gadgets beyond 8)
  leaves the span too wide, so the band fill repaints the gadget; a repaint of the title after SetWindowTitles (XCON
  after UPDemo 15) would then show exactly "gadget lost, title blank". Unverified.
- Gadget box: c2_get_pens (flat.c:317) uses the band path only when flat_glyph_on_band(which) is true
  (close, depth, zoom, iconify); a gadget with an unresolved which falls to the BACKGROUNDPEN frame, which is the window
  bg (26,27,37) with no glyph: matches the reported solid block. Unverified.

## Next
Boot rig on aga256 FIRST (rig.py start --max, then aga256), wait for the reboot, start amarchy-flat, then Palette USE
(watch for the exception 3 above), Locale, grid the zoom gadget at depth 8. Then run SetWindowTitles on a shell window
with a counter on flat_tx_c. Frame the regression test in tests/test_flat_core.c on flat_title_span with a gadget
list that includes the OS 3.2 zoom/iconify case.
Scratch grid script: scratchpad/grid.py (not kept). Rig 143 stopped and cleared; shots not copied (none useful).
