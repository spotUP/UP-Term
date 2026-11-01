---
title: s28 FLAT-BUTTON-ROUND-BG, round button corners on a non-pen-0 background
status: rig-verified
---

# Round button corners on a patterned background, rig 10 (3.2, 256 colours), 2026-11-01

Setup: build from origin/main a25d9ef3; `rig.py start --max --os 32`, `rig.py aga256`, `amarchy_gui_rig.py --setup`,
theme vantablack, flat prefs on. `amarchy-flat STATUS`: flat=on, pens text=2 line=3 back=0 outline=3 button=4.
Workbench pens 0-3: 000000, bebebe, 505050 (pen 2), 8d8d8d (pen 3).

Probe: `tests/amiga/btnbgprobe` opens a window at (40,40) 160x100 filled with a 2x2 checker of pens 2 and 3 (never pen 0), mode
GAD adds one 80x14 GadTools button at (60,70). `tools/rig/amarchy_btnbg_rig.py` grabs the window without the button
(baseline), with the button idle, and with the mouse held on it. For each of the 12 pixels the box cuts (4 corners + 8 cuts,
h >= 12) it compares the pixel with the baseline pen at the place the product samples (flat_button_corners in
amarchy/flat_glyph.c: diagonal for a corner, the pixel above or left of the box for the cuts).

Result: PASS idle (12 of 12), PASS pressed (12 of 12). Flat hook ran (frame calls 102 -> 142), button drawn (fill 0000ff
idle), press changed 760 interior pixels. No cut pixel is pen 0 or the button fill.

Note: a first run compared each cut with the baseline pixel at the SAME place. On a 2x2 checker that reads 8 false FAILs
(the cut holds the neighbouring outside pen, by design). The criterion in the script is the sampled outside pen.
