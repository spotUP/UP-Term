# RL19: why the right corners of the flat button stayed square

Date 2026-11-05, worktree s57-rl19 (from main fdb5e99f).

## Cause

The product draws all four corners alike. The FAIL was a measurement artifact.

- `raa_frame_draw` (amarchy/roundaa_core.c) reads ONE backdrop pen per corner, from the pixel diagonally outside the corner
  (ox-1 / ox+w, oy-1 / oy+h), before it paints. It mixes the fill with that pen (re-draw safe: the box never paints there).
- btnbgprobe paints a 2x2 checker of pens 2 and 3. Under vantablack at depth 8 these are be and 8d; the button fill is 8d.
- Pixels from the rig 124 grab (~/Desktop/upterm-shots/rl19-new-button.ppm, button 60,70 80x14):
  - outside top-left (59,69) = be, outside bottom-left (59,84) = be: blend visible (b6, 99).
  - outside top-right (140,69) = 8d, outside bottom-right (140,84) = 8d = the fill. A correct blend is 8d over 8d = 8d.
  - The box does end at x=139 (row 70: 134..139 = 8d box, 140..141 = be backdrop). No wider box, no repaint.
- So the right corners can never show a step on that checker phase. Same reason the stock binary gave the same 4 of 12.

Violated invariant: the rig check assumed the pen the corner blend mixes with differs from the fill. Level: the rig
measurement (probe backdrop + script verdict), not the frame dispatcher or the core.

## Fix

- tests/amiga/btnbgprobe.c: `NOGAD|GAD SOLID` paints pen 2 everywhere instead of the checker.
- tools/rig/amarchy_roundaa_rig.py: uses SOLID by default (`--checker` keeps the old backdrop); prints the outside pixel per
  corner and reports a corner whose outside pixel has the fill colour as UNMEASURABLE, never as a miss.
- tests/test_flat_core.c test_round_aa: a 40x14 box on a plain backdrop blends all four corners alike (mirror checks on every
  4x4 corner pixel); on the rig's checker phase the right corners are fill over fill while the left ones blend.
  This pins the design; no product code changed, so there is no product revert for it to catch.

## Results

- `make test-one SUITE=flat_core`: 2523 checks, 0 failed.
- `make amarchy-flat amarchy-gui build/amiga/btnbgprobe`: rc 0.

## Rig 129 (3.2, --max, not proven)

- `rig.py aga256` + restart left the Workbench at depth 2 (wbpens DEPTH 2); `amarchy_gui_rig.py --setup` failed twice:
  "flat prefs off, amarchy-flat BOOT left nothing resident". Theme pick failed ("open menu: no stable state within 60s"),
  so the theme was NOT vantablack; fill read 7aa2f7.
- `amarchy_roundaa_rig.py build/amiga/amarchy-flat solid` (SOLID backdrop, depth 2): 0 of 12 intermediate, 0 unmeasurable.
  With the frame dispatcher status shown below, the number says nothing about the blend. The depth-8 four-corner proof
  and the GadTools prefs-window proof are still open.
- Next: a rig where setup passes (rig 124's method for depth 8), then this script without --checker; expect the right
  corners intermediate.
- Rig 129 stopped and cleared.
frame=hooked calls=0 flat=0
