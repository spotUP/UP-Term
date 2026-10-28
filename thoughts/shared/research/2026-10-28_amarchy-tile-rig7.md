# AmTile reachability on rig 7 (P4-2), 2026-10-28

Rig 7 started `--max --purpose "Amarchy s18: autotile reachability"`; no [WARN] drift line in start output ("refreshed keymap 18528 bytes", "up"). Staged by the script (stage_rig): AmTile restaged, Amarchy current.

## Script bug fixed
tools/rig/amarchy_tile_rig.py front_screen(): wasabi `list_windows` returns a tuple (screens, raw lines); the script indexed it as a list
(TypeError 'list indices must be integers'). Fixed by unwrapping the tuple.

## Run 1 (before fix)
[FAIL] start: exception TypeError('list indices must be integers or slices, not str')
[FAIL] row_on: expected row rc 0 and STATUS "on 2 n" n>=1 vs RUN 38 rc 5 STATUS None
[FAIL] boxes2: exception TypeError(...)

## Run 2 (after fix)
[OK] start: Amarchy up True, windows '0x1010a204' '0x10111acc', AMTILE port absent
[FAIL] row_on: expected row rc 0 and STATUS "on 2 n" n>=1 vs RUN 38 rc 5 STATUS None
[FAIL] boxes2: expected [(4, 15, 433, 581), (441, 15, 355, 581)] vs [(0, 20, 800, 400), (0, 20, 800, 400)]
Remaining steps not run (budget; row_on is the entry point).

## Findings
1. `RUN 38` (script's row index, "super+1..9" counted as nine) returns rc 5: cx_row(38) is null/not accepted. Amarchy STATUS says rows=31
   (installed rows). dist/keymap [bind] index of the AmTile row counted one line each = 30. `RUN 30` returned rc 0 (hits=1) but no AmTile process
   started and no AMTILE port appeared. Unresolved: which index reaches the row (or whether act_run's async start of
   `UP-Term:bin/AmTile TOGGLE` fails on the rig). Needs a look at how the handler/keymap parser expands 1..9 vs cx_row.
2. `UP-Term:bin/AmTile TOGGLE` run in the foreground does not return (commodity); `Run >NIL: <NIL: UP-Term:bin/AmTile ON` works:
   port AMTILE answers `on 0 0`, ENV:Amarchy/tile is "on".
3. PRODUCT QUESTION (not fixed): with AmTile on and two UP-Term windows open, STATUS stays `on 0 0` and RETILE does not move them (still 0,20,800,400).
   tile.c term_screen() locks the public screen named "UP-Term" only. In the first window list (run 1) the only screen was "Workbench Screen"
   (DOpus), the two XCON windows (title "UP-Term") sat on it, so nothing is tiled. Later the window rig opened an "UP-Term" screen, so the
   result depends on which screen the windows are on. AmTile on the Workbench screen (A1200 DOpus case) tiles nothing.
4. Title bar height: not determined (run did not reach a box compare). Hint: the backdrop DOpus window on the Workbench screen has top 14
   (screen BarHeight+1 = 14), UP-Term window border top 14; default 11 in the script is probably wrong (tests use 12).
5. Extras (Exchange Disable/Enable, super+alt+t): NOT run.

## Regression: amarchy_window_rig.py
Not clean: passed 11 of 17. Fails: menu_move_left (no menu window titled 'Amarchy'), move_left (KeyError box0 / box unchanged while AmTile was
running), b2_redraw, b2_patterns, b2_icons_selected, b2_viewname (`no port named "WORKBENCH"`: this rig has no Workbench port, DirectoryOpus
runs). The WORKBENCH failures look environmental (not the win_apply refactor) but I had no baseline from before the refactor.
AmTile was running during the first run (my manual start), which can explain the move_left failure; second run used recorded verdicts.
Verdicts: build/rig7/amarchy-window.
