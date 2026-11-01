---
date: 2026-10-31
topic: WBRESET-EXT - palette change with Amarchy running, 3.2 and 3.1
status: fixed on branch worktree-agent-a671f8f6ff1f85944
---

# WBRESET-EXT: Workbench reset with Amarchy running

## Symptom
`Palette ... USE` (or any pen change) with Amarchy running: IPrefs shows "Intuition is attempting to reset the
Workbench screen. Please close the following windows" listing "Amarchy bar".

## Causes
1. 3.2 (rig 9): the main build has no CloseWorkBench gate; only R1.17 closes the bar around Amarchy's own desk
   apply. An external pen change hits CloseWorkBench with the bar open. The branch gate (amarchy/wbreset_patch.c
   close_hook: signal clients, wait up to 3 s, call the saved vector; open hook reopens) fixes it.
2. 3.1 with Directory Opus (rig 9, `--os` default): the gate alone still failed 5 of 5 tries. Probe (diag fields in
   wbr_rec, printed by tests/amiga/wincensus): at the original call, 0 windows and 1 visitor on the Workbench screen.
   IPrefs' first try fails while DOpus closes its windows; the gate then tells the bar to reopen; bar_open takes the
   screen lock at once and reports its window only after ~0.5 s CPU calibration. IPrefs retries at once: the bar's
   has_win was still 0, so the retry neither signalled nor waited and ran into the half-open bar's lock. Every retry
   repeated it. With BAR OFF the same USE succeeded on the 3rd try (diag_vis 0, diag_ok 1).
   Fix: amarchy/wbreset_core.c wbr_client_step, DO_SHOW branch sets has_win = 1 (the client holds the screen from
   its first lock on); its own report sets the real value after the open.

## Regression test
tests/test_wbreset.c test_retry_while_reopening: 75 checks pass; with the fix line removed 2 fail
(test_wbreset.c:94 wbr_close_begin = 0 want 1, :95 wbr_pending = 0 want 1).

## Rig results (build Amarchy md5 3aed5bda5d0109c3f2ee5b850682b732)
- 3.1 rig 9, DOpus 5 as Workbench, bar + amarchy-gui + amarchy-menu running: pal_a, pal_b, pal_a USE, then pal_b
  with the drawn menu open. 4 of 4: no IPrefs requester, dripens SHINE alternates 2/0/2/0, bar window reopened
  (new window address each time), menu dismissed, Amarchy still running. Gate counters: closes 16, failed 8 (DOpus
  first tries), opens 4, timeouts 0.
- 3.1 before the fix (same rig): requester every time, counters closes 5 failed 5 per USE, diag_vis 1 diag_wins 0.
- 3.2 rig 9 with the gate (before the 3.1 fix): pal_a, pal_b, pal_b with drawn menu open: no requester, pens changed,
  bar back. 3.2 has no failing first try, so the retry race does not arise there.
