---
date: 2026-11-02
topic: RL15 IPREFS-SYSREQ - IPrefs "System Request" on a Workbench reset during an Amarchy apply
status: open - cause not reproduced, no fix committed
base: origin/main 4ece6785
rig: shared rig 91 (OS 3.1, exec 40.10, Directory Opus 800x600x24 as Workbench screen)
---

# IPrefs "System Request" (Retry | Cancel) during Amarchy settings writes

Requester text (from RIG-IPREFS-SYSREQ, todos.md:3002, and R1.17-WBRESET, todos.md:3105):
"Intuition is attempting to reset the Workbench screen. Please close all windows, except drawers."
3.2 variant: "... Please close the following windows: Amarchy bar".

## Amarchy writes that can make IPrefs reset the Workbench screen

All go through dk_apply (amarchy/desk_dos.c:359-466), reached from gt_dos_select (amarchy/gthemes_dos.c:206,
desk_dos_apply at :225), called from the main process (amarchy.c:371, KA_GTHEME) and from the gui/menu process (leaf.c:370).

| Target | Write | Resets WB? | Own windows closed first? |
|---|---|---|---|
| DT_PALETTE (3.2 only, gated by desk_gate_mask) | palette.prefs via SYS:Prefs/Palette | yes when desk_pens_reset() | yes, `reset` (desk_dos.c:396) |
| DT_WBPATTERN | wbpattern.prefs via SYS:Prefs/WBPattern | 3.1 yes, 3.2 measured no | 3.1 yes, `old_ws` (desk_core.c:1238) |
| DT_ICONTROL (3.2 only) | icontrol.prefs in place, ENVARC+ENV (dk_icontrol, desk_dos.c:136-166) | yes | yes, `old_ws` |
| DT_FONT | font.prefs, WBFONT chunk only (desk_font_pens / desk_font_name, desk_core.c:708-733) | not seen (rig 91 test below) | only when another target set reset/old_ws (desk_dos.c:448) |
| DT_POINTER | pointer.prefs | no | no |
| flat pens palette write (dk_flat_pens, desk_dos.c about 698-704) | palette.prefs | yes when pens change | NO window close: only dk_wait_reset + dk_wait_iprefs |
| restore / revert / start_apply (desk_dos.c:1206/1222, 1250, 1329/1367) | several | yes | yes |

Window owners: bar (Amarchy main, closed by desk_dos_wb_close or "BAR OFF"), amarchy-gui window (gui_hide via
desk_dos_own_close, gui.c:1672), amarchy-menu popup, theme window (themewin.c, bar.c:504). note.c opens no window.
External resets are handled by the WBRESET patch (wbreset_patch.c close_hook about 53-82): it signals every client
with a window except the calling task, waits WBR_WAIT_TICKS=150 (3 s), then calls the original CloseWorkBench.

## Candidate causes (not confirmed on a rig)

1. dk_flat_pens writes palette.prefs on 3.2 with no dk_windows_close around it, so IPrefs resets WB while the bar is
   open. close_hook signals the bar, but when dk_flat_pens runs in Amarchy main, main is blocked in dk_wait_reset and
   cannot service its Ctrl-F; the 3 s wait expires and the original CloseWorkBench fails with the requester.
2. Same self-deadlock for any apply that runs inside a process that still owns an open window (gui or menu process,
   leaf.c:370): close_hook skips only the calling task (IPrefs), and the applying task is busy.
3. Ledger history (todos.md:3324-3333): the menu popup stayed open after a 3.2 border apply.

## Rig 91 measurements (2026-11-02, text only)

- Before: `W 0 0 587 800x13 - "Amarchy bar"`, `W 2 0 14 800x573 active ""` on screen "Directory Opus ..."
  (DOpus is the Workbench screen on this rig). Processes: Amarchy, amarchy-gui, amarchy-menu.
- WBFONT pen change (front 1 to 2, back 0 to 3) fed through `SYS:Prefs/Font FROM RAM:fontA.prefs USE` with the bar
  open: ENV:Sys/font.prefs changed (hex row 0030: `00000203 0100...`), no requester, bar stayed open.
  The original font.prefs was fed back afterwards. So a DT_FONT write alone is not a trigger on 3.1.
- WBPattern USE with the unchanged ENV file: no requester, no reset (same content, IPrefs did nothing). A changed
  pattern was not tried.
- No requester could be produced on 3.1. The reported cases (palette, icontrol, flat pens) are 3.2 paths and need a
  3.2 rig (`rig.py start --max --os 32`), which this hunt was not allowed to start.

## Open

- Reproduce on a 3.2 rig: flat-pens apply and border (icontrol) apply with bar, gui and menu open; capture
  `ami.py uitree` and `ami.py requesters`.
- If candidate 1 holds: put dk_flat_pens' palette write inside dk_windows_close/dk_windows_open like dk_apply's
  `reset` path, decided by a desk_core function with a test in tests/test_desk.c.
- If candidate 2 holds: the apply must close its own process's window before writing, or close_hook must not wait on
  the task that started the apply.
