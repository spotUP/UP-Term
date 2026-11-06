---
date: 2026-11-06
session: vtcon-c1 (main session, host only, no rig run)
build: origin/main 65ab6335, vbcc from brew (targets/m68k-amigaos/lib/vc.lib)
verdict: DEFECT REAL (library code read on host), but NOT the cause of the rig crash: the rig proof failed with the fix linked (see restart-crash-rig-proof.md)
---

# Amarchy restart crash (TE-APPLY-DESKTOP-FREEZE-32): the C library aborts Amarchy behind its cleanup

Follows hunts 1-3 in this folder (apply-desktop-freeze-hunt.md, amarchy-start-freeze-hunt2.md,
amarchy-restart-crash-hunt3.md).

## 1. Why hunt 3's fix shape is not the root cause
Hunt 3 named two windows in the three ipatch users (strut, wbreset, borderblank): the few
instructions between the gate jump and busy++, and the 12 s busy timeout. Both are real, both
are far too narrow for the measured rate (Break + restart crashed 2 of 4). A 2 of 4 rate needs a
systematic path on which the patch removes do not run at all.

## 2. Measured (host)
vc.lib unit `_chkabort.c`, code hunk (11 longs), disassembled by hand from the library bytes:

    2f0e            move.l  a6,-(sp)
    2c79 xxxxxxxx   movea.l _SysBase,a6
    7000            moveq   #0,d0
    223c 00001000   move.l  #SIGBREAKF_CTRL_C,d1
    4eae fece       jsr     -306(a6)            ; SetSignal(0, SIGBREAKF_CTRL_C): reads and CLEARS Ctrl-C
    c0bc 00001000   and.l   #SIGBREAKF_CTRL_C,d0
    670c            beq.s   done
    4878 0014       pea     20.w
    4eb9 xxxxxxxx   jsr     _exit               ; exit(20)
    584f            addq.l  #4,sp
    2c5f 4e75       done: movea.l (sp)+,a6 / rts

Linker symbol `__chkabort` (C identifier `_chkabort`). References in vc.lib: stdio/_fillbuf.c,
stdio/_flushbuf.c, stdio/_putbuf.c. So every buffered stdio read or write polls Ctrl-C and calls
exit(20) when it is pending.

Repo: no definition of `_chkabort` / `__chkabort` anywhere (grep over *.c *.h Makefile *.md,
build/ excluded: 0 hits). Amarchy sources that use stdio: clips_core, flat_core, iconpen_core,
index_load, index_core, iconpen_dos, gthemes_core, leaf, menu_fontspec_core, job_dos,
iconapply_core, menu, pkgs_dos, pkgs_core, leaf_core, wblaunch, themewin (17 files).
Amarchy handles Ctrl-C only in its main Wait (amarchy/amarchy.c:636-637); the exit path
(amarchy.c:680-712) with the three patch removes is reached only from there.
The rig recipe sends `Break <n>` (Ctrl-C) to every running Amarchy-family process
(tools/rig/amarchy_gui_rig.py:1240-1241), then `Run >NIL: .../Amarchy` (:1305).

## 3. Mechanism
Ctrl-C arrives while Amarchy is NOT in its main Wait (index load, theme read, any file work).
The next stdio buffer call runs the library `_chkabort`, which clears the signal and calls
exit(20). exit() runs atexit hooks and the library shutdown, never Amarchy's exit path:
- the strut gates (OpenWindow, OpenWindowTagList), the wbreset gates (CloseWorkBench,
  OpenWorkBench) and the borderblank gates (OpenScreen, OpenScreenTagList) stay ARMED with
  hook addresses in a segment that is then unloaded;
- the three named semaphores and the commodity broker stay, with records owned by a dead task.
The next Amarchy loads at the same address (hunt 2: hunk 0 = 0x10235920-0x1028B0A8 both times).
Its strut_patch_install finds the old semaphore, frees its own gates and returns -3, so its
BSS statics stay zero: g_pub = 0, g_pt.gate = 0. The next OpenWindowTagList of any task goes
old gate -> same address = the new instance's tags_hook -> `ipatch_saved(&g_pt)` with gate 0
reads the long at address 18 -> `jsr (a2)` to an odd address -> Exception 3 at hunk offset
0x5392E. That is crash 2 of hunt 2 to the instruction. A window open while the new segment is
still loading runs half-loaded code: crash 1 (0x28116, data in sd_settings) fits that.
A fresh first start has nothing to Break: 0 of 3 crashes, as measured.

## 4. Invariant, levels, choice
Invariant: Amarchy leaves the system only through its own exit path; no other code may end the
process while it holds patches, semaphores, a broker or ports.
Levels:
- C runtime (root): the library ends the program on Ctrl-C from inside stdio. Fix here: the
  program supplies its own empty `_chkabort`, so Ctrl-C stays pending until the main Wait takes it.
- Patch layer (hunt 3 shape: saved target read from the gate, busy counted in the gate, stay
  resident on busy timeout): hardens two narrow windows, does NOT stop the library from ending
  the process with the gates armed. Alone it would be a cover for the runtime fault. Ledger row
  for after the release.
- Rig recipe (wait before Break, Break only when idle): hides the fault; a user's Break or a
  shutdown script hits the same path. Rejected.
Choice: the C runtime level, for every Amarchy-family program that handles Ctrl-C itself and
holds system state (Amarchy, amarchy-flat, and each sibling the fix agent finds with its own
Ctrl-C handling). Programs with no own Ctrl-C handling keep the library behaviour.

## 5. Proof still owed (rig)
One fresh 3.2 rig, two legs. Leg U (fix commit's parent): Break Amarchy while it is busy; expect
at least one cycle where the cleanup did not run (sentinel AMARCHY_STRUT_INSTALLED stays 1 after
the process is gone) or the restart crashes. Leg F (fix): 12 Break + restart cycles, cleanup ran
every time, 0 crashes. If leg U shows no skipped cleanup, this document's mechanism is not what
the rig hits and the verdict goes back to UNPROVEN.

## Correction, 2026-11-06 (after the rig proof)

The rig proof (restart-crash-rig-proof.md, rig 176, OS 3.2, 68060) ran the Break + restart
loop with and without the override. The build with the override still crashed in 5 of 12
cycles (Exception 3, CPU reset). The run without it never showed the state this note
predicted (process gone, patches still armed).

So the `_chkabort` path described here is a real defect and the override is a correct
hardening, but it is not the cause of TE-APPLY-DESKTOP-FREEZE-32. Section 1 ("Why hunt 3's
fix shape is not the root cause") is withdrawn: the patch-layer candidates of hunt 3
(amarchy-restart-crash-hunt3.md) are open again. The cause is UNPROVEN.

Note on the proof's reading: `strut_patch_remove` writes `AMARCHY_STRUT_INSTALLED` 0 before
its busy-leak return, so that sentinel at 0 does not show that the gates were freed.
