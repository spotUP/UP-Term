---
date: 2026-10-31
topic: rig 9 (3.2) on main d3a5346c, flat look measured
status: measured, scroller/white-edge result NOT flat (see Pixels)
---
# Rig 9 flat final

md5 (built in worktree agent-ae3a5905d5be5f7be, copied to VTCX:Apps/bin, re-read from the Amiga, equal):
Amarchy e71c9e27633129beffd80e69ca051dcd, amarchy-gui 78cc8d30d91eeb5e659814e4b439df35,
amarchy-menu 4158ffc8056ba0d96b489749fe5e09cd, amarchy-flat 1ea13ff0d0aa9ac089485a3862386eac.
Rig 9 had no amarchy-flat binary at all and flat=absent before; ami.py printed [REFRESH] on first call (15 files from main build).

Stop: `Break 5 ALL` (Amarchy takes gui and menu with it). Start: `Run >NIL: VTCX:Apps/bin/Amarchy`.

Toggle: scratchpad script, prefs written flat-system-look off then on, then Style > Amarchy theme > Return on the current theme
(tokyo-night) = gthemes_dos select -> desk_dos_apply -> dk_flat_pens. Same colours (theme re-applied).
Palette.prefs pen words (0x34..0x4F):
- before (off): 0001 0001 0000 0001 0003 0001 0000 0002 0001 0002 0001 0002
- after  (on):  0001 0002 0000 0000 0002 0000 0002 0002 0002 0000 0001 0002
Colour block (FFFF 0000 ...) identical. Requester: none (ami.py requesters empty; windows: "Amarchy bar", "Prefs", "Empty-12MB-FFS  50% full, 2GB free, 2GB in use", "Workbench").
STATUS: flat=on calls=277 chained=0 / pens text=2 line=3 back=0 outline=3 button=4 ptext=0 / rectfill calls=523 hits=64 shits=120.

Side effect: put_flat_prefs rewrote ENV:Amarchy/prefs with only the two flat keys: amarchy-gui-theme=white and desktop-theme=on were lost.

## Pixels (s28-rig9-flat-final.png, drawer window 28,39 462x103, Prefs drawer behind)
border left (28,80) 255,255,255 WHITE; border top (100,39) 170,170,170; title (100,43) 170; interior (200,100) 170; button face (480,115) 170.
Scroller column x=481: y51 (102,136,187), black/white alternation y52-99, track area dithered 170/255 at y=80; white pixels in window: 7938.
So: interior = border grey, but a white left border edge and a dithered white/grey scroller remain in the already-open windows
(likely not redrawn after the pens changed, or the drawer was opened before flat; unverified: reopen the drawer and re-sample).
