# Menu gone after Keep: cause (THEME-PICK-MENU-GONE)

Verdict: CAUSE PROVEN. The menu window is reopened correctly by gui_show, then closes itself on IDCMP_INACTIVEWINDOW
(amarchy/gui.c:1452-1454) when the FP-1 "Amarchy desktop theme" requester takes the focus. Nothing reopens it after Keep, and
nothing is supposed to: the menu should have stayed open through the requester.

Build: s58-integ a0b023ce + probe sentinels (not committed), rig 150, OS 3.2, --max, `installer_rig.py ONLY fresh-all` [PASS].
amarchy-gui md5 on the rig == build/amiga (ede82e6d... probe 1, 92a8db74... probe 2 via ami.py put + reboot).

## Path of the pick (all in the resident server process, `amarchy-gui serve`)
1. serve (gui.c ~2040) -> gui_open -> event_loop returns with g->ixpath set (Return on a Style > Theme row).
2. gui.c:1976 lf_open_index -> leaf.c:538 lf_gt_select -> gthemes_dos.c:250 gt_dos_select -> gt_dos_apply_saved ->
   desk_dos.c:1124 desk_dos_apply -> dk_apply.
3. dk_apply (desk_dos.c:1314) dk_windows_close -> desk_dos_own_close = gui_hide (gui.c:1696): menu window closed.
   Palette editor, dk_wait_reset, then desk_dos.c:1340 dk_windows_open -> gui_show (gui.c:1710): window reopened (WA_Activate).
4. desk_dos.c:1150 dk_confirm_ask -> rexx_send "DESK CONFIRM <mask>" to the commodity. The commodity (amarchy.c:475 dc_begin,
   desk_confirm_dos.c:32 req_open) opens the EasyRequest "Amarchy desktop theme" on the Workbench screen; it becomes the active
   window. The requester and its 15 s timer live in the commodity, asynchronously; Keep (act, desk_confirm_dos.c:55) only runs
   desk_dos_confirmed (ENV: -> ENVARC:), it touches no window.
5. Back in the server: gui.c:1977 theme_live, `continue`, event_loop again with g->win set. The queued IDCMP_INACTIVEWINDOW
   (focus went to the requester) hits gui.c:1452: g->active_seen is still 1 from the first open, lf_exit_needs_ask is false on
   the theme list, so g->quit = g->stop = 1. The for loop breaks, cleanup closes the window, gui_open returns, serve waits.
   Server stays alive (matches menu-gone-measure.md).

## Candidates
- C1 gui_show returns early (screen_bind fails, or window_open returns 0): menu never reopened. REFUTED (MG_SHOW=4).
- C2 a pending Ctrl-C / Ctrl-F (WBRESET-EXT dismiss from the Workbench reset patch) ends event_loop: REFUTED (MG_QUIT never written).
- C3 the reopened window quits on focus loss (IDCMP_INACTIVEWINDOW) to the FP-1 requester: PROVEN (MG_INACT=10).

## Probe (sentinels via am_sentinel, global ENV:)
MG_SHOW in gui_show (1 not hidden, 2 bind failed, 3 window failed, 4 open); MG_LOOP = 10 + (g->win != 0) at event_loop entry;
MG_LIVE=1 after theme_live; MG_QUIT = Ctrl-C/F bits when a signal ends event_loop; MG_INACT = hidden + 10*(g->win != 0) at the
INACTIVEWINDOW quit.

Raw, pick miasma (probe 1, no MG_INACT yet):
```
keep miasma: requester seen yes at 10.0 s | Keep returned True at 10.0 s | requester gone yes
uitree Keep+5/+30/+60: "Amarchy bar", W 1 "", "f1host"; no "Amarchy" window
MG_SHOW=4  MG_QUIT=(not set)  MG_LOOP=11  MG_LIVE=1  AMARCHY_WBCLOSES=1  AMARCHY_DESK_APPLY_R=15
```
Raw, pick ristretto (probe 2, ENV:MG_#? deleted first):
```
keep ristretto: requester seen yes at 10.9 s | Keep returned True at 10.9 s | requester gone yes
[FAIL] select 3: no menu window after the pick (Keep answered)
MG_SHOW=4  MG_QUIT=(not set)  MG_LOOP=11  MG_LIVE=1  MG_INACT=10
```
(One pick before that, right after the reboot, failed earlier at "open menu" (cold first press, MG_QUIT=4096 from the harness's
close toggle); it does not count.)

Inferred, not measured: the window that took the focus is the FP-1 requester (it is the only window opened after gui_show,
~10 s after Return, and EasyRequests activate on open). Keep is not involved; the menu was already gone when Keep was clicked.

## Fix design (no code here)
- Violated invariant: "the menu stays open across a theme pick" (theme_live, owner 2026-10-28 / 2026-10-30) versus "a launcher
  closes when another window takes the focus" (gui.c:1452). The focus rule treats Amarchy's own apply-time windows (the FP-1
  requester, opened by the commodity because of this very pick) as a foreign program taking focus.
- Level: the menu's event model in gui.c (focus-loss close policy), not desk_dos or the requester. The requester is right to be
  modal-ish and active; the apply path already brackets the reset with gui_hide / gui_show.
- Fix: the focus-loss close must not fire for focus lost to the confirm the pick itself started. One correct shape: gui_show
  clears g->active_seen (the reopened window arms the focus rule only after it is active again), and the pick path keeps the
  rule disarmed until the FP-1 confirm the apply started has ended (the commodity publishes the confirm state, desk_confirm_dos.c
  publish(); the server reads it, or the requester's close re-activates the menu window), so a later real focus change to
  another program still closes the menu. Clearing active_seen alone is not enough: the reopened window does get ACTIVEWINDOW
  before the requester opens.
- Regression test: rig test through the real entry (amarchy_theme_save_rig.py --selects 1-1, the case that fails today with
  "no menu window after the pick (Keep answered)") asserting the "Amarchy" window exists after Keep and AMARCHY_THEME names the
  picked theme; plus a host test of the focus-policy function (if extracted to a core: inactive during an own confirm = stay,
  inactive otherwise = close) that fails before and passes after.
