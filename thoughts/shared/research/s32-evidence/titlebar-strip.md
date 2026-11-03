# Title bar strip, rig 35 (wasabi port 1268), measured 2026-11-03

Grab = 640x512 (rows doubled: 1 Amiga row = 2 image rows; x not doubled). Active bar colour measured 7aa2f7 (not 82a1f1), background 1a1b26.

Window C (active, front): wasabi raw: Amiga left 91 top 123 w400 h100; borders (L,T,R,B) = 4,11,18,10.
Gadgets (image px, y doubled): depth x468 y246 w24 h22 (= Amiga 24 x 11 rows), zoom x445 w24 h22, close x91 w20 h22, drag x91 w400 h20 (10 rows), size 473,426 18x20.

Rightmost runs, C bar (image coords; x 450..499):
- rows 246..265 (Amiga rows 0..9): 7aa2f7 from the left up to x490, 1a1b26 from 491 (outside window); gadget glyphs 450-482 as 1a1b26 runs.
- rows 266..267 (Amiga row 10): x400..473 = 1a1b26 ; x474..490 = 7aa2f7 (THE STRIP, 17 px wide, 1 Amiga row = 2 image rows); 491+ outside.
- rows 268+: x489..490 = 7aa2f7 (right border, 2 px), interior 1a1b26.
So bar bottom: left part ends Amiga row 9 (image 265); right part ends Amiga row 10 (image 267).
Strip rect, window coords (Amiga px): x 383..399 (w17), y 10 (h1). Right edge = window right edge (x490 abs). Left edge = depth gadget left + 6 (474 vs 468).

Matching gadget: depth (x468..491, rows 0..10, h11). Its height 11 exceeds the 10-row bar fill, so row 10 is the strip; zoom (x445..468, also h11) shows no strip at row 10 (1a1b26). Strip = depth box minus its left 6 px, row 10 only.

Inactive Shell (300,60 raw; 300x120): no strip visible. Inactive bar is painted 1a1b26 (same as background), only 7aa2f7 outline at top row 120 and right border x598..599; bar interior rows 122..137 constant 1a1b26 at x540..597. Gadgets: depth 577,120 24x22; zoom 554,120 24x22. Strip would be background coloured, so invisible when inactive.

Flat OFF comparison: not done. amarchy-flat usage is only "[STATUS]; Ctrl-C unloads it" (no OFF/ON toggle); unloading would disturb the held eye-check rig. STATUS: flat=on calls=1058, rectfill installed=yes.
Not verified: whether stock (flat off) shows the strip.
