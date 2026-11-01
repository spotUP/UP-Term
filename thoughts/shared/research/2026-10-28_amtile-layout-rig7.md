# AmTile layouts on rig 7 (Phase 6), 2026-10-28

Rig 7: `UPTERM_RIG=7 rig.py start --max --purpose "Amarchy s20: AmTile layouts"`; no [WARN] drift line. `make wasabi` (562672a8), AmTile and Amarchy
already current. The start refreshed AmTile (64332), Amarchy (164540) and the keymap (20780) onto the rig itself, so `rig.py install`
(which copies the handler) was not run. md5 on the rig vs build: AmTile 7c0346e935d3aee95833f6bd535ecb43 = build; Amarchy
90600edf08ee93798e0f7f2497489448 = build.
Command: `UPTERM_RIG=7 python3 tools/rig/amarchy_tile_rig.py --screen 800x600 --title 14` (all 29 STEPS).

## Run A: full run in one go, 17 of 29 OK
OK: start, row_on (RUN 15), boxes2, second_on, env_on, third, close_one, midapply, layout_default, layout_row (RUN 16, dwindle -> master-stack,
retiles 2 -> 3, boxes [(4,18,433,578),(441,18,355,578)] = helper), lay_next_prev, promote, persist (grid 2 60), reset, off, fourth_free, restart.
FAIL (want vs got, from the script's own message): layout_hotkey, lay_dwindle, lay_master, lay_mirrored, lay_columns, lay_rows, lay_grid,
lay_monocle, ratio, nmaster, cycle, monocle_raise. In each box failure STATUS (layout, tiled, retiles) was correct and the got list held a
duplicate of an old box, e.g. lay_dwindle want [(4,18,433,578),(441,18,355,315),(441,337,355,259)] got [(4,18,433,578),(4,18,433,578),(441,337,355,259)].
After the run 4 UP-Term windows were on screen (script tracked none: state.json empty after restart step), so windows leaked from the earlier steps.
Direct check at the end of run A with those 4 windows: LAYOUT mirrored / columns / dwindle gave exactly the helper boxes for n = 4.

## Run B: rig restarted, steps rerun one by one (--only), 2 windows then 3
OK in isolation: layout_hotkey (super+alt+l: master-stack -> mirrored, retiles 3 -> 4, boxes [(4,18,355,578),(363,18,433,578)] = helper),
lay_dwindle, lay_master, lay_mirrored, lay_columns, lay_rows, lay_grid, lay_monocle (all three boxes (4,18,792,578)), lay_next_prev,
ratio (ratio 60, boxes [(4,18,472,578),(480,18,316,344),(480,366,316,230)], RATIO 90 rc 10), nmaster (nmaster 2, 4 boxes = helper, NMASTER 0 rc 10).
FAIL in isolation: promote (was OK in run A; run B started it with nmaster 2 and 4 windows left by the nmaster step: "was (4,309,433,287) now None"),
cycle and monocle_raise (fail in both runs).

## Failing in both runs: cycle, monocle_raise
cycle: rcs [0,0,0,0], start None, front ['0x1011d6dc' x3] (run B: '0x102f4464' x3). monocle_raise: [False, False].
Probe check (monocle, 3 windows, wasabi list order and topmost()): the window list order never changed, not after `CYCLE NEXT` and not after
the script's own `activate()` (ui_front plus click) either: topmost() returned 0x102f4464 after activating each of the three windows in turn.
So topmost() cannot tell the layer order here, and the same blindness hides whether CYCLE/monocle raise work. Verdict: UNPROVEN either way.
No product fix, no script fix made (the evidence does not prove the script wrong or AmTile wrong). All CYCLE/LAYOUT commands returned rc 0.
Needs: a front-window probe that reads Intuition layer order, or a visual check of a monocle stack with distinguishable windows.

## Open issues for the script (not fixed)
1. Full-run window leak: layout_hotkey onward failed in run A but passes in isolation; the steps from midapply/close_one leave windows the script no longer tracks.
2. Step order dependence: promote needs nmaster 1 and exactly the tracked windows.

Result: full run 17 of 29; best per step across both runs 27 of 29 (cycle, monocle_raise open).

## Run 2 (rig 7, rig script fixed): full run 29 of 29 OK
Command: `UPTERM_RIG=7 python3 tools/rig/amarchy_tile_rig.py --screen 800x600 --title 14 --no-build`; 0 steps failed (all of start .. restart PASS).
Intermediate full runs: first fix 19/29 (layout_hotkey..nmaster still failed), after open_win + prefs fix 28/29 (promote), final 29/29.

Defect A, causes found (all in the script, no product change):
1. open_win() took `ami.open_window()[0]`; that diff keys on (addr, task, title), so an old window whose task name changed (or a
   Workbench window) counted as new. Seen: after midapply STATE held a gone address while a real UP-Term window (0x1028dc44) was untracked.
   Fix: open_win() diffs UP-Term addresses itself (before/after) and tracks exactly the one new address.
2. close_win() dropped the address from STATE even when the click closed nothing. Fix: it waits for the address to leave the window list.
3. known_window_set() runs before every step: forgets dead addresses, closes any other UP-Term window on the front screen.
4. Prefs persisted by a previous run (tile-layout mirrored) changed the boot layout of the next run (boxes2 failed): strip_tile_prefs() now
   also runs in s_start (shared with layout_default).
promote (pass in full run A, fail isolated, then fail once in run 2): STATUS flips to nmaster 1 before the windows move, so the step read the
target's old box (nmaster-2 cell (4,309,433,287)) and the PROMOTE fired against a layout still settling; in run A the timing happened to fit,
isolated runs started from stale state.json addresses. Fix: wait for the settled master-stack nmaster-1 cells (tiled_boxes_match) first.
Result: promote now "was (441,18,355,287) now (4,18,433,578)" = master box.

Defect B, probe: wasabid WINDOWS walks `Screen->FirstWindow/NextWindow` (wasabid.c:2603) and the protocol says "front first", but on rig 7
that order never changed after WindowToFront/CYCLE, so covered() over it is blind. No memory-read or layer command exists in wasabid
(PROTOCOL.md: commands PUT GET RUN SLOTS FREE LS DEBUG SNOOP PS KILL GRAB SCREEN MOUSE KEY WINDOWS UI UIACT AREXX PORTS SUM INPUTS LIVE CLIP HEALTH),
so the Screen.LayerInfo chain cannot be read without changing wasabid. Probe used instead: topmost() left-clicks the centre of the
stack and reads the active flag. Intuition delivers the click to the layer that WhichLayer finds under the pointer (the topmost) and activates
that window, without changing depth order; the WINDOWS listing carries the active flag ('a'). So the active window after the click is the layer top.
cycle: rcs [0,0,0,0], start 0x1012146c, front after NEXT x3 [0x10107b34, 0x100f7df4, 0x1012146c] (three different, back at the start),
PREV -> 0x100f7df4 (= second). PASS. monocle_raise: [True, True] PASS. Caveat: activate() uses ui_front (WindowToFront) + click, so
monocle_raise proves the probe sees the raise, not that AmTile raises on activation by itself (a monocle window cannot be activated
without raising from the script because the others cover it). No AmTile bug found.
Final state: 4 UP-Term windows on the rig after the run (the fourth is the untiled fourth_free window); the next run's known_window_set closes them.
