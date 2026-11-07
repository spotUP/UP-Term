# B6 stack sibling audit

Probe: opt-in knob `make STKPROBE=1 BUILD=build/stkprobe build/stkprobe/amiga/<name>` (amarchy/stkprobe.c, stkprobe.h, `-DAMDBG_STK`,
measuring stack 131072 through -D only). Prefixes AMIDX, AMICO, AMFLT, AMTIL; `GetEnv <PREFIX>_STK_USED` on rig 96.

## Byte-identical proof (no knob, against a `git archive HEAD` build)
cmp IDENTICAL for amarchy-flat (210060), amarchy-icons (169328), amarchy-index (149964), AmTile (80444).
Probe build sizes: flat 210816, icons 170132, index 150776, AmTile 81264.

## Measurement (bytes of stack used, high-water, rig 96, the probe binaries)
| step | index | icons | flat | AmTile |
|---|---|---|---|---|
| start, Amarchy up | - | - | - | - |
| amarchy-index SYS: / Work: (64 apps, 4 volumes) | 3812 | - | - | - |
| icons APPLY (lucide.dat missing, rc 10) | 3812 | 828 | - | - |
| icons RESTORE | 3812 | 1592 | - | - |
| amarchy-flat resident, idle wake | 3812 | 1592 | 2656 | - |
| AmTile start / ON | 3812 | 1956 | 2656 | 4004 / 4200 |
| icons APPLY, lucide.dat in place (223 icons) | 3812 | 7276 | 2656 | 4200 |
| icons RESTORE (223 restored) | 3812 | 2104 | 2656 | 4200 |
| cycle 1: SHOW, RELOAD, DESK CONFIRM, 2 shells, AmTile ON (3 tiled), OFF, QUIT, restart | 3812 | 7276 | 2656 | 4200 |
| cycle 2 (same, 5 tiled) | 3812 | 7276 | 2656 | 4212 |
| cycle 3 (same, 7 tiled) | 3812 | 7276 | 2656 | 4212 |

## Maximum and decision (rule: 2x max, power of two, floor 16384; raise only above 8192)
| program | max | 2x | stack |
|---|---|---|---|
| amarchy-index | 3812 | 7624 | 16384 kept |
| amarchy-icons | 7276 | 14552 | 16384 kept (closest to the 8192 line) |
| amarchy-flat | 2656 | 5312 | 16384 kept |
| AmTile | 4212 | 8424 | 16384 kept |
No maximum exceeds 8192, so no `__stack`, cookie or floor changes. MIN_BY_NAME entries carry the measured number as a comment.

## Tests
No program was raised, so there is no old-binary FAIL to quote (the floors did not move).
`python3 -I tests/test_amarchy_stack.py build/amiga/<name>` for amarchy-index, amarchy-icons, amarchy-flat, AmTile: each prints `amarchy_stack: 0 failed`.

## Not measured
- amarchy-flat patched Text/RectFill/etc. entry points: they run on the CALLER's stack, the probe covers only flat's own task.
- Flat on/off through prefs (desk_dos_flat_sync) and a theme apply: the flat resident was started by hand and idle; no drawing load.
- amarchy-index and amarchy-icons are not relaunched by Amarchy on restart in this setup, so cycles 1-3 re-measured them only through the resident env value.
- AmTile with real application windows (only two empty shells), layouts other than dwindle.
- amarchy-flat BOOT setup check printed FAIL (flat prefs off) in `amarchy_gui_rig.py --setup`; not a blocker for these numbers.

## Branch restart-measure
Obsolete: the probe now lives in stkprobe.c. Not deleted (owner decision).

## git diff --stat (commit)
Makefile 25, amarchy/flat.c 8, icons.c 8, index.c 9, stkprobe.c 69, stkprobe.h 31, tile.c 8, tests/test_amarchy_stack.py 8, plus this file.
SHA: in the reply.
