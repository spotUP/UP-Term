# Rig 96 knob reproof on AmigaOS 3.2 (main 412780d5)

Install: `installer_rig.py ONLY fresh-all` (UPTERM_RIG=96, --os 32 --max, INSTALLER_KEEP=1): [PASS], menu window after reboot True. Kickstart 47.115, Workbench 47.2. (A first attempt was cut by my own `timeout 590` (rc 124) and rerun without it.)
Rig 96 left running.

XCON FillA (30,40,400x150), `type RAM:fill` (60 lines). Prop gadget 23312 box (416,52,10x105) = 1050 px. Pixels measured from screenshots (PIL counts).

| Row | Verdict | Counts |
|---|---|---|
| XCON active knob | PASS | a.png: 7AA2F7 1038, 1A1B26 12 (knob spans y56..152) |
| XCON inactive knob | PASS | b.png: 5A5E75 1038, 1A1B26 12 |
| Drawer control (System, 497,92,10x105), active | PASS | d.png: 7AA2F7 1038, 1A1B26 12 |
| Drawer inactive | NOT MEASURED | window was covered by other windows in b/c shots |

Caveat: knob fills nearly the whole box (no scrollback, so XCON reports no overflow); drawn-or-not is what is proven, not knob sizing vs content.
Shots: ~/Desktop/upterm-shots/r96knob-{a,b,c,d}.png (a active XCON, b inactive XCON, d System active).
FillA closed, RAM:fill deleted.
