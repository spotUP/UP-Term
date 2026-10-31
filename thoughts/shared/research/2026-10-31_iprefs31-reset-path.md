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
