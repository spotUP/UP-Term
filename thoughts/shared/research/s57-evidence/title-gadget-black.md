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

## Run 3 (depth 8, 640x256, 2026-11-05, rig 143, OS 3.2, 060 max, worktree build of main 7e07580f)
Rig started with `--max --os 32` (boots aga256 640x256 depth=8 by itself; uitree S line "640x256 depth=8"). VTC:amarchy-flat
started by hand, STATUS flat=on calls=51 after Locale opened. Palette USE NOT run (stock palette: band pen 6688bb).
Locale window x0 y11 552x192 active; zoom gadget x506..529, depth x529..553 (uitree).
- Zoom gadget (x511..523, y13..19): outlined box with inner square, band-coloured fill 6688bb, black glyph lines. NOT black.
- Depth gadget (x533..548, y13..19): two overlapping boxes drawn in aaaaaa/black glyph on the band. NOT black.
- Bottom line under them y21: black (000000) straight from x500 to x549, no step, no hole. Title-bar drag/depth top row y10 black.
- Flat buttons (Save x12 y181 88x14): left edge 2 px (ffffff x12..13 at y188), top edge 1 px (ffffff y181), right 2 px
  000000 (Cancel x538..539), bottom 1 px 000000 (y194). Matches the expected 2 px sides / 1 px top.
- Window sides at y100: left x0..1 = 6688bb (2 px), right x550..551 = 6688bb (2 px).
Verdict: at 640x256x8 the zoom gadget does NOT draw black. Title-gadget-black NOT reproduced at depth 8 either; no fill named,
no code change. Screenshot: ~/Desktop/upterm-shots/title-gadget-depth8-rig143.png. Caveat: with palette-flat-pen8 USE
(skipped, it crashes the rig) the pens differ; the two code leads (flat_title_span first-8 scan, c2_get_pens BACKGROUNDPEN
fallback) stay unproven. Reproduction may need a window with gadgets beyond the 8 scanned or a SetWindowTitles repaint.

## Run 4 (UPDemo 15 exit, depth 8, 640x256, 2026-11-05, rig 143, OS 3.2, 060 max, worktree build of 96ef3a03)
Rig: `--max --os 32 --allow-stale-base` (boots 640x256 depth=8). amarchy-flat started by hand (flat=on). UPDemo assets copied
to VTCX:Apps/share/updemo (rig had none). XCON shell NewShell XCON:0/11/640/240/UPDemoShell/CLOSE (a 600 px window fails:
"UPDemo: the window is 72x20, it needs 76x20"). `UPDemo 15` run, title became "UP-Term demo" (OSC 2), then `q`.
- Before demo: title "UPDemoShell" text aaaaaa on band 6688bb; close, zoom, depth glyphs aaaaaa on 6688bb.
- After exit: uitree window title is "" (empty string), flat calls 204, flat=on. Title strip = 6688bb only: BLANK TITLE reproduced.
- Gadgets after exit are pixel-identical to before: close x8..11 y14..18 aaaaaa, zoom x600..611 and depth x620..636 glyphs
  aaaaaa on 6688bb, bottom line y21 black. NO black block, close gadget NOT missing. Not reproduced at 640x256x8 stock palette.
- So: the blank title is the window title being "" after UPDemo's title pop (the title string, not a flat fill). Which code
  restores "" (handler push/pop 22;0t / 23;0t, UPDemo line 2741) is not yet traced; not tested with flat off (call budget).
  flat_title_span / c2_get_pens leads stay unproven for the black block; gadgets drawn correctly here.
Shot: ~/Desktop/upterm-shots/title-after-updemo15-rig143.png. Palette USE not run.
