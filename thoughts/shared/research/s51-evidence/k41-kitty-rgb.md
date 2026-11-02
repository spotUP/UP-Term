---
title: K41 kitty RGB rig proof (partial)
date: 2026-11-02
status: case 1 PASS 4/4 runs, case 2 FAIL, K41 not ticked
---
Rig 91, OS 3.2, `rig.py rtg`, shot 800x600 (24-bit RTG), build 534bef43 (= 570b18c8 plus 6 origin/main commits;
rig.py refused 570b18c8 with RIG-STALE-BASE, so s51-k41 was fast-forwarded to origin/main). rig.py takes
build/amiga from its own checkout (ROOT = worktree), so the rig runs the worktree build. The rig starts on
aga256 under --os 32 (the screen was not RTG until `rig.py rtg`).

Script: tools/rig/kitty_rgb_rig.py (raw f=24 100x80 picture, 8000 distinct colours, 1:1; two 40x32 f=32 pictures at
alpha 128 red). The plan names f=32 demo.png from VTC:; none exists in the tree, so the picture is generated.

| run | case 1 distinct shown | case 1 exact pixels | case 2 (blue cells, z=1 C=1) | case 3 (default bg) |
|---|---|---|---|---|
| 1 | 8000/8000 (100%) | 8000/8000 (100%) | FAIL: expected (128,0,119) got (128,0,0) on 1280/1280 | PASS (128,0,0) 1280/1280 |
| 2 | same | same | same | same |
| 3 | same | same | same | same |
| 4 (after z=1 C=1 change) | same | same | same | same |

Case 1 (true colour) PASS: 100% distinct and exact, far above the 255 the index path could show.
Case 3 PASS: the soft blend over black gives (128,0,0), matches the formula.
Case 2 FAIL: the blend ignores the blue cell background (0,0,238): the result is as if ReadPixelArray returned
black. The host fake returns canvas pixels (k40_soft_blend_reads_the_text_under_a_front_picture), so the real
P96 read path is the suspect (r->rp may be the back bitmap, img_run_rgb in render/amiga_render.c). Not diagnosed
further; cases 2 and 3 do not both pass only because case 3's base is black, which hides a read that returns zeros.
Shots: ~/Desktop/upterm-shots/k41-run1..3.png (runs 2-4 of the table; the rig rebooted once between the first and the second batch) and k41-run4-final-script.png.
AGA pixel-equal comparison: open (needs an aga256 rig and the a6de061d reference
truecolor-aga-ref.png, which does not exist in the tree).

## K40c fix (diagnosis, 2026-11-02, rig 91, OS 3.2 RTG 800x600x24, build c7ab3fc6 + script change)

Result: the soft blend and the P96 read path are correct. Case 2 fails because its expectation contradicts the
agreed cell model, not because of a render bug. No code change made; the expectation is an owner decision.

Cause: engine/vtengine.c sixel_place_row (line ~5332, `b.fg = b.bg = VT_COLOR_DEFAULT`, `b.pad = VT_CELL_IMAGE`)
overwrites every cell under a z >= 0 placement (sixel and kitty) with a blank in the default colours. That is the
documented model: plan 2026-10-19-kitty-graphics.md K30 ("z >= 0 keeps today's model") and D12 (text written over
a z >= 0 image replaces it, same as sixel). So under the z=1 picture of case 2 the bitmap holds black, not the blue
SGR 44 background, and the blend gives (255*128 + 0*127 + 127)/255 = 128, blue 0 = the measured (128, 0, 0).
Pixel row y=41 of the probe shot: blue x 460-476, picture (128,0,0) x 476-516, blue x 516-548: the cells under the
picture are not blue.

Proof that the read path works: new case 4 = the same 50% red picture at z=-1, C=1 over blue cells (z<0 keeps the
cells, VT_CELL_UNDER). Measured (128, 0, 119) on 1280 of 1280 pixels, expected (128, 0, 119) from bg (0, 0, 238).
That pixel can only come from ReadPixelArray returning the blue cell background on the real P96 driver.
(The first probe placed case 4 at column 90; the window is 94 columns wide, so the bg sample hit the border.
Moved to column 40.)

Runs (tools/rig/kitty_rgb_rig.py, 3 runs): case 1 PASS x3, case 2 FAIL x3 (128,0,0), case 3 PASS x3, case 4 PASS x3.
Shots: ~/Desktop/upterm-shots/k40c-run1..3.png, k40c-probe1.png (case 4 at column 90, clipped).

Open (owner): kitty's own spec keeps the text and cell background under a z >= 0 picture (translucent parts show
them). Matching that means a kitty z >= 0 placement sets VT_CELL_IMAGE without blanking the cells (sixel keeps
blanking), which changes D12/K30 and the host test k40_soft_blend_reads_the_text_under_a_front_picture
("A z>=0 picture owns its cells"). Until decided, case 2's expectation is wrong under the current model. K41 not ticked.
