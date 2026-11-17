# DT-9 rig run (rig 63, os32, 640x256, DOpus 5.104), commit 54c444dc_dirty

Build: make dopus-toolbar -> 12 banks Amarchy-toolbar-{filled,outline}-{16,16t,24,24t,32,32t}, no old unprefixed files (dir is rebuilt). make amiga ok. rig.py install ok.
Rig 63 was already running ("already running" on start), so my --purpose was NOT applied (window title unverified).

amarchy_dopus_toolbar_rig.py:
[OK] (a) theme 1 "white": lister B toolbar = DOpus5:Buttons/Amarchy-toolbar-filled-16t  (DT-9 moves the lister to the right bank)
[OK] (a) lister A keeps DOpus5:Buttons/Custom-bank
[OK] (b) theme 1 CMAP filled-16t/Copy.small == dt_cmap
[OK] (a) theme 2 "white": lister B = filled-16t ; lister A custom kept
[OK] (b) theme 2 CMAP == dt_cmap
[FAIL] (b) second theme changed the CMAP bytes (identical ffffffab...)
RESULT: FAIL (1)

Cause (one level): both picks applied "white". Log: "theme list 23 rows, cursor on 21 (vantablack (current))" printed
both times, so AMARCHY_SEL/current marker did not move after theme 1 and d.pick() (amarchy_desk_rig.py:200-218) chose the same
row again. Same theme twice -> same bytes. Not a DT-9 bank problem. Open question: why the Amarchy theme list's current
marker is stale after an apply on this rig (maybe real Amarchy bug, maybe env var read before update).

End state: DOpus screen 640x256 with 2 listers (RAM Disk, Device List), amarchy-flat running, theme "white" last applied
(not vantablack), lister B on filled-16t. Screenshots deleted.
