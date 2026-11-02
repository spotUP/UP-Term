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
