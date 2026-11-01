# IPrefs 3.1 reset path and the UP-Term fix (WB-RESET-SCREENNOTIFY, vtcon-05 s40)

## Path (evidence: thoughts/shared/research/2026-10-31_wbreset-external-use.md, "Reset entry", IPrefs 40.7 disassembly)
- IPrefs 3.1 does its own window check BEFORE any CloseWorkBench: LockIBase, walk the Workbench screen's FirstWindow
  list, count windows whose Flags lack WFLG_WBENCHWINDOW (0x02000000), UnlockIBase. Count > 0: its own "close all
  windows, except drawers" System Request (the string lives in IPrefs, not intuition). Only count 0 reaches
  CloseWorkBench (-78).
- So screennotify's CloseWorkBench patch (and its test client) never runs while a plain window is open: that is why
  the s39 registration changed nothing. No other vector carries the reset (lvoprobe counts in the research doc).

## Fix (commit 8b2d62ac)
- create_window opens the console window with WA_WBenchWindow when screennotify.library is present and the screen is
  the Workbench screen (decision: wbpark_flag_window in handler/wbpark.h). IPrefs then counts 0, calls CloseWorkBench,
  screennotify messages us, the handler parks (existing acd33353 path) and reopens on OpenWorkBench.
- Without screennotify the window stays plain (a flagged window nobody parks would make CloseWorkBench fail silently).
- Test: tests/test_wbpark.c flag_decision (decision table plus a scan that create_window asks it and adds the tag);
  `make test ONLY=wbpark` 16/0.

## Rig result
- Rig 2, 3.1 060 RTG, UP-Term window open: `SYS:Prefs/ScreenMode FROM VTC:screenmode.aga16.prefs USE` gave depth 8 -> 4
  with no System Request and the UP-Term window present afterwards (0,0 640x256). Copying a file into ENV:Sys does
  NOT notify IPrefs; the Prefs program with USE is the trigger.
- No revert control run on this rig (the s39 baseline with the same rig and the old handler showed the requester).
- 3.2: see ledger row.

## Gaps
- Tabbed consoles (ntabs >= 2) and tab hosts are not parked and not flagged on a later tab; a window opened plain then
  tabbed keeps the flag but is not parked: CloseWorkBench then fails and screennotify re-sends "opened".
- XCON:/foreign windows (WINDOW 0xaddr) are not ours to flag.

## s40 tabbed consoles (vtcon-05, 2026-11-01)
- Violated invariant: every window that carries WFLG_WBENCHWINDOW must close on a Workbench-close message, or CloseWorkBench
  fails and screennotify re-sends "opened". A tabbed host kept the flag (opened plain-flagged, later tabbed) but
  wb_eligible refused ntabs >= 2, so it never parked.
- Level of the root cause: the park decision (wb_eligible), one level above the window: the tabs share the host's
  window (tab_rp copies its RastPort layer), so parking had to let the tabs go too, not just exempt them.
- Fix (reuses the acd33353 path): wbpark_eligible has no tab count. wb_msgs HIDE calls wb_tabs_park (each tab, one at a time,
  TM_PARK answered before the window closes: tab closes its window parts, vtwin_unbind, unlocks the screen) then
  window_unbind as before. SHOW: window_rebind, then wb_tabs_unpark (TM_UNPARK with the new window: tab copies
  the RastPort, vtwin_rebind, menus, scroll bar), bar_draw, tab_show_active. Tabs themselves ignore the screennotify
  message (is_tab). Choice: parked, not "unflagged" (a tab opened flagged would need to lose a flag after open, impossible).
- DISK_INFO while parked: id_VolumeNode = NULL (wbpark_disk_window), no window opened (wb_is_parked covers the host and
  its tabs). Blocking until reopen would stall the caller inside the Workbench close, which waits for every client.

Rig result (s40, 2026-11-01): rig 2, `--os 31` then `--os 32`, UP-Term with a second tab (Left Amiga T), `SYS:Prefs/ScreenMode FROM VTC:screenmode.aga16.prefs USE`. Both: no System Request (requesters empty over 5 polls), screen reopens at depth 4, UP-Term window back at 0,0 640x256 on the new screen. On 3.1 the tab bar is drawn after the reopen (pixel check). Not verified: tab count after reopen (no text channel for the bar on the rig).

## s40 park timeout and tab count (2026-11-01)

- Invariant: the screennotify handler must never wait forever on a tab. `tab_sync` waited unbounded on `c->tab_reply`, so one stuck tab stalled the Workbench close inside IPrefs.
- Fix: `wbpark_wait` (handler/wbpark.h) polls every 2 ticks and gives up after 250 ticks (5 s). On timeout `wb_tabs_abort` posts TM_UNPARK to every tab, the window stays open (keeps WFLG_WBENCHWINDOW), the screennotify message is still replied, so CloseWorkBench fails and IPrefs reports it. A late reply is freed by the next drain.
- `tab_unregister`: no bound added. It answers incoming PARK/UNPARK while it waits, so it cannot deadlock a host in `tab_sync`.
- lock_screen: TM_UNPARK used the tab own spec (FRONT, default pubscreen), which can differ from the host window screen after a reset. It now uses `t->win->WScreen`.
- Tab count channel: reused `C:UPTerm state` (`tabs` counter), read with tools/rig/tabstate.py `counters()`.
- Rig 2 (3.1), tools/rig/wbtabs_rig.py: two tabs before the reset, two after `ScreenMode FROM VTC:screenmode.aga16.prefs USE`, both tabs run a marker command (mark-m1, mark-m2).
- Revert proof: with the HEAD handler, tests/test_wbpark.c:134 fails (`wbpark_wait(sync_got` missing); with the fix, 43 checks pass.
- Open: after the Workbench reopen the UP-Term window sits behind the Workbench window; only the top 11 rows show until a click at y=5. Candidate fix: WindowToFront after reopen. Not verified before the reset.
