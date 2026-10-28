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
