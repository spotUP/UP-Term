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
AGA pixel-equal comparison: done, see "K41 AGA" below.

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

## K41 AGA (2026-11-02)
Rig 84, `rig.py start --max --os 32`, Workbench screen aga256: ENVARC:Sys/screenmode.prefs on the rig is byte-equal to
VTC:screenmode.aga256.prefs (640 wide, depth 8 inside the prefs), shot 640x256. Script: `kitty_rgb_rig.py aga save|compare`.
Stream: the generated 100x80 picture as raw f=32 (alpha 255), plus the two 40x32 alpha-128 red pictures (z=1 C=1 over
blue cells at column 50; plain at column 70); crop = XCON window (0,12,640,240), 153,600 pixels. The 780x500 RTG window
does not fit the 640x256 AGA screen, so the AGA geometry and columns differ from the RTG mode; both builds got the same stream.

Reference: worktree vtcon-s51-agaref detached at a6de061d, `make amiga` there, rig 84 started from that checkout (rig.py
reads build/amiga from its own checkout; it refused the old base with RIG-STALE-BASE, so `--allow-stale-base "<reason>"`
was used; no push of files by hand). Rig VTC:vtcon-handler md5 19480a8e... = the reference build's. Shot saved as
truecolor-aga-ref.png: 150 distinct colours.
Compare: build 23513ced (s51-k41aga, handler md5 ded538d7...), rig refreshed by the rig gate (LIBS reboot) on the first run.

| run | differing pixels vs reference | distinct colours | verdict |
|---|---|---|---|
| 1 | 0 of 153600 | 150 | PASS |
| 2 | 0 of 153600 | 150 | PASS |
| 3 | 0 of 153600 | 150 | PASS |

Run 3 first failed with connection refused (wasabid down: the rig was resetting after run 2); after the rig answered again
the same command passed. A first reference attempt ran the script from the s51-k41aga checkout, whose gate pushed the main
build onto the rig; it was discarded, the reference was re-taken after the gate put the a6de061d build back (handler md5
checked before and after).
Shots: ~/Desktop/upterm-shots/k41-aga-ref.png, k41-aga-new1..3.png. K41 stays open: the case 2 blend FAIL (RTG half) is not fixed.

Note for K40d (owner decision O6, kitty z>=0 keeps the cells): the AGA stream's z=1 picture at column 50 sits on blue
cells, so after K40d the pixels of that picture change from the reference (blue shows through). The AGA compare after
K40d must differ only inside that picture's rectangle; everything else stays pixel-equal.

## K40d (2026-11-02, branch s51-k40d, rig 91, OS 3.2 RTG 800x600x24)

Owner decision O6: a kitty z >= 0 placement keeps the text and cell background under it, as real kitty does.
Fix in engine/vtengine.c sixel_place_row: a kitty picture (kind 1) with z >= 0 now only ORs VT_CELL_IMAGE into
the cells (glyph, fg, bg, attrs and any VT_CELL_UNDER stay); a sixel still blanks them to the default colours.
Text written later took the cell when this was written (D12); reversed 2026-11-03: a kitty z >= 0 cell keeps VT_CELL_IMAGE. No render change: draw_rows
already draws every cell's background and glyph, and draw_images then blends the picture over them (K40b soft blend,
ReadPixelArray). Other VT_CELL_IMAGE readers checked: reflow copies the cell whole and carries the placement
(place_carry); scrollback packing stores pad with the cell; row_no_glyphs / cell_plain_blank already treat a pad
cell as written; erase and ED/EL rewrite the cell as before; selection reads ch (now the real text under a kitty
picture, as in kitty).

Host tests: tests/test_kitty.c k40d_kitty_front_picture_keeps_the_cells (replaces k30_z_0_and_up_still_takes_the_cells:
kitty z=5 over red-on-blue "XY" keeps X/Y, fg 1, bg 4, image bit; a later "q" takes its cell; a sixel over the
same text blanks), tests/test_render.c k40_soft_blend_reads_the_text_under_a_front_picture rewritten (the X under
a z=1 picture is drawn before it; column 0 row 12 is the blend over an RGB blue cell: (0,48,24) over (0,0,238)).
Before the fix (cbd9e471 engine): kitty 4 failed, render 2 failed (pixel blend read black, 0x0C blue instead of
0x83). After: kitty 434/0, sixel 451/0, reflow 147/0, xterm 470/0, render 702/0.

Rig runs (tools/rig/kitty_rgb_rig.py; rig 91 vtc/vtcon-handler md5 60938589 = worktree build; case 2 expectation
comes from the blue cell beside the picture, so the script needed no change, only its docstring):

| run | case 1 (exact px) | case 2 blue cells z=1 | case 3 default bg | case 4 z=-1 blue | result |
|-----|-------------------|-----------------------|-------------------|------------------|--------|
| 1 | 8000/8000 PASS | (128,0,119) 1280/1280 PASS | (128,0,0) 1280/1280 PASS | (128,0,119) 1280/1280 PASS | PASS |
| 2 | 8000/8000 PASS | (128,0,119) 1280/1280 PASS | (128,0,0) 1280/1280 PASS | (128,0,119) 1280/1280 PASS | PASS |
| 3 | 8000/8000 PASS | (128,0,119) 1280/1280 PASS | (128,0,0) 1280/1280 PASS | (128,0,119) 1280/1280 PASS | PASS |

Before K40d (k40c runs): case 2 measured (128,0,0) x3. Shots: ~/Desktop/upterm-shots/k40d-run1..3.png.
AGA: not rerun here. The K41 AGA compare against the pre-K38 reference will now differ only inside the z=1 picture
rectangle over the blue cells (the picture blends with the blue instead of a blanked cell), as noted above.

## K41 ICAT f=100 (s51-icat, rig 91, 800x600x24 RTG, 2 Nov)
`UPTERM_RIG=91 python3 tools/rig/kitty_rgb_rig.py icat <prefix>`: writes the case-1 picture (100x80, 8000 distinct colours) as PNG to VTC:kr-icat.png,
runs `VTC:icat VTC:kr-icat.png` in an XCON: window (icat md5 1c34ca79, equal to build/amiga/icat; PNG goes as kitty f=100, not scaled: smaller than the window),
measures with the same function as case 1 (measure_pic1). Thresholds: >= 99% exact pixels, >= 90% distinct source colours.
| run | origin | distinct shown | exact pixels | verdict |
|---|---|---|---|---|
| 1 | (4,39) | 8000/8000 (100%) | 8000/8000 (100%) | PASS |
| 2 | (4,39) | 8000/8000 (100%) | 8000/8000 (100%) | PASS |
| 3 | (4,39) | 8000/8000 (100%) | 8000/8000 (100%) | PASS |
Shots: ~/Desktop/upterm-shots/k41-icat-run1..3.png.
Default mode rerun (cases 1-4, same rig, handler md5 19a477ed): case 1 PASS, case 3 PASS, case 4 PASS (128,0,119), case 2 FAIL measured (128,0,0) over blue cells,
expected (128,0,119). Not caused by this change (script change only touches case 1 measurement); the rig's handler build (23:16) likely predates K40d (85c5c1f7, 23:19): rebuild and remount before judging case 2.

## K41 rerun with the K40d handler

Rig 91 rebooted via `rig.py install` + `rig.py reboot`; handler md5 on the rig (build/rig91/vtc/vtcon-handler) d2dee4b626b8c0eb6b044f17f881fca6 (main 1ed14b5f build). Default mode, 3 runs, all PASS (shot 800x600):

- case 1 origin (12, 39): 8000/8000 exact pixels (100.00%) PASS
- case 2 (on blue cells): bg (0, 0, 238) expected (128, 0, 119) measured (128, 0, 119) on 1280 of 1280 pixels PASS
- case 3 (default background): expected (128, 0, 0) measured (128, 0, 0) on 1280 of 1280 PASS
- case 4 (z=-1, blue cells): expected (128, 0, 119) measured (128, 0, 119) on 1280 of 1280 PASS

Identical lines in runs 1, 2, 3. Shots: ~/Desktop/upterm-shots/k41-default-run1..3.png.
