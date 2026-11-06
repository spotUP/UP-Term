---
date: 2026-11-06
session: up-term-49 (sonnet measurement agent, hunt 4)
build: branch restart-measure HEAD f33e987b (AMDBG markers), build/amiga/Amarchy 6 Nov 18:55, md5 eb252028e0a465562517b0c6e763b2b5 (same md5 as the copy on the rig)
verdict: PROVEN for the faulting code and bad pointer value; UNVERIFIED for how the gate memory became garbage (see section 4)
---

# Amarchy restart crash, hunt 4 (MuForce)

## 1. Rig and recipe
Rig 177, `rig.py start --max --mmu --os 32`, then `amarchy_gui_rig.py --setup --no-build` (known FAIL "flat prefs off"), `rig.py muforce on`.
Loop (scratchpad loopm.py, rewritten): Run Amarchy, wait for port AMARCHY, +10 s, read MuForce hits.log and grep fs-uae log for Exception, Break, wait for exit, read both again. No marker polling. Step A only; step B was not needed (a crash plus a hit came in cycle 2).

## 2. Per-cycle table
| cycle | crash | PC | MuForce hits |
|---|---|---|---|
| 1 | no (Break ok, exit ok) | - | 0 |
| 2 | YES, Exception 3 about 6 s after Run (19:39:44), after a clean Break of cycle 1 | 40A98582 | 1 (printed twice, identical) |
| 3-12 | not run (stopped at the first crash; question answered) | - | UNVERIFIED |

fs-uae log: `Exception 3 (6b796f2c) at 40a98582 -> f80bc2!`

## 3. Verbatim MuForce hit
```
06-Nov-26  19:39:44
Exception !!   00000003     TCB: 40948F80     CTX: 4005F3F8     SSP: 4000230C
USP : 40B3CE40 SR: 0000  (U0)(-)(-)  TCB: 40948F80
  PC: 40A98582
Data: 40B3CEF8 0000002D 00000000 40B3CE7C 40A926EC 00000001 00000000 FFFFFFFF
Addr: 00000000 40B3CEF8 6B796F2D 40A79946 00000000 40950780 4000BDC4 4000230C
Stck: 40A798F0 00000000 00000001 40A926EC 00000001 00000004 40009310 000001B2
Stck: 00000000 40950780 4000BDC4 6B796F2D 40B3CEF8 00000000 4000BDC4 40950780
Name: "Background CLI"  CLI: "VTCX:Apps/bin/Amarchy"  Hunk 0000 Offset 00053BDA
```
No other MuForce hit (no freed-memory read, no low-address read) before the exception.

## 4. PC to function (arithmetic)
- MuForce: hunk 0, offset 0x53BDA, so code hunk base = 0x40A98582 - 0x53BDA = 0x40A449A8 (hunk size 0x55788, so 0x40A449A8..0x40A9A130).
- Link map: probe link of the same objects with -M, output cmp-identical to build/amiga/Amarchy (map in scratchpad Amarchy.map). amarchy/strut_patch.c CODE = 0x53288..0x54B78; 0x53BDA = strut_patch.c + 0x952, before _strut_patch_install (0x53D84): static `tags_hook` (strut_patch.c:218), the OpenWindowTagList hook.
- Bytes in the binary at code offset 0x53BDA (file offset 52 + 0x53BDA): `4E92` = jsr (a2), directly after the call `ipatch_saved(&g_pt)` (strut_patch.c:237, reading the saved-vector field of g_pt.gate).
- Bad pointer: A2 = 0x6B796F2D (ASCII "kyo-", odd) = the value returned by ipatch_saved, i.e. the gate's saved-vector field holds text, so the gate block g_pt.gate points at was freed and reused (or never valid). The jump to an odd address gives Exception 3 (fs-uae shows 6b796f2c).
- Caller: stack return 0x40A798F0 - 0x40A449A8 = 0x34F48 = gthemes_core.c +0x2828 (_gt_ref_text region); second return 0x40A926EC - base = 0x4DD44 = bar.c _bar_tick +0xBE8. Stack return addresses are heuristic (UNVERIFIED as true frames).

## 5. Old or new instance
Task is "Background CLI" running VTCX:Apps/bin/Amarchy (the Run created process), TCB alive; MuForce resolved the PC to hunk 0 of that task's own segment, and the caller return addresses lie in the same hunk range. At Run time the previous instance had already left (Status showed no Amarchy process). So the faulting code is the NEW instance's own tags_hook, called by the new instance itself through the OpenWindowTagList vector, not the old unloaded code. UNVERIFIED independently by instance base addresses (step B not run); the old instance's hunk base was not logged.

## 6. Not measured
- Why the gate's saved field holds text: whether g_pt.gate was freed by the new instance, or the vector the new instance installed points at a freed gate of the old instance (stale chain). The A3 register 0x40A79946 and the gate address were not resolved.
- Crash rate for steps over 12 cycles (stopped at first crash).

## 7. Cleanup
Rig 177 stopped (rig.py stop removed build/rig177 including hits.log; the hit text is above). Nothing changed on the rig by md5 (setup files only). Scratchpad keeps loopm.py, runA.log, Amarchy.map (575 KB), Amarchy.probe (369 KB).
