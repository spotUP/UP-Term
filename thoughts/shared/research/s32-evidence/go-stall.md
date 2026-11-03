---
date: 2026-11-03
topic: S31-RIG96-FREEZE boot stall at "go started" - which Copies hang and why
status: root cause located, fix not implemented
---

# Boot stall at "go started" (rig 73, 2026-11-03)

## Setup

- Rig 73 ran `rig.py start --max --purpose "vtcon-16: go-started boot stall hunt"` in a 10-boot loop.
- The emulator was fs-uae-intena, branch probe/intena-ring, commit 673abee04. That commit adds pr_Arguments, queued pr_MsgPort packets and semaphore waits to the SIGUSR2 dump.
- A watcher sent SIGUSR2 once "go started" had been the last log line for 90 s. 90 s is used because rig.py's own 180 s wasabid watchdog retries the boot first, and good boots pass "go started" well before 60 s.
- One boot stalled. Boot log: `08:19:19 boot 60 s: go started` / `08:20:19 boot 120 s: go started`, with no change until the rig was stopped minutes later.
- Host load was high during the run: rigs 91 and 96 at about 108% CPU each, plus a vitest run from another repo.

## Q1: what the two Copy commands copy

Taken from the dump (frame 4953):

```
task wait 100104c8 'Initial CLI'    sigwait 00000100  command 'C:Copy'  args 'ENVARC: RAM:ENV ALL NOREQ'
task wait 1002a4c8 'Background CLI' sigwait 00000100  command 'C:Copy'  args 'C:UP-Term-Prefs VTC:kit-orig/C-UP-Term-Prefs CLONE'
```

- **Initial CLI** runs DH0:S/Startup-Sequence line 17: `C:Copy >NIL: ENVARC: RAM:ENV ALL NOREQ`. Its source is the ENVARC: tree on DH0:, its destination is RAM:ENV.
  - This matches the earlier boot-8 dump, where the RAM handler's stack held "zenbones-nordbones.conf", an ENVARC file.
- **Background CLI** runs BOOTX:go. It is in the first kit-shadow row's backup step, `Copy <dest> VTC:kit-orig/<bak> CLONE`. That row comes from kit_shadow_lines(False) in rig.py:257-288, which the GO template inserts.
  - Source: C:UP-Term-Prefs on DH0:.
  - Destination: VTC:kit-orig/C-UP-Term-Prefs.

## Q2: which semaphore ramlib waits on

**Correction: ramlib does not wait on a semaphore.** freeze-loop.md was wrong about this.

- ramlib has sigwait 00000010 and its return address is f82576.
- In Kickstart 40.68, f8255a-f82576 is exec WaitPort. It tests whether mp_MsgList is empty, then calls Wait(1 << mp_SigBit). Bit 4 (0x10) is ramlib's own port signal.
- So ramlib is idle, waiting for a library-load request.
- The probe's `waits-sem` line for ramlib is a false match:
  - sem 2a4833e8 is outside RAM.
  - owner and nest are 0.
  - The probe tests bit 0x10, which is SIGF_SEMAPHORE but is also ramlib's port bit.
- The only named semaphore in the dump is `. SetPatch .`, with owner 00000000 and no waiter.
- Nothing in the dump is blocked on a semaphore.

What the two CLIs actually wait on:

- Both wait only on 0x100, which is SIGF_DOS: each is waiting for the reply to a DOS packet.
- No process has a queued packet. Every handler shows `msgport queued 0`: RAM, DH0, DH1, DH1_0, DH2, DF0 and CON.
- Every handler is idle in its own wait. For example, RAM has sigwait c0000100.
- So each Copy's packet was taken off a handler's port and either run or dropped, but no reply reached the sender.
- This is a lost-reply deadlock, not a lock-order deadlock.
- Initial CLI shows sigrecvd 00000004. A stray signal bit may have been delivered where the DOS reply was expected. Not confirmed.

## Q3: root cause and fix direction

**Root cause:** rig.py STARTUP starts go in the background before the system Startup-Sequence runs, so go is already copying while SS still sets up the system.

```
tools/rig/rig.py:161  C:Run >NIL: C:Execute BOOTX:go
tools/rig/rig.py:162  C:Execute DH0:S/Startup-Sequence
```

- go's kit-shadow Copies therefore run at the same time as SS line 11 `SetPatch QUIET` and SS line 17 `Copy ENVARC: RAM:ENV ALL`.
  - SetPatch patches exec and DOS while other processes have DOS packets in flight.
  - The Copy ENVARC: RAM:ENV runs before `Assign ENV: RAM:ENV` at SS line 22.
- On the stalled boot, both Copies lost their packet reply in this window.
- The exact mechanism that drops the reply is unverified. The window is a scheduling race, which is why the stall happens only sometimes and more often under host load.
- rig.py:606 (OS_HOOKS) starts go the same early way on the other OS paths.

**Fix direction (ordering only, no sleeps or retries):**

- Start the kit-shadow pass only after the system Startup-Sequence has passed SetPatch, the ENVARC: to RAM:ENV copy and the ENV: assign.
- Do this by moving kit_shadow_lines(False) into the existing rig-upterm-shadows block in S:User-Startup. That block already runs `Execute BOOTX:shadows-up` at that point in the boot.
- Keep in the early `Run go` only what must run before SS, such as the wasabid start.
- With this order, no DOS packet from go can overlap SetPatch, whatever the host load.
