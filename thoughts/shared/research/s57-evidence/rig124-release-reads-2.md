# Rig 124 release reads 2 (2026-11-05)

Build: main b193312c, make amarchy-gui amarchy-flat Amarchy amiga, `amarchy_gui_rig.py --setup` rc 0, md5 of installed binaries equal build/amiga. Theme tokyo-night. Screen 640x256 depth 8. Shots: ~/Desktop/upterm-shots/s57r2-*.ppm.

| ID | Verdict | Measured |
|---|---|---|
| RL16 welcome | PASS | click in checkbox box (x+55,y+157): 200 px changed, interior 1a1b26 -> 7aa2f7. Rig script click point was wrong (blank area); fixed in amarchy_gui_rig.py case_welcome |
| FLAT-BUTTON-FILL | FAIL | idle fill 7aa2f7 (accent, 463 px), label 1a1b26; owner rule wants raised fill lighter_background 24283b. Held fill a9b1d6 |
| S32-GREEN-LINE-96 | PASS (not green) | title bar row 9 = 7aa2f7 for x 0..369, 1a1b26 for x 370..639; 0 green px in rows 0-14. Caveat: accent line covers only x 0..369 |
| S32-SCROLLBAR-PROP | PASS | Locale prefs: knob (530,150..170) = 7aa2f7; track around it 1a1b26; knob differs from track |
| RL3 | NOT RUN | backdrop 1a1b26 seen in window bg only; no desktop-pixel read |
| RL15 | NOT RUN | budget |
| RL8, RL20, RL21, RL19 | NOT RUN | budget |
