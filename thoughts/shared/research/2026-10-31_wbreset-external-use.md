---
date: 2026-10-31
topic: Workbench reset started by another program (external prefs USE) with Amarchy windows open
status: design, implemented in the same branch
ids: WBRESET-EXT (follows R1.17-WBRESET, AMARCHY-31-PALUSE-REBOOT)
---

# Workbench reset from an external `Palette ... USE`

## Defect

`Palette FROM <file> USE` (also Font / ScreenMode / Overscan USE) makes IPrefs call
intuition `CloseWorkBench()`. It fails while the Amarchy bar (process Amarchy), the drawn
menu (amarchy-gui) or the text menu (amarchy-menu, an XCON: window) is open on the
Workbench screen, and IPrefs raises "Intuition is attempting to reset the Workbench
screen. Please close the following windows: Amarchy bar" (Retry | Cancel).
R1.17-WBRESET (38923fcf) only covers Amarchy's own desk apply (desk_dos_apply calls
desk_wb_close / gui_hide around its own write).

**Violated invariant:** an Amarchy window never blocks a Workbench reset. Amarchy closes its
windows before the Workbench closes and reopens them on the new Workbench screen.

**Level of the root cause:** the only point every reset passes through is the intuition
LVO pair CloseWorkBench (-78) / OpenWorkBench (-210). The NDK 3.1 / 3.2 has no public
Workbench-close notification (IDCMP_WBENCHMESSAGE is Workbench-private, screennotify.library
is third party), so the hook is ours to install, the way screennotify does it.

## Mechanism

1. **Owner: the Amarchy commodity process.** It runs for the whole desktop session, it
   owns the bar, and it is the process that sees Amarchy quit. At start it makes the
   public record `Amarchy.WBReset` (a SignalSemaphore plus a client table,
   `amarchy/wbreset_core.h`) and swaps the JMP operands of intuition's CloseWorkBench and
   OpenWorkBench library entries.
2. **Reused patch model.** The swap is `flat_swap_install` / `flat_swap_remove` and
   `flat_jmp_target` from flat_core.c (the amarchy-flat Text / RectFill patches: Forbid,
   JMP check, operand swap, SumLibrary, CacheClearU, restore only when the slot is still
   ours). The slot points to a 22-byte gate stub in AllocMem'd memory (`wbr_gate_build`,
   bytes pinned by the host test):
   `move.l hook(pc),-(sp); bne.s go; addq.l #4,sp; move.l saved(pc),-(sp); go: rts`.
   hook = a C function in amarchy/wbreset_patch.c taking `__reg("a6") struct Library *`
   (vbcc; no asm file), saved = the original routine.
   Clearing hook turns the gate into a pure pass-through.
3. **Clients** register task, signal mask, mode and "has a window on the Workbench" in the
   record (always under Forbid + FindSemaphore, so no client keeps a pointer to a record
   that may be gone; a restarted Amarchy is found again on the next update):
   - Amarchy (the bar): mode HIDE, signal Ctrl-F, has_win = bar_is_open(), refreshed every
     main-loop pass. Close = desk_wb_close(), reopen = desk_wb_open() (the R1.17 paths).
   - amarchy-gui: mode DISMISS, signal Ctrl-F, has_win updated after every change of the
     popup window (gui_open, gui_hide, gui_show, cleanup). event_loop treats Ctrl-F like
     the hotkey close: the popup goes away and is not reopened after the reset.
   - amarchy-menu (text menu, an XCON: console window): mode DISMISS, signal Ctrl-C (the
     MENU-TOGGLE close it already has); has_win set around Open / Close of its window. A
     console window cannot be hidden and shown again by the menu, so it is closed, not
     reopened; the next hotkey opens it on the new screen.
4. **Patched CloseWorkBench** (runs on the caller, normally IPrefs): `wbr_close_begin` sets
   phase CLOSING and lists the clients to signal (those with a window, never the caller's
   own task); the hook signals them and polls `wbr_pending` with Delay(2) up to
   WBR_WAIT_TICKS (3 s; no wait when the caller is not a process); then calls the
   original. `wbr_close_end(ok)`: success -> phase CLOSED (clients stay hidden until
   OpenWorkBench); failure (another program's window) -> phase OPEN and the hidden HIDE
   clients are signalled to reopen at once.
5. **Patched OpenWorkBench**: call the original; on success `wbr_open_end` sets phase OPEN
   and signals the hidden HIDE clients, which reopen on the new screen.

## Deadlock

- The hook never holds Forbid while it waits and never waits on the caller's own task.
- A client blocked on IPrefs (Amarchy or amarchy-gui inside desk_dos_apply) has already
  closed its windows (R1.17), so has_win = 0 and the hook does not wait on it. The 3 s
  timeout is the backstop for any other case: the original call then runs as before (the
  old requester at worst, never a hang).

## Removal at Amarchy exit

Forbid; clear both gates' hook (pass-through from now on); `flat_swap_remove` each slot;
RemSemaphore; Permit. Then wait (bounded) until no caller is inside the hook (busy
counter) and free the record. A gate whose slot is no longer ours (someone SetFunction'd
on top) stays allocated as an inert pass-through (22 bytes left by design, as flat's
still_chained).

## Tests

- Host: `make test ONLY=wbreset` (tests/test_wbreset.c on amarchy/wbreset_core.c): close,
  failed close reopens, open, self skip, timeout, dismiss clients, table full, gate bytes.
- Rig 8, 3.1 and 3.2: Amarchy with the bar up, `Palette FROM <prefs> USE`: no System
  Request, pens changed, bar back. With the fix reverted the requester returns.

## Step 1 measurement: which intuition vector does a Palette USE reset go through (rig 8, KS 3.1, 2026-10-31)

Probe: `tests/amiga/lvoprobe.c` (`make build/amiga/lvoprobe`) SetFunction-wraps a counting pass-through stub on every
intuition.library JMP vector (-30 .. -828, 138 vectors; only LockIBase -414 / UnlockIBase -420 left alone), on top of
the Amarchy hooks, and prints every vector called at least once after Ctrl-C. Amarchy was the commit-293a99d5 build
(md5 319c5dade578e04dd0a4346a80b5c8a2), started before the probe. The installed kit Amarchy on a fresh rig is an old
build without the patch (156912 bytes): copy build/amiga/Amarchy over UP-Term:bin/Amarchy first.

- Patch installed: semaphore `Amarchy.WBReset` PRESENT when the probe started (wbr_patch_install did install; its
  return code is still not printed anywhere).
- `SYS:Prefs/Palette FROM RAM:pal_red.prefs USE` -> System Request "Intuition is attempting to reset the Workbench
  screen" (task IPrefs, buttons Retry|Cancel), counts during that run:
  -78 CloseWorkBench = 0, -210 OpenWorkBench = 0, -324 SetPrefs = 0, -576 / -672 / -720 / -726 (intuitionPrivate1..4) = 0,
  all other nonzero vectors are the public ones Palette/IPrefs/Amarchy use (-510 LockPubScreen 1, -516 1, -522/-528 11,
  -600 SysReqHandler 106, -678/-684/-714 MakeClass/AddClass/FreeClass 2 each).
- After Cancel on the requester a second requester came (screen "Workbench Screen"); in that run -78 = 1,
  -576 = 3, -84 = 6, -600 = 167.

Verdict: the hypothesis "IPrefs resets the screen through a private vector (SetIPrefs) that we can wrap" is NOT confirmed.
CloseWorkBench is not called (hook never runs, as suspected), but no private or public vector shows a call that
carries the reset either: the reset code runs inside IPrefs' call into intuition by a route no vector counter saw.
The hidden vector is not in the 3.1 NDK FD (intuitionPrivate1..4 only). No fix chosen; next measurement: wasabi snoop
on IPrefs, or disassemble IPrefs / intuition.library to find the entry it jumps to.
