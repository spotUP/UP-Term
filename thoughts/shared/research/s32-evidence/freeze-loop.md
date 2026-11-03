---
date: 2026-11-03
topic: S31-RIG96-FREEZE - which Amarchy loop runs per vblank, why wasabid starves
status: not proven (loop function); earlier caller analysis corrected
agent: vtcon-16 hunt, rig 67
---

# S31-RIG96-FREEZE: boot-loop hunt (vtcon-16)

## Answer in brief

- Loop function and source line: **not proven.** No freeze in this hunt was
  caught with Amarchy alive, so no Amarchy stack was captured.
- What it waits for: the only evidence is from freeze-task.md: a task sits in
  graphics f87320, which is WaitTOF-shaped (Disable, AddTail to a list at
  10004c0c, likely GfxBase TOF_WaitQ, Wait(0x10), Remove, Enable). Amarchy
  source has no WaitTOF call. Grep finds WaitTOF only in
  render/amiga_render.c:3011, handler/vtcon_handler.c:3136 and :8869, and
  tests. So the per-vblank waiter is either the vtcon handler or the render
  code (linked into a console that Amarchy opens), not Amarchy's own loop.
  Unverified.
- Why wasabid starves: not proven. The boot-8 dump shows a different hang
  that happens before Amarchy or wasabid starts (see below).
- A/B: not run. The budget went on boots with the extended probe.

## Correction to freeze-task.md

The "callers" 10004c10, 00200104 and 100cf068 listed there are not return
addresses:

- 10004c10 is the stale node ln_Succ, equal to &list->lh_Tail. The list
  head is 10004c0c (likely GfxBase TOF_WaitQ).
- 00200104 is the old SigRecvd value pushed by SetSignal (bits 2, 8 =
  SIGF_DOS, and 21).
- 100cf068 is stack garbage.

At f87356 the routine ends with `adda #16,a7; movem.l (a7)+,a5/a6; rts`, so
the real return address sits 16 + 8 bytes above the node. A future dump must
read that slot.

## Boots on rig 67

Build: main f1f1fe5a plus another session's uncommitted WIP (desktop theme
default "on" in prefs/settings_def.c, an amconf_core.h comment, test
changes). All boots ran with that WIP.

- 8 boots, 2 failed, 6 OK.
- Boot 3: classic signature: "up", then read_ui timeout, then "the Amiga
  stopped answering while Assign was running" (start rc 1). Not dumped:
  the script then dumped only while the start was still running. The script
  was fixed afterwards.
- Boot 8: stalled at boot.log "go started"; "assigns done" never came.
  Dumped twice (frames 5566 and 6069), and both dumps are identical.

## Boot 8 walk (probe commit e41078a76 in fs-uae-intena, probe/intena-ring)

Raw dump: freeze-loop-ring67-boot8.txt (same folder).

- ThisTask is input.device, idle. intena is 602c.
- There is no Amarchy and no wasabid process in any exec list. The hang is
  earlier than the Amarchy line of User-Startup.
- 'Initial CLI' (100104c8) runs C:Copy and waits on sigwait 0x100
  (SIGF_DOS = waiting for a DOS packet reply). Its stack return addresses
  are all in ROM DOS/exec (f828da, f9faa8, f9f154, fa562e, fa3bc2, fa3b72).
- 'Background CLI' (1002a4c8) also runs C:Copy and also waits on 0x100.
  Its stack is the same shape (fa7234, fa71f2, fa3bc2, fa3b72, fc...
  handler calls).
- 'ramlib' (10022f00) waits on 0x10 (SIGF_SINGLE: a semaphore obtain or
  WaitTOF) with sigrecvd 0x100. Its stack holds only exec/ROM addresses
  (f80c0c, f828da, f82576, f9f86a).
- Every filesystem and handler task (CON, RAM, DH0-2, DF0) waits idle.
  No handler is working on the packets that the two Copy processes wait
  for.
- The only public semaphore is '. SetPatch .', and it is free. The
  semaphore ramlib waits on is private (not on SemaphoreList), so its owner
  is not visible in this dump.

Reading: two Copy processes wait for DOS replies that no handler sends, and
ramlib is blocked on a private semaphore. This looks like a library or
device open (through ramlib) that deadlocks against a filesystem packet
during the Assign/Copy phase of startup. This shape matches boot 3 ("stopped
answering while Assign was running"). It suggests that the rig-96 freeze may
be a startup-sequence deadlock, not an Amarchy vblank loop. Not proven.

## Fix direction (not implemented)

1. Find what the two Copy commands at "go started" copy, and which
   library/device they make ramlib load (the Installer/go script in
   tools/rig). A Copy into LIBS:/DEVS:/L: that runs while the same
   object is being opened is the prime suspect. Order those steps so
   nothing is copied over an object that a running task may open, or run
   them serially.
2. Extend the probe to print, for a task in Wait(0x10), the semaphore it
   queues on: walk the SemaphoreRequest on its stack, and print the owner
   task name. Also print for each handler its pending message port queue
   (pr_MsgPort mp_MsgList count). One dump would then name the deadlock
   pair.
3. For the original WaitTOF signature: in the dump, read the long 24 bytes
   above the node in f87320 frame. That long is the true return address.
   Map it against the vtcon-handler and Amarchy hunks.

## Scratch

Scratchpad files are all under 10 MB; none deleted. hunt67.py, hunt67.log,
ring67.txt stay in the session scratchpad. build/rig67 is removed.
