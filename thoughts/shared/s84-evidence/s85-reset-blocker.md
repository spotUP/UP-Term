# S85 reset blocker, rig 84, measured 2026-11-15

Method: UPTERM_RIG=84, ami._m().read_ui(conn, every_screen=True) (wasabid UI op: window title, owner task, flags a/b) plus
ami.requesters(). Requester NOT re-triggered; it was still up. Flags BACKDROP shown, WBENCHWINDOW not exposed by this op.
Task = UserPort->mp_SigTask name as reported by wasabid.

Requester state: still open. "System Request" (Retry|Cancel), owner task "« IPrefs »", addr 0x10110274, at 0,0 279x101,
on the only screen "Directory Opus ..." 800x600 (the Workbench replacement screen).

Windows on that screen (front to back):
| title | task | backdrop | geometry |
| Patina bar | Patina | no | 0,587 800x13 |
| System Request | « IPrefs » | no | 0,0 279x101 |
| Device List | dopus_lister | no | 211,53 196x173 |
| Device List (second, identical geometry) | dopus_lister | no | 211,53 196x173 |
| (no title) | dopus_start_menu | no | 12,456 39x15 |
| (no title) | DOpus5:DirectoryOpus | yes | 0,14 800x573 (active) |

Suspects (non-DOpus, non-backdrop, non-IPrefs): only "Patina bar" (task "Patina"). It is excluded from Patina's own count
by title (win_core.c:194 desk_window_foreign-style filter), but Intuition's CloseWorkBench does not know that: the bar is a
real window on the WB screen and would block the close if it is open at reset time.
Other possible blockers: the two dopus_lister "Device List" windows (duplicate at identical position, odd) and
dopus_start_menu; Patina skips these (desk_task_dopus) trusting DOpus to close them on DN_CLOSE_WORKBENCH.
No window had "no UserPort" in the output (wasabid prints the task name only; a window with no UserPort not distinguishable).

## Race timing (rig 84, 2026-11-15, wasabid read_ui polls every ~0.03-0.25 s, t=0 at Retry click)

Step 1 (click Retry on the standing IPrefs System Request, addr 0x10110274):
- t=0.07 screen titled "Workbench Screen" (addr 0x100298d8) still has Patina bar, the requester, 2 dopus_lister, start menu, DOpus backdrop.
- t=0.13 requester gone; t=0.15 Patina bar gone; DOpus windows still open.
- t=0.20 a NEW System Request (addr 0x1011017c, task "-" then "« IPrefs »") appears with the DOpus windows still open. The requester DID come back.
- t=0.54 all windows gone (requester and DOpus windows); the screen stays up empty, titled "Workbench Screen".
- t=0.72-0.75 Patina bar reappears (new addr 0x1002b2d4) and vanishes again; it is NOT back at t=1.8 nor at t=9 (so ~0.7 s, not 6 s, and it did not stay).
- t=0.86-0.91 DOpus backdrop reopens (new addr), screen retitled "Directory Opus"; t=0.92 start menu; t=1.12-1.46 the two listers; "Device List" titles by 1.46. DOpus windows were absent ~0.35-1.1 s.
- No requester after t=0.54; final state at 9 s: no requester, no Patina bar, DOpus + 2 listers + start menu.
- Screen address 0x100298d8 never changed (screen object may be reused at the same address; not proven reset either way; palette not sampled).
- PATINA_DEFERRED (ENV: global, read via ami.read_file('ENV:PATINA_DEFERRED')): unset (None) before and after.

Step 2 NOT done: after the Retry run the rig's wasabid stopped answering (connect times out, 30-120 s retries, emulator still at ~53% CPU). Two key attempts were sent first
(ami.key(0x59,0x48): no visible effect in 8 s; then lamiga+ctrl+f10 via key_combo_events: the sampling client hung at once, daemon dead after). The freeze may be
caused by the second keypress apply (unproven). Rig left frozen, not restarted.

## Run 4: closewb retry (S85k)

Setup: rig 84 rebooted (`rig.py reboot --purpose "vtcon S85: closewb retry check"`, DH0 kept; reboot refreshed Patina 412908 bytes, 23:11, in VTC:Patina, VTC:Apps/bin/Patina, Rushhours:Tools/UP-Term/bin/Patina and UP-Term:bin/Patina, all verified 412908 on the rig). Two DOpus listers opened with ARexx `address DOPUS.1 'lister new'`. Trigger without keys: keymap.user row `super+shift+f9 = gtheme:tokyo-night` (row 72, picked up at once), then ARexx `PATINA RUN 72` (the same dispatch as the hotkey; no key_combo path, nothing wedged). Sampler: read_ui(every_screen) every ~0.1-0.3 s; t=0 is the start of the sampling loop, RUN sent at t=0.50.
Aside: the first boot of this run showed a standing IPrefs System Request before any RUN (left over from the frozen run's disk state); I cancelled it, which left Patina with its own System Request and no PATINA port. A second reboot booted clean (no requester at boot, bar present).

Timeline:
- t=0.02 screen "Directory Opus ..." addr 0x101092c8: 2 dopus_lister, dopus_start_menu, DOpus backdrop, Patina bar.
- t=0.50 RUN 72 sent. t=0.57 Patina bar gone (Patina closes it); DOpus windows still open.
- t=2.65 System Request (IPrefs, Retry|Cancel) appears; screen retitled "Workbench Screen"; the two listers, start menu and backdrop are STILL open. That is ~2.1 s after RUN: the 2 s retry window ran out with DOpus never having closed its windows.
- t=+8..25 s later: same screen addr 0x101092c8, System Request still up, listers/start menu/backdrop still open, plus a "Patina desktop theme" window and a re-created Patina bar. ENV:PATINA_DEFERRED unset before and after.

Verdict FAIL: requester left (appeared after the retry window), screen address unchanged (no reset), DOpus listers never closed/reopened, Patina bar present (re-created) but with the requester standing. PATINA_DEFERRED unchanged (PASS). The retry loop does not help here: during the 2 s DOpus did not close its windows, unlike run 1 where they closed ~0.4 s after the Retry click. Likely DOpus only closes on the failed return seen by ITS patch (DOpus patch sits above ours in the chain, or its close is driven by the IPrefs return code, not by time), so retrying inside our hook never gives DOpus the failure it waits for.
Rig 84 left running in this state (requester standing); do not click Cancel (it breaks Patina); Retry is the owner's next test.

## Run 5: DOpus close check (rig 84, 2026-11-15, no reboot, no source edit)

Tools: no Window->Flags / UserPort peek exists (wasabi has no memory-read op; read_ui gives title, task = mp_SigTask name, active/backdrop only; `ps` via ps_rows works). So WFLG_WBENCHWINDOW and UserPort==NULL could NOT be checked. Census = title + task. Scratch: scratchpad/cen.py, run.py, run2.py.

Census before touching (Workbench screen): "Patina bar" (task Patina), "System Request" (task « IPrefs », active), 2x dopus_lister, dopus_start_menu, DOpus5:DirectoryOpus backdrop. No process named dopus_closewb (dopus procs: dopus, dopus_clock, dopus_lister x2, dopus_rexx, dopus_start_menu).

Retry click (t=0.5 sent; the click op itself occupies wasabid ~0.8 s, so ps/ui sampling is blind from ~0.04 to ~1.4 s; second run: 0.04 -> 1.45):
- run A: t=0.77 requester gone, Patina bar still there; t=0.83 bar gone; DOpus 4 windows open; t=2.92 NEW System Request (new addr 0x10030bb4, task IPrefs) with DOpus windows STILL open; t=3.45 Patina bar back. DOpus did NOT close.
- run B (ps in a second thread): ps t=1.45 no dopus_closewb; UI t=1.35 DOpus windows open, only Patina bar gone; t=3.67-3.75 requester back, DOpus still open. No dopus_closewb seen at any sample.
Unlike run 1 (closed at 0.54 s) DOpus did not close within ~3 s after the Retry.

Apply (`PATINA RUN 72`, twice by mistake, second time via my mode "none"): returns rc 0, opens a "Patina desktop theme" window (task Patina, Keep|Revert gadgets) and recreates the bar; screen never closed, no CloseWorkBench-style sequence, requester stays, DOpus windows stay, no dopus_closewb in ps samples every ~0.07 s (the sampler did catch a transient dopus_pattern_stuff at t=1.19, so a 1 s-lived dopus_closewb would be caught when sampling is not blind). The state is not the clean path (half-applied, requester standing), so the apply here proves little.

Verdict: UNDETERMINED between A and B, leaning against (A) by census: at no sample did any window other than DOpus's own, the IPrefs requester (task « IPrefs » passes the rule) and Patina bar/Patina theme windows (Patina-owned, present only before Patina closes the bar) stand on the screen; the bar was gone (0.83 s, run A) while DOpus still did not close. dopus_closewb was never observed, but the 0.04-1.4 s window right after the Retry click, where it would live, is blind (click occupies the daemon). To decide: either a daemon-side op that reads Window->Flags/UserPort, or a sampler that does not go through the click (inject the click via a key/Amiga-side script) or an Amiga-side watcher (Forbid-free) logging the task list.
Rig 84 left: IPrefs System Request standing (addr 0x10030bb4), DOpus + 2 listers + start menu open, Patina bar + "Patina desktop theme" (Keep|Revert) window open; daemon answers. No screenshots taken.

## Run 6 (blocker census; rig 84, 2026-11-15, fresh reboot, Patina 413792 bytes)

Setup: `UPTERM_RIG=84 rig.py reboot --purpose "vtcon S85: closewb blocker census"` (reboot refreshed Patina 413792 in all four places). 2 listers via `address DOPUS.1 'lister new'`. First `PATINA RUN 72` did nothing (keymap.user was missing after the reboot: ENV:up-term/keymap.user absent, no sentinels, no window); wrote `[bind]` + `super+shift+f9 = gtheme:tokyo-night` via ami.put, RELOAD, then `PATINA RUN 72` (rc 0). Waited 10 s, then census.

Sentinels (ami.read_file ENV:, verbatim):
- PATINA_WBR_BLOCKERS = 0
- PATINA_WBR_BLOCKER = -
- PATINA_WBR_BLOCKER_TASK = -
- PATINA_WBR_TOP_OURS = 1
- PATINA_WBR_TOP = 273857592
- PATINA_WBR_SAVED = 273216832
- PATINA_WBPENS 0, PATINA_WBREFUSED 0, PATINA_DESKS 1, PATINA_DESKMASK 15, PATINA_WBCLOSES 2, PATINA_DEFERRED unset, PATINA_WBRESET_BUSY_WAIT unset.
- diag_vis / diag_wins / diag_ok are struct fields (rec), not env sentinels; not readable with the available tools.

Windows at +10 s (Workbench Screen): "Patina desktop theme" (Patina, active), "Patina bar" (Patina), dopus_lister x2, dopus_start_menu, DOpus5:DirectoryOpus backdrop. Window addresses all differ from before the apply (DOpus listers 0x1002b3fc/0x10030f2c became 0x10029c0c/0x100299c4; backdrop 0x105240ac... -> 0x100296ac): DOpus closed and reopened. No IPrefs System Request. No dopus_closewb process.

Outcome: close_hook ran, found no blocker (BLOCKERS 0), our gate was on top of the CloseWorkBench vector (TOP_OURS 1; TOP != SAVED as expected, SAVED is the original). This path is the clean path: the apply succeeded, DOpus windows closed and reopened. It does not reproduce the blocker, so it does not answer "which window blocks" (there was none this time). Difference from runs 4/5: fresh reboot, no earlier failed apply or standing requester. Rig 84 left up: theme applied, Keep|Revert window "Patina desktop theme" open.

## Run 7 (rig 84, 2026-11-16, census on, repeat 1 of 3)

Rig 84 had been stopped by the idle watchdog at 00:04:57 (15 min idle), so run 6's applied state was lost before the eye check. Rebooted with `--idle-minutes 60 --purpose "vtcon S85: DOpus theme apply eye check"`. keymap.user again missing after reboot; put back, RELOAD, 2 listers, `PATINA RUN 72`. At +10 s: no requester; BLOCKERS 0, TOP_OURS 1, WBCLOSES 2, DEFERRED unset. Clean path again. Rig left up for the owner's eye check (Keep|Revert window open).

Run 7 follow-up (owner, 2026-11-16): the IPrefs "System Request" (Retry|Cancel, "please close all windows except drawers") came up AFTER the +10 s check. Cause of the second reset: the "Patina desktop theme" Keep|Revert requester reverts after 15 s (desk_confirm_dos.c); nobody clicked Keep, so PATINA_CONFIRM_REVERTS_TIMEOUT = 1 and desk_dos_revert() ran a second Workbench reset. That second close failed with DOpus open: requester standing, both dopus_lister + start menu + backdrop still open, Patina bar present. WBR_* sentinels unchanged (identical values would be rewritten anyway: cannot tell whether close_hook ran on the revert). Added PATINA_WBR_CALLS / OK / RETRIES to close_hook to tell. Lead: the apply works, the revert (second reset within ~15 s, DOpus just reopened by its 1 s timer) fails; run 4 may have been a second reset too (earlier RUN in that boot unrecorded).

## Root cause (S85k, 2026-11-16, source-proven)

Two faults, both needed for the IPrefs requester:

1. Patina sent `dopus refresh all` while DOpus had closed its display for the reset. With `GUI->screen_pointer` null,
   `dopus screen` returns rc 5 (RXERR_INVALID_QUERY, dopus5 rexx_dopus.c:227-245) and an empty result. rexx_send
   returns rm_Result1 (5), but dopus_theme_send (patina/menu_theme.c) only stopped on -1, so the empty reply read as
   "DOpus on its own screen" and Patina sent `set palette` + `refresh all`. That runs main_reset -> display_open, which
   clears GUIF_CLOSED but not GUIF_CLOSE_PENDING (display.c:121). With CLOSE_PENDING stuck, event_loop.c:617 ignores
   every later DN_CLOSE_WORKBENCH, so DOpus never closed for the revert's reset.
   Fix: dopus_theme_send stops on any non-zero rc; desk_dos.c dk_dopus_theme waits (bounded, DK_RESET_SECS*4 polls of
   Delay 12) until `dopus screen` answers before sending the theme. Regression test: tests/test_keymap.c
   dopus_theme_send_goes_to_the_dopus_port, closed case (fails 13 vs 1 with the fix reverted).
2. DOpus closes its windows only after the first CloseWorkBench fails (its patch notifies it asynchronously). The
   close_hook retry (wbreset_patch.c, WBR_RETRY_TICKS 100, every 10 ticks) is load-bearing: confirm run with fix 1 but
   without the retry showed the IPrefs requester at t+22.3 s after the revert.
   Diagnostics (diag_blocker, PATINA_WBR_C* ENV) dropped; saved in s85k-wbr-diag-retry.patch.
