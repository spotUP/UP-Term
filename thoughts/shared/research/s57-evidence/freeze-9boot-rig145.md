---
date: 2026-11-05
topic: S57-FREEZE-2IN9 root cause, read from the live hung rig 145 (3.2, 060 max), worktree vtcon-s57-freeze9 f8c4ddba
verdict: FIXED (ceebb596): lock ring between Amarchy and the Workbench task; 19 clean boots on rig 146, 0 hangs
---

# S57-FREEZE-2IN9: the boot freeze is a lock ring on the public screen list

## Symptom

Rig 145 boot series from a full install: boots 0-4 clean, boot 5 of 9 never answers a wasabi ping (647 s, no recovery).
FS-UAE keeps running at 91% CPU. Earlier series: 2 hangs in 9 boots.

## How it was read

The hung FS-UAE process (pid 68006) was inspected with lldb (`natmem_offset` + Amiga address, big-endian), no reboot.
ROM routines were disassembled from the ROM image in memory (0xf80000) with m68k-amigaos-objdump.

## Measured state in the hung machine

- IntuitionBase = 0x1000bdc4. Lock array at +2082, 46 bytes each; lock index 4 (+2266) guards the public screen list.
- Word at IntuitionBase+2874 (Workbench open/close guard) = 1.
- Public screen list: node 0x1000d84c "Workbench", psn_Screen 0x10052a90 (title "Workbench Screen"), psn_Flags 0x0001
  (PSNF_PRIVATE), visitors 1.
- Workbench task 0x1066cf10: its LockPubScreen("Workbench") took the reopen path (0xfe8b50), set the guard word,
  opened the screen (0xfeab5a, 0xff1b1c) and now sits in the PubScreenStatus body (0xfe8a74) that makes the screen
  public. That call waits for lock 4.
- Amarchy task 0x100ff0e0, at start (wbmode.c wb_mode_id): inside `ipd_lock_wb_screen`. It holds lock 4
  (LockPubScreenList) and is inside LockPubScreen("Workbench"), in routine 0xfeaac8: Forbid, guard word not 0,
  Permit, WaitTOF, loop.
- IPrefs, amarchy-flat and wasabid queue on lock 4. wasabid holds lock 0, so input.device stops as well. That is why
  the rig stops answering.

## ROM facts behind it (Intuition V40 in the 3.2 ROM)

- LockPubScreen (LVO -510, 0xfe88cc) calls 0xfe8af8: lock(4), find the node by name, `btst #0,19(node)`: a node with
  PSNF_PRIVATE counts as not found, unlock(4).
- Not found and the name is "Workbench": call 0xfeaac8 with d0 = 1, the "reopen the Workbench" path.
- 0xfeaac8 waits for the guard word to be 0 by polling with WaitTOF. It never gives up.

## The ring

1. LoadWB's Workbench task opens the Workbench: holds the guard word, screen node in the list but still private.
2. Amarchy takes lock 4, `ipd_wb_screen_in` finds the "Workbench" node with a screen, so `ipd_lock_wb_screen`
   calls LockPubScreen("Workbench").
3. Intuition treats the private node as not found and takes the reopen path: Amarchy polls the guard word, still
   holding lock 4.
4. The Workbench task needs lock 4 to make the screen public and drop the guard. Neither side can move.

The window is the short time between "screen node added" and "screen made public" in the Workbench open, so the hang
shows in only some boots.

## Violated invariant

`ipd_lock_wb_screen` holds the public screen list lock across LockPubScreen("Workbench"). That is safe only when the
call cannot take the reopen path. The helper checked "a Workbench node with a screen is in the list". Intuition's own
test is stricter: the node must also be public. BOOT-31-WB-DEADLOCK fixed the "node absent" half of the same
invariant; this is the "node present but private" half.

## Levels the fix could live at

1. The predicate of the single helper (root level): ask for the lock only when Intuition will find the node, which
   means listed, with a screen, and not PSNF_PRIVATE. A private Workbench reads as "not open yet", the state every
   caller already handles (Amarchy starts before LoadWB).
2. The lock order: drop the list lock before LockPubScreen. Wrong: without the list lock the screen can close between
   the look and the lock, which brings BOOT-31-WB-DEADLOCK back.
3. The caller (wbmode.c start path): delay or retry around the start. A band-aid: 15 other callers of the helper keep
   the ring, and a delay only moves the window.

## Options compared

- Option A, level 1, one pure predicate `ip_wb_lockable(listed, has_screen, flags)` in iconpen_core.c, used by
  `ipd_lock_wb_screen`. Cost: a few lines plus a host test. Covers every caller, since all go through the helper
  (tests/test_wb_lock_guard.py enforces that). Does not cover a program outside Amarchy that locks the list and
  the Workbench; none of ours does.
- Option B, make `ipd_wb_screen_in` itself skip a private node. Same effect for the lock helper, but it also changes
  the two users that only read the screen under the list lock (pen sync at iconpen_dos.c:175, recolour at :288).
  They do not call LockPubScreen and cannot enter the ring. For them a private Workbench would read as "screen
  gone": the pen holder would drop its record while the pens are still allocated on the live screen. That creates a
  second problem to cover the first.
- Option C, level 3 retry in wbmode.c. Rejected above.

Chosen: Option A. Two facts stay two functions: "the Workbench screen exists" (`ipd_wb_screen_in`, for readers under
the list lock) and "LockPubScreen will find it" (`ip_wb_lockable`, for the lock helper).

## Callers and the 0 result

`ipd_lock_wb_screen` returning 0 already means "Workbench not open" everywhere (S32-START-LOOK: Amarchy starts from
S:User-Startup before LoadWB). wbmode.c `wb_mode_id` returns WBM_INVALID_ID and the hold waits for the next tick;
`desk_start_gate` runs again on the next tick. No caller needs a change.

## Regression test

- Host, tests/test_iconpen.c: "a Workbench that is still private while it opens is not locked": the predicate gives 0
  for a private node, 1 for a public node with a screen, 0 with no screen, 0 when not listed.
- Host, tests/test_wb_lock_guard.py: the helper must consult `ip_wb_lockable` before LockPubScreen("Workbench").
- Rig: boot series on a full install built from the fix (see the result section).

## Result

Fix: commit ceebb596 on branch `s57-freeze-fix`. `ip_wb_lockable(listed, has_screen, flags)` in iconpen_core.c;
`ipd_lock_wb_screen` asks for the lock only when the predicate is 1.

Host: iconpen suite and tests/test_wb_lock_guard.py green on the branch (red before the fix).

Rig proof, 2026-11-05, rig 146 (3.2, 060 max), build from ceebb596 (make amiga + wasabi + dist, rc 0):

- Full install `installer_rig.py ONLY fresh-all`: rc 0 (19:18:47).
- 19 stop/start boots (0..18), 18:47 to 20:57. Every boot: wasabi ping up within 1 s of rig start returning, UI tree
  clean (no AmigaDOS window, no System Request). Hangs: 0.
- Unfixed rate was 2 hangs in 9 boots. 19 clean boots at that rate happen by luck with probability about 0.8%
  ((7/9)^19).

Verdict: FIXED. The same ring is the likely cause of the rig 96 reboot hang (RIG96-REBOOT-HANG); that link is not
measured.
