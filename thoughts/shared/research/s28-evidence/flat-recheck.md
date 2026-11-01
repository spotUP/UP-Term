---
title: s28 flat recheck on rig 11 (button round, inactive title dim)
status: rig-verified
---

# Flat recheck, rig 11 (3.2, 256 colours), 2026-11-01

Setup: build from origin/main 19888120 (amarchy-flat, Amarchy, amarchy-gui, amarchy-menu); `rig.py start --max --os 32`,
then `rig.py aga256` worked on 3.2 (wbpens: DEPTH 8). amarchy_gui_rig.py --setup, put_flat_prefs(True)+pick() (theme vantablack).
`RAM:amarchy-flat STATUS`: flat=on, pens text=2 line=3 back=0 outline=3 button=4 ptext=0; dri_Pens (wbpens) SHINE=0 SHADOW=0
FILL=0 BACKGROUND=0 = the flat map (this is the fix over rig 8, where SHINE stayed 2). With windows open: `frame=hooked calls=162 flat=36`
(frame calls > 0 and flat > 0). Windows: tests/amiga/flatprobe FOREVER (inactive at x=10, active at x=250; not overlapping).
Palette (wbpens): pen 0 aaaaaa, pen 3 6688bb, pen 5 ff00ff. Screen grab by ami.grab (640x256), PNG
~/Desktop/upterm-shots/s28-flat-recheck-rig11.png (unread; pixels measured in a script).

## FLAT-BUTTON-ROUND: PASS (limited)
Four 80x14 buttons (two per window), box (18,48)-(97,61) etc. Fill 0000ff, pixel just outside = aaaaaa.
- 4 corner pixels of each button = aaaaaa (= pixel just outside), not 0000ff. All 16 corners.
- the 8 extra cuts (h >= 12: (x0+1,y0),(x0,y0+1) and the other three corners) = aaaaaa.
- (x0+2,y0+2) and mid-top edge = 0000ff (fill); the first inner diagonal pixel = 0000ff.
Limit: the background here is pen 0, so a fixed pen 0 would give the same numbers. "Reads the pen outside" vs "fixed pen 0" is NOT
separated by this run (needs the patterned/MUI group-box case of button-round.md step 6). Pressed state not done either.

## Inactive title dim: PASS
- inactive window title bar (x 40-190, y 30-39): aaaaaa 1215 px (bar), ff00ff 135 px (glyphs) = palette pen 5 (ff 00 ff); 6688bb 150 px (accent in the bar).
- active window title bar: 6688bb 1398 px (accent band = pen 3, 66 88 bb), aaaaaa 102 px (glyphs).
