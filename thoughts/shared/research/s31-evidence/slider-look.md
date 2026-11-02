# RL6 FLAT-SLIDER-LOOK: rounded scroller knob and arrows (s31)

Verdict: host test PASS and fails on revert; `make amiga amarchy-menu` rc 0; rig pixel proof NOT obtained (see below). Not done.

## Code
- amarchy/flat_core.c/.h: `flat_scroller_knob` (knob layout, extracted from `flat_scroller_pen`, behaviour unchanged).
- amarchy/flat_glyph.c/.h: `flat_scroller_cuts` returns the corner pixels (absolute coordinates) when the fill rect equals the knob rect exactly, reusing `flat_button_corners` (1 px cut, 12 pixels for boxes 12 px or taller).
- amarchy/flat.c: `flat_rf_c` paints each cut pixel with the track pen (`pen_back`) after the knob RectFill.

## Host test
`make test ONLY=flat_core`: with fix `[OK] flat_core: 1177 checks, 0 failed`. With `flat_scroller_cuts` forced to return 0:
`FAIL tests/test_flat_core.c:969: n = 0, want 12`, `FAIL ...:972: hit = 0, want 1`, `[FAIL] flat_core: 1177 checks, 2 failed`.

## Arrows (a)
Code reading only: `flat_glyph_window_gadget` includes FLAT_W_LEFT/UP/RIGHT/DOWN and `flat_clone_route` routes IM_DRAW for them. Not verified on screen.

## Rig 67 (OS 3.2, FS-UAE), flat look on via put_flat_prefs(True) + pick(), `WINDOW SYS:C OPEN`
Flat state: rectfill installed, hits=164, shits=228, clone draws=168 fallback=0, pens-held knob=245.
Shot: ~/Desktop/upterm-shots/s31-slider-flat.png (640x256).
Measured window x50-449 y50-149 (border 8d8d8d), vertical scroller column x436-448:
- track pen 000000, knob rectangle x436-445 y62-116 filled 993366 (not 494949), every corner pixel of the knob is knob colour, NO corner cut.
- No arrow gadgets visible in that column (rows 117-144 all track colour).
No pixel in the screen is 494949. So the measured knob was not drawn through the flat knob path (colour and square corners both differ), or the measured window is not the SYS:C drawer. Cause not diagnosed; window identity not confirmed (no window list taken). Rig stopped and cleared.

## Rig 72 diagnosis (OS 3.2, FS-UAE, 2026-11-02, worktree agent-ae4afa3fa22cbd372 at 6f44ba14)

Setup: `amarchy_gui_rig.py --setup --no-build` (build Amarchy from VTCX:Apps/bin), then `put_flat_prefs(True)` + `pick()`,
WB ARexx `WINDOW 'SYS:C' OPEN; CHANGEWINDOW 'SYS:C' 50 50 400 100`.

- (d) stale binary: ruled out. VTC:amarchy-flat md5 c05cf0ae47d22be40d1a217e008f642c = build/amiga/amarchy-flat.
- (a) not a Workbench drawer: ruled out. UITREE: `W 0 50 50 400x100 active "C"`, `G 2 436 62 10x55 prop` (the vertical
  scroller container), `G 3 432 118 18x11 button`, `G 4 432 129 18x11 button` (the up/down arrows), size/zoom/depth/close.
  The arrow gadgets exist; their pixels are all 000000 (rows 117-139: `000000*16 8d8d8d*2` over x432-449): the arrows are
  drawn, but glyph and background come out the same colour (separate defect, not this question).
- (c) wrong pen: not supported. STATUS `pens text=2 line=3 back=0 outline=3 button=4`, `knob=5`. The pick on this rig gave
  a non-vantablack theme; the knob measured 222222, solid, every knob pixel the same (rows 62-116 `000000*4 222222*10
  000000*2 8d8d8d*2`), track solid 000000 (pen_back, no stock dither). The knob colour follows the picked theme, so rig 67's
  993366 with "no 494949 on screen" most likely means rig 67's pick was not vantablack either (494949 is only vantablack's
  mix). Not measured: pen 5's RGB, to tie 222222 to pen_knob directly.
- Corners: square again (x436 and x445 on rows 62 and 116 are 222222). STATUS rectfill shits moved 216 -> 288 across the
  drawer open, so flat_rf_c's scroller path ran for this machine's scrollers, and the knob is solid knob-coloured (the
  stock knob is a framed box): the knob fills reach scroller_pen.

Cause (UNPROVEN, best fit to the measurements): flat_scroller_cuts only cuts when one RectFill equals the knob rectangle
exactly (`x0 != kx || y0 != ky || x1 != kx+kw-1 || y1 != ky+kh-1` -> 0). 3.2 Intuition paints the PROPNEWLOOK knob as
several fills (frame strips + interior), each inside the knob, so each takes pen_knob (uniform colour, measured) but none
equals the whole knob rectangle, and no corner is ever cut. A second possible mismatch: flat_scroller_knob's pixel layout
(truncating body*ch/0xFFFF) can differ by a pixel from Intuition's own knob size/position, which also defeats exact
equality.

Proof still needed: a ring of the last 16 scroller fills (rect, knob rect from flat_scroller_knob, ncuts) in flatshared,
printed by `amarchy-flat STATUS`; one drawer open shows the fill list.

Proposed fix (after the ring confirms): flat_scroller_cuts returns every knob corner cut pixel that lies inside the
current fill (fill inside the knob is already guaranteed by flat_scroller_pen), instead of requiring the fill to be the
whole knob; each frame strip then recuts the corners it covers, whatever order Intuition paints in. Host test: in
tests/test_flat_core.c next to the existing cuts block, a top strip (10,20)-(25,20) and a left strip (10,20)-(10,69) of
the 16x50 knob return the corner pixels they contain (non-zero, includes (10,20)); the interior (11,21)-(24,68) returns 0.
If the ring shows a layout offset instead, flat_scroller_knob must copy Intuition's knob arithmetic.
