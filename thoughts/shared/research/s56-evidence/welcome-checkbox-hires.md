# Welcome checkbox on a 640x256 hires PAL screen (fix 35c5bedc, merged a22308c3)

Build: main at c7684988, `make amiga`, rig 71 (--max --os 32), `rig.py aga` (Workbench PAL hires). Screen read back: `S 0 640x256 depth=4 "Workbench Screen"`.
Welcome opened with `VTC:Apps/bin/amarchy-gui welcome` after deleting ENV/ENVARC Amarchy/prefs. Window "Welcome to Amarchy" 568x205 at 36,25.
Measured from `ami.grab` pixels (Amiga pixels, no doubling):

- Box outline (2 px wide): 32 wide x 16 high. 32 == 2 x 16: PASS (square on screen).
- Tick fill after one click: 20 wide x 10 high (2:1, square on screen).
- Label "Don't show this again" starts 17 px right of the box edge; no overlap.

Screenshots: ~/Desktop/upterm-shots/welcome-checkbox-hires-unticked.png, welcome-checkbox-hires-ticked.png
