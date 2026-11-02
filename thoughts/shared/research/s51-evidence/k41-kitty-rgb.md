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
