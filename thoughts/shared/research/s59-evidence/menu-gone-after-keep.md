---
date: 2026-11-06
topic: THEME-PICK-MENU-GONE - the Amarchy menu does not come back after a kept theme pick
status: HYPOTHESIS (code reading plus the rig 149 logs; no new rig run)
branch: s58-integ (eeed1323)
---

# Menu gone after Keep

## Measured facts (rig 149, pens47-keep-check.md and the keepcheck logs)
- While the FP-1 requester "Amarchy desktop theme" is up (09:42:29), uitree shows the screen titled
  "Workbench Screen" (a newly opened Workbench screen) with the requester, the bar, the Workbench backdrop
  and f1host. There is NO "Amarchy" menu window. The menu is already gone BEFORE Keep is answered.
- After Keep, at +5 s, +30 s and +60 s, Status lists amarchy-gui (process 10) as alive. The process has no window.

## Call path (all in the amarchy-gui process)
1. gui.c:1949-1990 main loop: a theme row calls lf_open_index (leaf.c:526), then lf_gt_select (leaf.c:513),
   then gt_dos_select (gthemes_dos.c:250), then amconf_save and gt_dos_apply_saved (:231), then desk_dos_apply.
2. desk_dos_apply (desk_dos.c:1123-1151) runs dk_apply. dk_apply closes the windows at :425
   (dk_windows_close calls desk_dos_own_close = gui_hide) and reopens them at :484
   (dk_windows_open calls desk_dos_own_open = gui_show). dk_flat_pens does the same pair at :750 and :756.
3. Only AFTER that does desk_dos_apply call dk_confirm_ask (:1150), which sends DESK CONFIRM to the commodity.
   The commodity's requester (desk_confirm_dos.c, BuildEasyRequestArgs) is non-blocking.
   So gui_show has already run when the requester is visible, and the menu still is not there.
4. gui_show (gui.c:1710-1730): `hidden = 0; if (screen_bind(g)) return; ... g->win = window_open(g);
   wbr_report(SIGBREAKF_CTRL_F, WBR_DISMISS, g->win != 0); if (!g->win) return;`.
   gui_show has no failure path back to its caller. It does NOT clear a pending CTRL_F.
   The first open does clear it (gui.c:1931 `SetSignal(0, SIGBREAKF_CTRL_F)`).
5. Back in the main loop, lf_open_index returned 0, so theme_live (gui.c:1526-1545) runs and `continue`
   re-enters event_loop. Nothing checks that g->win is non-NULL.
6. event_loop (gui.c:1403-1416) does `Wait((1UL << g->win->UserPort->mp_SigBit) | CTRL_C | CTRL_F)`.
   With g->win == NULL, this reads low memory (no MMU fault on the rig). The task then waits for ever,
   alive and without a window. That matches the Status lines.

## Causes considered
- A1 (leading): gui_show runs, but screen_bind (gui.c:1767, lock_front_screen or LockPubScreen(0)) or
  window_open (gui.c:1551, WA_PubScreen, WA_WBenchWindow) fails during or right after the Workbench reset.
  The new "Workbench Screen" is the sign of that reset. gui_show returns silently with g->win == 0.
  The caller then enters event_loop with no window (steps 5-6).
- B: a second Workbench reset after gui_show. dk_wbborder (desk_dos.c:881-918, called at :1133) can run
  dk_put("Workbench", ...) AFTER the windows are reopened, with no close/open around it. The commodity's
  reload (amarchy.c:435, triggered by amconf_save through the prefs notify) can also reset.
  The CloseWorkBench patch then sends CTRL_F (WBR_DISMISS) to the gui, and the menu quits.
  Against B: the process would EXIT, and Status shows it alive. B holds only if the exit hangs in cleanup.
- A2: the gui is still inside the apply. Unlikely: DESK CONFIRM is sent only after the reopen.

## Product or harness
Product. The harness expectation (tools/rig/amarchy_theme_save_rig.py:78-85: the menu reopens on the same
level) matches the product intent (gui.c:1694 comment, theme_live and THEME-ENTER-BACK-ONE).
The harness comment in one_select (:330-370) that the apply "blocks on the FP-1 requester" is wrong:
the requester is non-blocking, and the reopen comes before DESK CONFIRM. Correct that comment with the fix.

## Violated invariant
event_loop runs only with an open window: after gui_hide, gui_show either reopens the menu or the menu ends cleanly.

## Root cause level and fix design (not written)
Level: the amarchy-gui hide/show hooks and their caller (gui.c), not the harness.
- gui_show returns a result. It retries screen_bind and window_open until the public Workbench screen is
  open again (bounded, like dk_wait_reset's 6 s), using the screen that IPrefs reopened.
- gui_show clears a stale CTRL_F before the reopen, like the first open at gui.c:1931.
- The main loop (gui.c:1976-1979) checks g->win before `continue`. No window: open again or end cleanly
  (exit_ok path), never event_loop with NULL.
- desk_dos_apply puts dk_wbborder's Workbench USE inside the same close/open pair as dk_apply
  (one reset per apply, windows closed across all of it).
- Regression test: a host test where the window open fails once on show; the gui must not enter
  event_loop with no window.

## Smallest deciding measurement
On rig 150 after one kept pick with the menu gone: `Status` (note the amarchy-gui number n), then
`Break n C`, then `Status`.
- The process ends: it was in event_loop's Wait on a NULL window (A1 proven).
- The process stays: it hangs elsewhere (A2, or B with a hung cleanup).
Optional second read: wasabi task state and the signal-wait mask of amarchy-gui (tc_SigWait includes
CTRL_C|CTRL_F plus a bogus port bit under A1).

## Rig
No rig was run. The rig budget did not cover install, pick and stop. No test-register row added.
