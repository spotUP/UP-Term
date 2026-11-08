# Installer flat buttons on KS 3.2.3: rig 95 measure -> FAIL

HEAD e5d9473d (b7377f75, c4844c4e, 05678a64 in it). build/amiga/amarchy-flat 212284 bytes, cmp IDENTICAL to
build/rig95/vtc/amarchy-flat (the file the rig ran).

## Rig

Rig 95, `rig.py start --max --os 32 --purpose "Amarchy session: Installer flat buttons on KS 3.2"`. Note: `rig.py reboot
--purpose ...` rewrote the config with kick40068 (KS 3.1) and lost --os 32 (title lost "OS 32"); the rig was stopped
--keep and started again with --os 32. `ami.py info`: exec.library 47.13; `Version`: Kickstart 47.115, Workbench 47.2
(KS 3.2.3). `rig.py rtg` then gave 800x600 depth 24 (the OS 32 default is 640x256). No DOpus on this rig.
The 3.2 Workbench boots with stock pens: tokyo-night needs `Run >NIL: VTC:Amarchy` (then a relaunched Installer
shows 1a1b26 etc). amarchy-flat started by hand: `Run >NIL: VTC:amarchy-flat`.
Installer: `Run >NIL: Execute VTCX:dopus_run.script` worked (the DOpus Installer, first page). STATUS:
`flat=on calls=126`, `rectfill installed=yes calls=1037 hits=66 shits=48 mhits=0 bhits=8`, `frame=hooked calls=38 flat=0`.
(bhits 8: two Installer windows were open, the first was still there after Break; bhits 4 on the first.)

## Pixels (guest grab via ami.py shot, 800x600, exact; boxes 213x13, four buttons in a 2x2 grid)

| box (x0,y0,x1,y1) | corners TL TR BL BR | edge colours (452 px) | fill | other |
|---|---|---|---|---|
| 185,349,397,361 primary | a9b1d6 414868 414868 414868 | a9b1d6 224, 414868 228 | 7aa2f7 (1960) | 1a1b26 350 (label), 414868 236 (inside) |
| 403,349,615,361 secondary | same | same | 3e4254 (2079) | 1a1b26 231 |
| 185,363,397,375 primary | same | same | 7aa2f7 (2183) | 1a1b26 127 |
| 403,363,615,375 secondary | same | same | 3e4254 (2212) | 1a1b26 98 |

Expected: corners 1a1b26, no ring, secondary fill 00ff9f, no a9b1d6. Measured:
- corners are NOT 1a1b26 (a9b1d6 top-left, 414868 the rest): square, no rounding.
- a 1-px two-tone bevel ring is drawn: a9b1d6 top and left (223 px), 414868 bottom and right (236 px). Not flat.
- primary fill 7aa2f7 correct; label 1a1b26 correct.
- secondary fill is 3e4254, not 00ff9f (00ff9f occurs 0 times on the screen).
- a9b1d6 present in every button (223 px each).
No disabled button visible.

## Where 3.2 differs (from installer-button-class-trace.md, measured on 3.2 rig 98; not stepped through again)

Buttons are plain struct Gadget with two Border renders (GadgetText NULL), drawn by Intuition via a temporary
frameiclass object (OM_NEW, IM_DRAWFRAME, OM_DISPOSE, caller intuition.library+0x6c14). The trace says the pen-3 ring is
four 1-px RectFill strips inside IM_DRAWFRAME that the RectFill hook does not count, and that the fix is c8_skip
recognising the Border-render gadget plus a RectFill-hook replacement of the pen-0 erase. Here the fill is flat (the
pen-0 erase was replaced for the primary buttons) but the ring survives and the secondary fill is the bevel/shadow
pen colour 3e4254: c8_skip's Border-gadget test (render != select, type 5, GadgetText NULL) did not suppress the ring
on KS 3.2.3, and the secondary role is not mapped (frame=hooked flat=0 on all 38 frame calls: the c8 path never took the
flat branch). On KS 3.1 the same build passes (installer-rig94-32-proof.md), so the 3.1 frameiclass dispatcher path
differs from the 3.2.3 one. Unverified: the exact line; no probe was run in this measure.

Screenshot kept: scratchpad k32.png (FAIL shot). Rig 95 stopped after the measure.

## Confound: this measure ran Installer 47.19, not 43.3

`dopus_run.script` is `CD VTCX:dopus_install` + `Installer "Install DOpus5" NOLOG`, so the C: path picks the binary.
The 3.2 install image (Workbench_3.2.hdf) carries `$VER: Installer 47.19 (8.3.2021)`; the 3.1 rig runs Installer 43.3
(30.4.96). The FAIL above is Installer 47.19 on KS 3.2.3, a different program from the one the 43.3 fix targets.
Also: Amarchy and amarchy-flat were started by hand on stock 3.2 (no tokyo-night at boot), and 00ff9f occurs 0 times on
screen, so the secondary role pen may be unset rather than the draw path differing. Next: rerun on 3.2 with 43.3 pinned.
Rig note: `rig.py reboot --purpose` dropped `--os 32` and booted KS 3.1 (same class as rig 94 losing --max).

## Run 2: Installer 43.3 pinned (rig 93, KS 3.2.3)

Rig 93: `rig.py start --max --os 32`, `rig.py rtg` (800x600 depth 24; this restarts the rig), then `VTC:Amarchy` (starts
amarchy-flat; flat=on). exec.library 47.13, Kickstart 47.115 / Workbench 47.2. Installer 43.3 = build/rig-inputs/Installer
(`$VER: Installer 43.3 (30.4.96)`), copied to VTC:Installer43; run as `CD VTCX:dopus_install` + `VTC:Installer43 "Install DOpus5" NOLOG`
(VTCX:dopus_run43.script; C: untouched). Proof: `Version VTC:Installer43` = Installer 43.3; `Status` showed
"Loaded as command: VTC:Installer43" (one Installer only).
Theme up: T:wbpens RGB 0..5 = 1a1b26 414868 a9b1d6 7aa2f7 3e4254 0e0e14 (tokyo-night). 00ff9f is not a theme pen
(grep of theme file and amarchy sources: no 00ff9f); HEAD flat_glyph.c:423 fills the secondary role with `face` = pen_button
(3e4254) by OWNER-2026-11-08 FLAT-BUTTON-NO-RING. So the 00ff9f expectation is stale (rig 94's old binary), not a missing pen.

Counters: `rectfill hits=22 shits=24 mhits=0 bhits=4`; `frame=hooked calls=16 flat=4` (run 1 on 47.19: flat=0 of 38).

| box | corners TL TR BL BR | edge | fill | label |
|---|---|---|---|---|
| 185,349,397,361 primary | 31354d 1a1b26 31354d 1a1b26 | 7aa2f7 434 px | 7aa2f7 | 1a1b26 (350) |
| 403,349,615,361 secondary | 1f2130 1a1b26 1f2130 1a1b26 | 3e4254 434 px | 3e4254 | 1a1b26 (231) |
| 185,363,397,375 primary | as first | as first | 7aa2f7 | 1a1b26 (127) |
| 403,363,615,375 secondary | as second | as second | 3e4254 | 1a1b26 (98) |

No ring (edge = fill), no a9b1d6 in any button, bhits 4 > 0, frame flat=4 (the c8 path takes the flat branch with 43.3).
Left corners are antialiased blends (31354d / 1f2130), right corners 1a1b26. Against the stated PASS list: FAIL only on
secondary fill 3e4254 != 00ff9f (and left AA corners); against HEAD source: flat. Conclusion: run 1's FAIL was the 47.19 binary.
Shot kept: scratchpad k32b.png. Rig 93 stopped.
