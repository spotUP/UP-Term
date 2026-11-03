# Rig 35 re-proof (main 525bc62f, 2026-11-03)

Rig 35 booted fresh, flags --max --os 32 --idle-minutes 0, purpose "vtcon-16 Amarchy session: owner eye-check rig 35".

## md5 (build/amiga = build/rig35/vtc, identical)
- Amarchy 000de69d955ecf8c73aed03491fbf403
- amarchy-flat 04c5023d03100492e585a96813c826df
- amarchy-gui f56a9a7693c0994342d9568ef0757ec2
- amarchy-menu 3dd1ea67ce2e713c9e0c4e608306d551

rig.py install failed (rebuilds unrelated stale programs, pi-llmprobe undefined _strtod); binaries copied straight into build/rig35/vtc.

## Proof a: inactive dragbar colour - FAIL
Inactive ShellA bar rows y=40..49, x=30..330: (50,63,95) = #323F5F, 2871 px. Pen 251 palette entry = 323F5F (screen and palette agree).
Expected #5A5E75 (gt_mix(1a1b26, a9b1d6, 45)). No 5A5E75 in the 256-entry palette. Fresh boot with HEAD build gives the same, so not stale pens. Cause open.
Pens: dim=249, knob=250, inbar=251 (same trio as pens 36-38).
Shot: ~/Desktop/upterm-shots/rig35-reproof-inactive.png

## Proof b: disabled menu text - INCONCLUSIVE
Workbench menu bar opened at x=30/80/170/230, pointer y=6/7. Shots t170 and t230 are pixel-identical: the dropdown with disabled items was never captured open. Only menu-bar titles seen (#A9B1D6 text, #747A94 outline, dim #858BAA 20 px), all x 23..75. Disabled-item rows not located, so no verdict on 0x4444/0x1111 holes.
Shots: rig35-reproof-menu.png, rig35-reproof-wbmenu.png (Desktop/upterm-shots).
Next: hold right button inside a title, screenshot while held, measure the disabled item rows.

## Round 2 (2026-11-03)

### Q1 inactive bar colour - cause narrowed; refresh fixes the state
- Config: ENV:Amarchy/prefs has no theme key -> GT_DEFAULT tokyo-night (gthemes_core.h:18); tokyo-night.theme has no `inactive` -> inbar_rgb = gt_mix(1a1b26,a9b1d6,45) = 5A5E75 (computed). Running amarchy-flat md5 04c5023d (525bc62f build).
- 323F5F is not a derivation of inbar: bg->fg per-channel ratio is 0.17/0.24/0.32 (non-uniform); no gt_mix of bg with fg/dim/muted/accent gives it. It equals icon slot 4 (ACCENT33) of the amarchy-icons colour set; pens 249-251 = 858BAA/747A94/323F5F are the icon slots 6/5/4 (pens 36-38 colours).
- Experiment: `Break 6 D` (CTRL_D to amarchy-flat = dim_refresh). STATUS pens 249/250/251 -> dim=242 knob=245 inbar=243. Two probe shells opened (ProbeA inactive): dragbar rows = 5A5E75, 3480 px (was 323F5F). So the colour maths and ObtainBestPen path (flat_core.c:713-717, fp_obtain flat.c:1280) are right; the failure is amarchy-flat drawing with pen numbers 249-251 that the icon client re-owns.
- Open: why flat_pens_follow did not re-obtain after the boot-time Workbench reopen (identity/epoch not seen as changed). bd1c1ddc touches only the epoch bump; relevance unverified.
- Shot: ~/Desktop/upterm-shots/rig35-r2-inactive-after-refresh.png

### Q2 disabled menu text - INCONCLUSIVE
- Right button hold works via ami._mouse(c,2,1) (button 1 = right; grab y = 2 x screen row): pointer (100,6), moves to (135,40) while held; a dropdown appears. Released after.
- Dropdown x 98..260, y 10..120: only 1A1B26 and A9B1D6, zero 747A94/858BAA/3E4254, no alternating-pixel dither. Which menu it is and which rows are disabled were not mapped, so no verdict. If it is Window (New drawer / Open parent / Close disabled), disabled rows are drawn in full A9B1D6 = not dim.
- Shot: ~/Desktop/upterm-shots/rig35-r2-menu-held.png. Next: hold at a known title x, map rows from `ami.py menus` offsets.

## Knob state (1206a2c3)
Built amarchy-flat 1391828a, Amarchy 4d98af93, amarchy-gui 9302dde5, amarchy-menu 7e84b491 (build/amiga); copied via ami put + Copy to UP-Term:bin; amarchy-flat stopped (Break 6 C) and restarted (Run amarchy-flat BOOT). STATUS pens-held dim=243 knob=245 inbar=244 (fresh start, no stale pens; inactive bar needed no forced refresh). Amarchy/amarchy-gui/amarchy-menu running processes not restarted (old code in memory).
Windows: "Eye check shell" (knob area x535-552 y72-120) and "UP-Term" (x622-640 y23-140). Pixel counts by PIL, activation by clicking the other window's drag bar, no other input.
- rig35-knob-active.png (shell active, UP-Term inactive): shell knob 7AA2F7 (48 px), UP-Term knob 5A5E75 (234 px), track 1A1B26.
- rig35-knob-inactive.png (UP-Term activated): shell knob 5A5E75 (48 px), UP-Term knob 7AA2F7 (234 px).
- rig35-knob-reactive.png (shell activated again): shell 7AA2F7, UP-Term 5A5E75.
Verdict: PASS. Colour flips on activation with no scroll/resize (repaint on activate works). Knob distinct from track in all states.

## Arrow outline (fb6aa600)
Setup: binaries from fb6aa600 (amarchy-flat md5 8a574ff2d47074ec31adc473fbc55c2b; VTC: copies md5-equal to build/amiga), copied to UP-Term:bin with CLONE, rig 35 rebooted via rig.py reboot, amarchy-flat autostarted (Status process 6, no NOAUTOSTART). Window: Workbench drawer "Empty-12MB-FFS" (SYS: root, opened by ARexx WINDOW OPEN) at x=28 y=39 462x103 on the 640x256 PAL screen; ring = columns 488..489 (W-2..W-1), rows 39..141 (206 px). Up arrow rows ~40-49, down arrow rows ~110-121, size gadget rows 132-141.
- PASS active, before clicks: ring 206/206 = 7AA2F7, 0 other.
- PASS active, after 5 clicks up arrow (481,45) + 5 clicks down arrow (481,115): ring 206/206 = 7AA2F7; up-arrow rows 40-50 22/22; down-arrow rows 108-121 28/28; knob moved (clicks registered). Size gadget rows inside the 206.
- PASS knob active: column 480 knob run = 7AA2F7.
- PASS inactive (click backdrop 600,150; uitree shows window without active flag): ring 206/206 = 5A5E75, 0 other. Knob col 480: 37 px 5A5E75, 19 px 1A1B26 (track), no 7AA2F7. Size gadget area: 62 px 5A5E75, 118 px 1A1B26, no 7AA2F7.
- Not done: arrow clicks while inactive (a click activates the window first).
Shots: ~/Desktop/upterm-shots/rig35-arrow-before.png, rig35-arrow-after-clicks-active.png, rig35-arrow-inactive.png. Rig 35 left UP, window re-activated.

## Arrow knob colour (33d894d8)

Result: NOT MEASURED (blocked). Active, pressed and inactive items are all unproven.

- Opened an XCON window with `ami.py open` (window "UP-Term", tree: W 0 0,11 640x235; scroller gadgets 23312 at 626,23 10x190; arrows 23313 at 622,214 18x11 and 23314 at 622,225 18x11; sys:size 622,236 18x10).
- The front-screen capture (`ami.py shot`, 640x256) does not show that window. Rows 11-19 hold the Amarchy desktop title text (x 31-84), not the UP-Term title bar. Regions 622-640 x 214-246 are 100% 1A1B26 (198/198, 198/198, 180/180 px): no frame, no arrow glyph, no triangle drawn there. Likely the XCON window is covered by another window (tree lists W 2 "" 0,11 640x235 and the Amarchy desktop); sys:depth / drag clicks on UP-Term did not change the capture.
- Not measured: active glyphs 7AA2F7 (0 px of 7AA2F7 in the arrow regions), pressed fill (no press-hold command in ami.py), inactive 5A5E75.
- Needs: a window with scrollbars visibly in front on rig 35 (e.g. close/move the covering Amarchy window, or an Amarchy-owned scrolling window), then re-measure.
- Shots: ~/Desktop/upterm-shots/arrow-knob-active.png, arrow-knob-rig35-front-screen.png
