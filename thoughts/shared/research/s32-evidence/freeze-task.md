---
date: 2026-11-03
topic: S31-RIG96-FREEZE - which task runs at the freeze
author: vtcon-16 agent (rig 66)
status: task named; the lock wasabid waits on is NOT proven
---

# Boot freeze: the task that runs (S31-RIG96-FREEZE)

## Run
Rig 66, main 72b9264c, `--max`, probe binary /Users/spot/Code/Emulators/fs-uae-intena/fs-uae (probe 9a8cedf1a),
`UPTERM_INTENA_RING=<scratchpad>/ring.txt`. Driver: scratchpad hunt.py (reads build/rig66/boot/boot.log; SIGUSR2 to the
rig 66 fs-uae pid once boot.log is unchanged 110 s after "go started", twice 10 s apart, then ends the start and stops).

Boot 1 froze (1 of 1):
- 07:28:47 `rig.py start` printed "up" (go's "assigns done" + wasabid answered once).
- 07:29:17 the start's own retry_wb_reset read_ui (every_screen) timed out: wasabid stopped answering 30 s after "up".
- boot.log tail: `assigns done`, `wasabid started` (the usual signature).
- 07:30:35 and 07:30:45: SIGUSR2 to fs-uae pid 10413 (rig 66), frames 7406 and 7957 (~11 s apart, same picture).
- 07:31:08 `rig.py stop` rc 0; build/rig66 gone, no rig 66 fs-uae in ps.

## Answer
The only task that runs is the process named **'Background CLI'** (task 104079c0, a Run'd CLI) whose CLI command is
**`UP-Term:bin/Amarchy`**. It is not wasabid.

Both dumps:
```
=== INTENA ring dump (SIGUSR2) frame 7406 seq 2013722 intena 682c intena_int 682c intreq 0041 stopped 1 pc 100317e6 sr 2000
thistask 104079c0 cli 100f68a4 seglist 10438ddc command 'UP-Term:bin/Amarchy'
=== INTENA ring dump (SIGUSR2) frame 7957 seq 2028678 intena 682c intena_int 682c intreq 0041 stopped 1 pc 100317e6 sr 2000
thistask 104079c0 cli 100f68a4 seglist 10438ddc command 'UP-Term:bin/Amarchy'
```
All 256 ring entries in each dump (about 10 frames) carry task 104079c0 'Background CLI'; no other task is switched in.

One frame (26 writes, the same every frame):
```
v 4000 682c->282c pc 100ac47a sr 2000 ...                (RAM code, supervisor)
v 4000 682c->282c pc 00f8280a sr 2000 (a7) 100ac572      exec Signal(), called from 100ac572
v 4000 ...        pc 00fc010e/00fc15bc/00fc123a/00fc1584/00fc12b0 sr 2319   level 3: timer.device, cia.resource
v 4000 682c->282c pc 00f828dc sr 0010 (a7) 00f87344 idn -1 tdn 0     Amarchy returns from exec Wait()
v c000 282c->682c pc 00f87356 sr 0010 (a7) 10004c10                  graphics routine's Enable, returns to 10004c10
v 4000 682c->282c pc 00f827ec sr 0019 (a7) 00f8731c                  exec SetSignal()
v c000 282c->682c pc 00f8287c sr 0019 (a7) 00200104                  SetSignal returns to 00200104
v 4000 682c->282c pc 00f87320 sr 0019 (a7) 100cf068                  graphics routine entered again
v 4000 282c->282c pc 00f82890 sr 0019 (a7) 00f87344                  exec Wait()
v c000 682c->682c pc 1003176a sr 2000                                relocated exec dispatcher: switch to idle, STOP
```
Module map from the ROM's RomTags (kick40068-a1200.rom): 00f827ec/00f828dc = exec.library (f800b6-f83706),
00f87320 = graphics.library (f851a8-f9d7a4), 00fc123a.. = timer.device, 00fbfcf6 = cia.resource.
Disassembly of 00f87320 (graphics): `Disable; AddTail(list, node on stack); moveq #$10,d0; jsr Wait(-318); Remove(node);
Enable` = the WaitTOF shape (node on GfxBase's TOF wait queue, SIGF_SINGLE).

What it waits on: Amarchy's process sits in a per-frame graphics wait (WaitTOF-shaped: SIGF_SINGLE on a queue that a
vblank-time Signal empties), wakes every vblank, calls SetSignal, and waits again; it holds no Forbid/Disable while it
runs (idn/tdn -1 outside the exec calls). That is a poll loop: it waits for something that never comes, and while it
loops no other task (wasabid, input.device, Workbench) becomes ready.

## Not proven
- Which code in the Amarchy process runs the loop. No `WaitTOF` in amarchy/*.c; the callers 10004c10 (WaitTOF's return),
  00200104 (SetSignal's) and 100cf068 are not the first hunk (seglist 10438ddc); they may be a second Amarchy hunk, a
  library called from Amarchy (intuition, libupkgui) or a patch stub. Needs a memory read of those addresses.
- What wasabid waits on. It never runs in either dump, so it sits in Wait() on something nobody signals; a lock Amarchy
  holds (LockIBase, LockPubScreenList, one of Amarchy's patch semaphores) fits wasabid hanging inside read_ui
  (every_screen) and the frozen screen, but the dump shows no lock state.
- Ties to 31ce61a5 (Amarchy's start steps from desk_start_gate, Amarchy BOOT from S:User-Startup before LoadWB): the
  freeze lands in the same window (Amarchy starts before the Workbench screen, wasabid answers once, then the read_ui
  hangs); suggestive only. Earlier reports of the same freeze are older than 31ce61a5, so it is at most a trigger.

## Fix direction (not implemented)
Root cause is in Amarchy's start path, not in wasabid or the emulator: some step in the Amarchy process waits frame by
frame (WaitTOF poll) for a condition, very likely the Workbench screen, while holding an Intuition or Amarchy lock that
wasabid's window walk and input need. Next probe, decisive: on a frozen boot read the Amarchy task's stack chain
(A7 104079c0's tc_SPReg walk, return addresses resolved against Amarchy's seglist hunks) and wasabid's tc_SigWait and
the IBase/pubscreen semaphore owners (wasabi debug can't, wasabid is dead: extend the probe dump to read
IntuitionBase's lock semaphores' ss_Owner and ss_NestCount). Fix: no lock held across a wait for the Workbench screen;
the wait goes through the 1 s tick (desk_start_gate) that never holds a lock, with a regression test that boots before
LoadWB. A/B check meanwhile: 8 boots with the Amarchy BOOT line out of S:User-Startup; zero freezes there points the
same way.

## Files
- Raw ring (both dumps, 546 lines): scratchpad ring.txt of session 286924bf (not copied; key lines above).
