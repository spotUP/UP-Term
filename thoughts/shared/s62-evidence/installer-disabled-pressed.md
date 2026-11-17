# Installer 43.3 disabled and pressed buttons, amarchy-flat, tokyo-night (rig 95, KS 3.2.3)

HEAD includes 05678a64. build/amiga/amarchy-flat 212284 bytes, `cmp` IDENTICAL to build/rig95/vtc/amarchy-flat
(rig.py install run; guest `list VTC:amarchy-flat` = 212284). Rig 95: `rig.py start --max --os 32 --purpose
"Amarchy session: Installer disabled and pressed buttons"`, `rig.py rtg` (800x600x24). Installer 43.3 pinned:
build/rig-inputs/Installer put as VTC:Installer43 (`Version VTC:Installer43` = Installer 43.3), run as
`VTC:Installer43 VTC:t.inst NOLOG`. `VTC:Amarchy` then `VTC:amarchy-flat` started by hand. Tokyo-night pens up.

## PRESSED: PASS (measured)
Single-button message page, box 303,357,496,368 (194x12). Mouse held (ami._mouse down at 400,364), grabbed with
ami.py shot while held, then released.

| state | corners TL TR BL BR | edge | fill | label |
|---|---|---|---|---|
| normal | 1a1b26 1a1b26 31354d 31354d | 7aa2f7 (394 px), AA 31354d/5570aa 6 each | 7aa2f7 (1781) | 1a1b26 (139) |
| pressed (held) | 1a1b26 1a1b26 333344 333344 | a9b1d6 (394 px), AA 333344/747a94 6 each | a9b1d6 (1781) | 1a1b26 (139) |

Pressed = same rounded flat shape (rounded corners, edge == fill, no square bevel, no 414868 ring), fill swapped to
the foreground pen a9b1d6 (distinct from normal 7aa2f7), label unchanged. a9b1d6 is the whole fill, not a ring: edge
and interior are one colour; 2 distinct inside colours in both states.

## DISABLED: PASS (measured, s63, rig 95)
Method (option 1 reached it): `Run >NIL: VTC:Installer43 VTC:t.inst NOLOG MINUSER AVERAGE NOPRETEND` with t.inst = (welcome)
(message) (askbool with (help)). MINUSER AVERAGE ghosts the Novice radio, NOPRETEND ghosts the Pretend radio (uitree
`button disabled`, 16x9 radio marks, not push buttons). The push buttons ghost on the askbool page's Help page (askbool Help button, gadget 100; s63 owner rerun confirmed the path), not on the askbool page itself: two 70x13
gadgets 98 (184,363) and 99 (546,363) carry the uitree flag `disabled` (only the 214-wide gadget 91 stays enabled). Ghost
verified by behaviour: clicking both left the gadget list unchanged (page did not advance).
Same rig recipe as PRESSED (KS 3.2.3, Installer 43.3, VTC:Amarchy + VTC:amarchy-flat 212284 B started by hand, tokyo-night).

Box 70x13 each, 7aa2f7 = primary role, 3e4254 = secondary role. Normal reference = the enabled 214x13 gadget 91 on the same
page (primary) and the PRESSED table.

| state | corners TL TR BL BR | edge | fill | label |
|---|---|---|---|---|
| normal 214x13 (g91, primary) | 1a1b26 x4 | 7aa2f7 (430 px edge ring cells), AA 31354d/5570aa 8 each | 7aa2f7 (2525) | 1a1b26 |
| disabled 70x13 primary (g98) | 1a1b26 x4 | 7aa2f7 (106 edge px), AA 31354d/5570aa 6 each | 7aa2f7 (599) + ghost dots 1a1b26 | 1a1b26 |
| disabled 70x13 secondary (g99) | 1a1b26 x4 | 3e4254 (107 edge px), AA 1f2130/333344 6 each | 3e4254 (602) + ghost dots 1a1b26 | 1a1b26 |

Counts per 910 px: g98 7aa2f7 599, 1a1b26 299, AA 12; g99 3e4254 602, 1a1b26 296, AA 12. Ghosting is the Intuition ghost
pattern: every 4th pixel of each row is 1a1b26 (phase shifts on alternate rows); 64 of 256 px in the four label-free rows = 25%.
It covers the label, which stays readable as 1a1b26 text through the dots. No 414868 pixel in either box, no square bevel, corner
and edge cells follow the normal rounded flat shape (AA pairs in the same family as normal).

STATUS before (both started, before the Installer): amarchy `rc 0 hits=0 rows=71 enabled=1 active=0 wins=0 snaps=0 bar=1 wbpens=0 wbrefused=0 shots=0 notes=0 ticks=1`;
`VTC:amarchy-flat STATUS`: `frame=hooked calls=0 flat=0`.
After (ghost page, clicks done): amarchy `... hits=0 ... ticks=124`; amarchy-flat `frame=hooked calls=140 flat=35`.
The ami.py rexx one-liner printed the full line this time (the s62 run printed only rc 0).

## Verdict
- owner eye check 2026-11-08 (rig 95, askbool Help page): PASS, "they look disabled"
- pressed: PASS
- disabled: PASS (rounded flat shape like normal, no square bevel, no 414868 ring, label dimmed by the 25% ghost pattern but
  legible). Caveat: the dim is a 25% dot overlay, so a ghosted primary button stays bright 7aa2f7 on average; whether that
  reads as disabled at a glance is the owner's call (no human eye check done).

Screenshots deleted after the numbers were taken.
