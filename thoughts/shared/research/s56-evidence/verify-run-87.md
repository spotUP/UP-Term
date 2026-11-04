---
topic: s56 shared verify run, rig 87 (Locale Save fill, S33 rig-only batch)
date: 2026-11-04
---
# Verify run rig 87 (s56)

Build: main 386666d6, own worktree `make worktree-ready` + `make -s amiga`; VTC:amarchy-flat md5 52e07378f7a58c6d65163144754035f4 equals build/amiga/amarchy-flat. Rig: `UPTERM_RIG=87 rig.py start --max --os 32 --idle-minutes 0`, Workbench 640x256 depth 8 (Kickstart 47.115, Workbench 47.2). Amarchy and amarchy-flat started by `Run >NIL:`; STATUS `flat=on`, `pens text=2 line=3 back=0 outline=3 button=4`. Palette (from the screenshots): pen 0 = 1a1b26, text = a9b1d6, accent = 7aa2f7, backdrop = 0e0e14, 414868 in the listview.
Screenshots: ~/Desktop/upterm-shots/s56-verify87-locale2.png, s56-verify87-input.png (Python/PIL pixel counts; nobody looked at the images).

## S56-LOCALE-SAVE-FILL: PASS (fill is pen 0, identical to a Workbench primary button)
Locale Preferences opened at 0,11 (552x192); gadget boxes from the UI tree are screen coordinates (the 100..187 x 224..237 box of the old note was another window position).
Locale Save box 12..99 x 181..194 (88x14 = 1232 px): 1148 px 1a1b26 (pen 0), 84 px a9b1d6 (label glyphs only), 0 px 7aa2f7, 0 px 414868. No outline.
Locale Use (232,181): 1167 pen-0, 65 label. Locale Cancel (secondary, 452,181): 933 pen-0, 299 a9b1d6 (1 px outline plus label).
Comparison, Workbench Input Preferences (same screen, same flat run): Save 1148 pen-0 + 84 label (exactly the Locale Save numbers), Use 1167 + 65, Cancel 933 + 299, "Show Double-Click >" 2534 pen-0 + 322 label, "Test Double-Click >" 2556 + 300.
Verdict: the primary button fill at depth 8 is pen 0 in Locale and in Workbench windows alike. Code agrees: flat_button_pen (amarchy/flat_core.c:560) returns pen_back when pen_sel is unset at depth 8. So the Locale Save fill is not a Locale-specific bug. Whether primary buttons should be distinct from the back pen (an unframed primary is invisible against the window) is a design question for the owner, not a failure of this check.

## S33-RIG-ONLY-BATCH: NOT RUN (no row left for this run)
The row text hands every 3.2 row to other runs: FLAT-SCROLLER, AMARCHY-WB-BACKDROP+RL3, S31-START-PALETTE, FLAT-BUTTON-FILL, FLAT-PENS-NOT-LIVE, R1.17-WBRESET and AM-FLAT-WB (3.2) to vtcon-ee's rig 96 fresh-all run (R3); AMARCHY-WELCOME to rig 64/65 (RL16 row, welcome-check.md); DESK-CHECK-INVERTED and AM-FLAT-WB 3.1 part wait for a batched 3.1 run. No new rig per row (owner 2026-11-03). Nothing was measured for them here.
Side data: FLAT-BUTTON-FILL (primary fill pen) matches the Save measurements above on 3.2.

## Cleanup
Rig 87 stopped, build/rig87 removed by `rig.py stop`/`clear`; fs-uae gone. Worktree ../vtcon-s56-verify removed.
