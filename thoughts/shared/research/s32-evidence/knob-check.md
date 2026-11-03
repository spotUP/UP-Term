# Scrollbar knob check, rig 34 (OS 3.2, flat on), 2026-11-03
Setup: installer_rig ONLY fresh-all rig 34 ([PASS]), overlay build/amiga/Amarchy (md5 6b8d0124..) + amarchy-flat (md5 15cab0f3..), both md5-matched via ami.py get, wasabi reboot, Welcome window up.
STATUS: flat=on calls=276 chained=0 / pens text=2 line=3 back=0 outline=3 button=4 ptext=0 / clone draws=53 fallback=0 / pens-held 16 refs=2 dim=250 knob=251 / frame=hooked calls=0 flat=0 (pen_knob=251, pen_dimtext=250; pen_back=0). Palette index->RGB not read.
Window SYS:C opened via arexx WORKBENCH 'WINDOW SYS:C OPEN' (400x200 at 50,100); props: vertical id2 at 436,124 10x110, horizontal id1 at 53,284 345x12.
Shots: ~/Desktop/upterm-shots/vtcon16-rig34-knob.png (icons only: content fits), vtcon16-rig34-knob2.png (Show all files + View by Name: content overflows).
Pixels (grab space):
- Track/background 1a1b26; knob 323f5f; arrows/ink a9b1d6; title bar/border 7aa2f7.
- Fits case: knob 323f5f fills x436-445 y124-233 (whole container, 1076 px).
- Overflow case: vertical knob x436-445 y124-135 (12 px tall) in track 1a1b26 y136-233; horizontal knob y284-295 x53-333 in track 1a1b26 x334-397. Arrows (a9b1d6) at y248-295.
Verdict: knob VISIBLE (323f5f vs track 1a1b26, contrast low: dark blue-grey on near-black, ~1.5:1) in both axes. Not pen-equal. Owner's "not visible" is likely low contrast, not missing paint.
