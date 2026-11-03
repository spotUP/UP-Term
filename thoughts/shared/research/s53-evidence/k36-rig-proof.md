---
title: K36 rig proof (kitty animation on the frame clock, image-animation key), AGA and RTG
date: 2026-11-03
session: vtcon-ee s53, rig 91
---
# K36 rig proof

Verdict: AGA on PASS, AGA off PASS, RTG on PASS, RTG off PASS.

## Setup
- Host part: main c63dcb4f. Rig tool: tools/rig/kitty_anim_rig.py (64x64 four-frame animation, 100 ms gaps, a=a,s=3; 4 shots ~150 ms apart; script counts differing pixels).
- PROF handler: fresh worktree of main (c1ec2125, detached), `make build/amiga/vtcon-handler PROF=1` (514480 bytes; strings show "PROF kitty anim switches/ticks").
- Pushing it: rig.py's refresh gate (ensure_built) rebuilds a handler whose flags differ from the environment, so a plain `rig.py start` from the worktree replaced it with a non-PROF build (506232). Fix used: run every rig command from the worktree with `PROF=1` in the environment (make -q then sees the PROF flags, the PROF build is pushed). Importing condev_rig from the MAIN checkout also refreshes the rig with main's build (506240, "84de0ec2_dirty"): the PROF file was overwritten once and re-pushed. Always run from the worktree with PROF=1.
- Handler proof: `Version VTC:vtcon-handler FULL` on the rig = `vtcon-handler 1.0 (05/29/21)` / `c1ec2125` (the PROF worktree build; main's dirty build prints 84de0ec2_dirty). Rig rebooted after the push (rig.py reboot, then start) before the first run; for RTG the rig was started again with the PROF file in place, same Version output.
- No window is shared: 8 PROF lines in serial.log, 4 per screen mode.

## Measured (neighbouring shot diffs in pixels, 800x600 shots)
| case | neighbour diffs | vs first | verdict |
|---|---|---|---|
| AGA on | 4096, 4096, 4096 | 4096, 4096, 4096 | PASS |
| AGA off | 0, 0, 0 | 0, 0, 0 | PASS |
| RTG on | 4096, 3840, 4096 | 4096, 4096, 4096 | PASS |
| RTG off | 0, 0, 0 | 0, 0, 0 | PASS |
4096 = 64*64: exactly the picture area changes, nothing else (so no other window overlapped the picture; a SolidWin window elsewhere in the shot was not checked, I did not view the shots).

## PROF lines (serial.log; second number is E-clock ticks spent rendering switches, ~709 kHz)
- AGA on: switches 0x48 (72), render 0xc653 (50771 ticks, ~71 ms total, ~1 ms per switch)
- AGA off: switches 0, render 0
- RTG on: switches 0x43 (67), render 0x111f5 (70133 ticks, ~99 ms total, ~1.5 ms per switch)
- RTG off: switches 0, render 0
Each window printed two lines (the second repeats the switch count with render 0, the counters reset after a print). off shows 0 as required, on shows > 0.
Note: the plan text says the PROF build counts "switches and their render time"; the log label says "switches/ticks" but the second number is render time (prof_anim), not a tick count.

## Spec note: re-arm cadence
render/vtwin.c anim_tick: after vt_anim_tick returns due >= 0 it calls frame_start(w), i.e. re-arms at the plain frame-clock period, not min(due, blink period). The plan text (K36, "frame_start re-arms with min(that, the blink's period)") is therefore not what the code does; the measured behaviour (72 switches in the window's life, shots always differ) matches the frame-clock cadence. Spec or code to be reconciled by the owner of K36; behaviour passes the rig criteria either way.

## Boot notes
- Every `rig.py start` (3) and the reboot/rtg restarts exited 1 with "the boot lacks the assigns ETC" while boot.log said "early LIBS: assigns done": RIG-START-ASSIGN-RACE (per vtcon-16), not the boot freeze. The rig answered afterwards each time; no retry was needed. No freeze seen in 4 boots.
- `rig.py rtg` stopped the rig and its restart failed with ConnectionRefused (same race); a manual `rig.py start` brought it up with the rtg screenmode file in place.
- `rig.py stop` deletes build/rig91 (serial.log gone), so the PROF lines above were read before stopping.
- Shots copied to ~/Desktop/upterm-shots/k36-*.png.
