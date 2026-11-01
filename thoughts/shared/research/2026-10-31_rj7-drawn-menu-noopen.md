---
title: RJ7-DRAWN-MENU-NOOPEN - drawn menu "no-open" is the hotkey toggle closing a menu left open
date: 2026-10-31
---
Rig 4, --max, fresh start, driver = s38 run.py logic (g.open_menu, walk Packages > Refresh, Esc at +4 s, Esc closes job view).

Reproduce: YES, 1 of 1 fresh boots; attempt 1 opened, attempt 2 (same boot) "failed" (menu window False). Attempt 3 opened. No feed server needed.

State, working vs failing attempt:
- Before attempt 1: no 'Amarchy' window, ports AMARCHY.MENU absent, one amarchy-gui process.
- After the job run + job-view Esc: window 'Amarchy' 360x332 at 220,134 STILL OPEN (Packages level: Browse|Search|Installed|Upgrades|Refresh), port AMARCHY.MENU registered, second amarchy-gui process alive.
- Before attempt 2 (failing): exactly that state. After LAmiga+Space: window gone, AMARCHY.MENU port gone, second amarchy-gui gone. So Space did not fail to open: it CLOSED the leftover menu (MENU-TOGGLE). Driver read "no window" as no-open and stopped.
- Attempt 3 Space: opens normally (root level, 360x504 at 220,48).
- Isolation: g.close_menu() (Esc until the window is gone) after a job, then Space: opened 2 of 2 (also 2 consecutive open/close cycles). Amarchy, hotkey commodity, AMARCHY.GUISRV/MENUSRV were alive in every attempt; nothing stuck.
Code: amarchy/amarchy.c:118 (a menu hotkey closes whichever menu is open), amarchy/menu_open.c (registry), amarchy/gui.c:1175 (Ctrl-C = toggle ends the menu), gui.c:1578 mo_register.
Cause: harness, not product. Esc in the job view returns to the Packages list; it does not close the menu. s38 drawn run 2/3 should have called g.close_menu() between runs. Text style likely behaves the same (not tested). Fix: add close_menu() at the end of run.py-style drivers (or before open_menu); no Amarchy change.
Probe scripts left in scratchpad (small): probe.py, p3.py, logs probe1-3.log.
