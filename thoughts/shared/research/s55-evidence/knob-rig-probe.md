---
title: XCON-KNOB-FULL-BOX rig probe (rig 55, main 486f98ff)
---
# Result: no defect. A 60+ line scrollback gives a small knob on main.

Rig 55 (--max, fresh `make amiga`, rig.py install), XCON FillA 400x150, prop gadget 23312 box (416,55,10x102).
Profile: ENV:up-term/up-term has scrollback only as a commented example (default 500 lines), so suspect 1 is out.

| State | Knob (pixels, column x=421) | Shot |
|---|---|---|
| Shell prompt only, no scrollback | solid knob y51..159, 105 of ~108 px = full box (correct: nothing to scroll) | ~/Desktop/upterm-shots/r55knob-a.png |
| After `type RAM:fill` (80 lines, screen text via UPTerm ACTIVE screen FILE shows lines 74..80 + prompt) | solid knob y148..156 (~9 px of 102 = ~9%); rest of the box is the black/blue dithered track | ~/Desktop/upterm-shots/r55knob-b.png |

The knob went from full to 9% after output with no resize or redraw trigger: vtwin_knob_sync runs after output (suspect 3 out),
sbar_gad_set applied the change (suspect 4 out), scrollback_lines > 0 (1 out), output scrolled the grid (2 out).
9% is consistent with ~8 visible rows over ~83 total + 8.

Likely cause of the s54 "full knob" report: its `type RAM:fill` never ran. `ami.py type "cmd\n"` leaves the command on the
prompt line without Return (shot r55knob-a state before I sent key 0x44: the command sat unexecuted, knob full). Return is
`ami.py key 0x44`. Unverified: s54's own typing method is not recorded.

No handler debug command prints PGA_Total/Visible; UPTerm scrollback needs an argument (LINES | none) so it cannot read the count. Not needed.
