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

## Reset entry (s28 opus hunt)

Rig 8, KS 3.1: intuition.library 40.85, SYS:C/IPrefs 40.7 (06/02/93), workbench.library 40.5. Amarchy = the 293a99d5
build (248304 bytes, md5 319c5dade578e04dd0a4346a80b5c8a2) copied over UP-Term:bin/Amarchy.

**Requester body (3.1, read from a grab of the window):** "Intuition is attempting to reset the Workbench screen.
Please close all windows, except drawers." (Retry | Cancel). It names no window. The string is in SYS:C/IPrefs itself
(`strings`), not in intuition: on 3.1 IPrefs owns this requester. (The 3.2 text that lists "Amarchy bar" is a
different, intuition-side check.)

**Directory Opus:** the rig's master image replaces the Workbench with DOpus 5 (C/LoadWB starts it, Libs/workbench.library
replaced; see rig.py `sys_hdf_no_dopus`; tasks `dopus`, `dopus_start_menu`, DO_LAUNCHER; the screen title
"Directory Opus ..." is DOpus' backdrop window title on the Workbench screen). It is not the blocker.

**Trigger:** only a change of the pens resets the Workbench; a colour-only USE does nothing (no reset, no requester).
Reproducible pair: RAM:pal_red.prefs and RAM:pal_pen.prefs (pal_red with TEXTPEN 1 -> 2 at file offsets 0x36 and
0x76), USEd alternately. Matrix:

| DOpus | Amarchy bar | Requester | Reset happened |
|---|---|---|---|
| running | open | yes (IPrefs) | no |
| running | stopped | no | yes (dopus_start_menu window reopened at a new address) |
| stopped | open | not run (needs a `--no-dopus` rig) | - |

**Call path (IPrefs 40.7 disassembly, m68k-amigaos-objdump, hunk offsets):**
1. Prefs notify -> IPrefs reads ENV:Sys/palette.prefs, opens workbench.library (snoop).
2. Window check at 0x48a: `LockIBase` (-414), find the Workbench screen (bsr 0x418), walk `scr->FirstWindow`, count
   every window whose `Flags` lacks `WFLG_WBENCHWINDOW` (`btst #1,24(a2)` = bit 0x02000000) except its own requester
   (`cmpa.l a3,a2`), `UnlockIBase` (-420).
3. Count > 0: `BuildEasyRequestArgs` (-594) and a polling loop on `SysReqHandler` (-600) that re-runs the window
   check each pass (0x53e..0x56e); when the count reaches 0 it proceeds by itself; Cancel gives up.
4. Count 0: `FreeSysRequest` (-372), then `CloseWorkBench` (-78) at 0x59c and, on success, `OpenWorkBench` (-210).

This explains step 1: CloseWorkBench is never reached while a non-WBENCHWINDOW window is open, and the probe skipped
LockIBase/UnlockIBase, which carry the check. DOpus passes because its windows carry WFLG_WBENCHWINDOW.

**Hook point:** keep the CloseWorkBench/OpenWorkBench gate (that is where 3.2's intuition-side check and every other
reset caller arrive) and make 3.1 IPrefs reach it: Amarchy's own Workbench-screen windows (bar, drawn menu) carry
WFLG_WBENCHWINDOW, so IPrefs 40.7 counts them as Workbench windows, calls CloseWorkBench, and the gate closes them
before the original runs and reopens them in OpenWorkBench. The XCON: text menu window cannot be flagged by us (the
handler opens it); it needs the same flag set by the handler or stays a blocker on 3.1. Not verified on the rig yet.

Open risks: (1) WFLG_WBENCHWINDOW windows may be treated as Workbench's own by workbench.library/DOpus (window list,
"close all", AppWindow paths); measure before choosing it. (2) The alternative without the flag is a DOS notify on
ENV:Sys/{palette,font,screenmode,overscan,wbpattern}.prefs, which races IPrefs. (3) wasabi's last guru on this boot:
RECOVERABLE ALERT #01000003 (exec: library checksum failure) in task UP-Term:bin/Amarchy: the vector swap likely
writes the jump table without LIBF_CHANGED before SumLibrary; check flat_swap_install / wbr_patch_install.

## Result of the WA_WBenchWindow flag on rig 8 (subagent run, 2026-10-31)

Change: gui.c window_open and themewin.c tw_open (the bar) open with WA_WBenchWindow. Host lint test test_wb_window_flag guards it.

- 3.1 without DOpus: three Palette USE pen changes with the bar open gave no System Request. Bar stayed alive (draws and ticks kept rising). No Guru recorded. Evidence for the reset is indirect: the Workbench window address changed on some runs; no CloseWorkBench counter was read.
- 3.1 with DOpus: FAIL. After every pen-change USE a System Request from task IPrefs (Retry, Cancel) stays open at 0x10050d24 on the Directory Opus screen. The bar (0x10090884) stays open, so the reset is blocked. BAR OFF did not clear it. Some other non-WBENCHWINDOW window is on that screen, or the flag is not honoured there. Unnamed windows seen at 0x101200e4 and 0x100fc454. Contradicts the earlier claim that DOpus windows carry the flag. Not resolved; no further mechanism tried.
- 3.2 rig (--os 32): not run (budget).
- Remaining: XCON: menu window is opened by the handler and is not flagged.
