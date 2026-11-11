---
date: 2026-11-11
topic: rig 68 owner eye check state from the main checkout (FLAT-SPACING + FLAT-WIDE-INPUT)
branch: main
commit: 81743df6
rig: 68
---

# Rig 68 owner eye check state (main checkout build)

Measure only. No source edits, no builds, no commits. Pixels read as text through
`ami.grab` with the previous agent's helper scripts, copied to point at the main
checkout's tools/rig (session scratchpad, folder `main/`). No screenshots taken.

## Steps done

1. `UPTERM_RIG=68 rig.py stop` (rig clear saved 38 files to
   build/rig-saved/68/20261111-113923, removed build/rig68), then from
   /Users/spot/Code/UP-Term/vtcon:
   `rig.py start --max --purpose "s73: FLAT-SPACING + FLAT-WIDE-INPUT owner eye check"`, rc 0.
   Refreshed: amarchy-flat 239792, amarchy-gui 567920, amarchy-menu 684260,
   Amarchy 399124, upprefs 199380, pty-handler 13156, vtcon-handler 610512.
2. `rig.py hold --hours 4.5`: hold until 2026-11-11 16:11:23 (pid 217), idle watchdog off.
3. ENVARC:Sys/palette.prefs was missing again (the stop cleared the rig). Ran
   `Copy ENV:Sys/palette.prefs ENVARC:Sys/palette.prefs CLONE`, then `ami.py reboot`
   (back after 5 s). Date inset now draws 3e4254, not 7f007f.
4. `Run >NIL: SYS:Prefs/Time`. Prefs not saved, no gadget clicked.

## Q1 owner.json: PASS
build/rig68/owner.json =
{"root": "/Users/spot/Code/UP-Term/vtcon", "purpose": "s73: FLAT-SPACING + FLAT-WIDE-INPUT owner eye check"}

## Q2 resident amarchy-flat: PASS
`List UP-Term:bin/amarchy-flat` = 239792 bytes, 11:40:33 (after the start refresh).
`UP-Term:bin/amarchy-flat STATUS` after reboot with Time Preferences open:
flat=on gboxes=38 gfit=1 gfitbusy=0 side=1.

## Q3 date row box hugs text: PASS
Time Preferences window W0 at 0,14 size 332x203 (window-relative coordinates, grab origin 0,14).
Rows 163..179: non-ground x range 92..239 (3e4254 inset 1371 px, a9b1d6 text 296,
666c85 89, 8188a6 8). Outside x92..239 every pixel is 1a1b26 (plain window ground).
Box rows y167..178, corners rounded (y167/178 span 94..237, y168/177 span 93..238,
y169..176 span 92..239). Matches the 17-char expectation (x92..239).

## Q4 day-cell grid position: window-relative 62-63..107-108; difference is grab origin
Window list (uitree): W0 "Time Preferences" LeftEdge 0, TopEdge 14, 332x203, active.

Column x=30, grab of the window (origin 0,14):
cells 00ff9f y49..61, 64..76, 79..91, 94..106, 109..121;
ground gaps 1a1b26 at y62-63, 77-78, 92-93, 107-108.

Column x=30, grab from screen origin (0,0):
cells 00ff9f y63..75, 78..90, 93..105, 108..120, 123..135;
ground gaps at y76-77, 91-92, 106-107, 121-122.

The screen-origin gaps are the window-relative gaps plus exactly 14 = the window's
TopEdge. So the s72 numbers (y76-77..121-122) are screen coordinates and the last
agent's (y62-63..107-108) are window-relative; both describe the same grid. No real
layout change. Cell pitch 15 px (13 px cell + 2 px gap) in both.

## Unexpected

- rig start printed `[BUILD] stale before push: Amarchy, pty-handler, upprefs,
  vtcon-handler` for the main checkout's build/amiga (sources newer than those
  binaries). amarchy-flat, amarchy-gui and amarchy-menu were not in the stale list.
  Not rebuilt (task forbids builds).
- DOpus 5.104 not installed in the rig's VTC: drawer (warning only, as before).
- No hang, no reset loop this time (no `ami.py put` was needed: rig start refreshed
  amarchy-flat itself).

## End state

- rig 68 up, hold until 2026-11-11 16:11:23, idle watchdog off.
- Front and active window: W0 "Time Preferences" 0,14 332x203.
- Scratch: helper script copies in the session scratchpad `main/` folder; scratchpad
  total 260 KB; nothing over 10 MB.
