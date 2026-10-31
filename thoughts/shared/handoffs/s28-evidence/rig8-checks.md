# s28 rig 8 checks (main 963d47ed)

Probe: ringprobe.c (this dir; ReadPixel of the 4 outer edges of two windows, A smart refresh, B simple refresh; ring pen 3 = accent 8d8d8d).
Logs: ring-32.log (3.2), ring-31.log (3.1).

## Prep
- Rebuilt build/amiga/amarchy-flat, Amarchy, amarchy-gui, amarchy-menu (all were 30 Oct, before cf78c357).
- First C9 run with the STALE Amarchy FAILED (pens 1/2/0/3, flat off pick did not clear enabled): the old binary lacked dk_flat_sync. Cause = stale build, not product code.

## A. C9 rig case, 3.2: PASS (second clean run, 0 failed)
before flat=off; after pick flat=on pens text=2 line=3 back=0 outline=3 calls 922 (rising); after off pick flat=off.
Rig gotcha: the case's "Break" does not kill amarchy-flat (it stays resident); a run right after another one reports "starts disabled" FAIL only if the
previous run left flat=on (seen once, run 2 of 3). Rerun was clean.

## B. Text grey, 3.2: PASS
Screen palette pen 1 = bebebe (190,190,190), pen 0 000000. Full-screen histogram: 190,190,190 = 5968 px, 255,255,255 = 0 px
(also 80,80,80 8005, 141,141,141 3553). Shot: ~/Desktop/upterm-shots/s28-rig8-32.png.

## C. Ring under resize / depth / simple-refresh, 3.2: PASS
Every visible outer-edge pixel = pen 3 (8d8d8d), active and inactive: open 580/580, activate A/B 580/580, resize A 240x110 700/700, B 170x80 500/500,
WindowToBack/Front 500/500 (700/700), A dragged over simple-refresh B 190/190 visible then A off B 500/500, B on top over A 500/500, final 700/700 and 500/500.
(B to back, 3.2: valid=0 on first sample = fully hidden by ReadPixel, next steps full.) STATUS rectfill installed=yes calls=17026..21751 hits=468..792 rising.
Note: the resident amarchy-flat on 3.2 was RAM:af-rf from the s27 session (a 963d47ed build), not today's rebuild; a new RAM:amarchy-flat exits as already resident.

## D. 3.1 (rig 8 restarted without --os 32): PASS
Fresh amarchy-flat (no resident before). Palette identical to 3.2: pens 0..3 = 000000 bebebe 505050 8d8d8d, STATUS pens 2/3/0/3, no pen difference seen
(the gate FLAT-31-G1 did not change pens in this configuration). Ring: all steps identical to 3.2, 580/580 open, activate, resize 700/700 and 500/500,
depth change, simple-refresh damage 190/190 then 500/500. STATUS rectfill installed=yes calls=209 hits=0 before, calls=656 hits=68 after.
Dopus line reads dopus=68.16 on 3.1 (dopus5 stamp absent, rig warns). 3.1 shot not taken.

## E. tests/test_amarchy_stack.py: PASS (amarchy_stack: 0 failed, with build/amiga/amarchy-menu from today's rebuild)

## Found
- build/amiga binaries in the main checkout go stale silently; amarchy_gui_rig --setup copies them as-is. make Amarchy amarchy-flat before any rig case.
- ReadPixel on a fully hidden window returns -1 for all pixels (valid=0), so hidden windows cannot be measured this way.
- Rig 8 is stopped.
