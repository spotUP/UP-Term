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

## Run 2 (s18, main 50d4040a + script fix; rig 7 started `--max`, start output: no [WARN] drift line; stage_rig "AmTile current", "Amarchy current")
Measured: front screen 800x600 (Workbench Screen, DOpus), title bar 14 (script default now right); master box (4,18,433,578).
Script bug fixed: close_win() clicked the close gadget by window title; all windows are titled "UP-Term", so it closed an arbitrary one
(first run closed the wrong window and left strays that made later steps count 5 windows). It now selects the window by address.
Final clean run (strays closed first, state.json removed), 11 of 11:
[OK] start: Amarchy up True, windows '0x1004f5e4' '0x1012196c', AMTILE port absent
[OK] row_on: expected row rc 0 and STATUS "on 2 n" n>=1 vs RUN 15 rc 0 STATUS (True, 2, 1)
[OK] boxes2: expected [(4, 18, 433, 578), (441, 18, 355, 578)] vs [(4, 18, 433, 578), (441, 18, 355, 578)]
[OK] second_on: expected rc 0, STATUS on vs rc 0 STATUS (True, 2, 2) (Status: 12 lines)
[OK] env_on: expected ENV:Amarchy/tile "on" vs 'on'
[OK] third: ... on cells True in 0.6 s, new window (4, 18, 433, 578), STATUS (True, 3, 3)
[OK] close_one: expected 2 windows on [(4, 18, 433, 578), (441, 18, 355, 578)] ... vs closed True boxes [(4, 18, 433, 578), (441, 18, 355, 578)] STATUS (True, 2, 4)
[OK] midapply: ... vs closed True STATUS (True, 2, 4)
[OK] off: expected row rc 0, STATUS off, ENV:Amarchy/tile "off" vs RUN 15 rc 0 STATUS (False, 2, 4) env 'off'
[OK] fourth_free: ... (0, 20, 800, 400, 'UP-Term') -> (0, 20, 800, 400, 'UP-Term')
[OK] restart: expected port AMTILE present, on, after Amarchy restart with ENV:Amarchy/tile "on" vs STATUS (True, 3, 1)

### Extras
Exchange (list rows Amarchy, AmTile, Directory Opus 5, Exchange): select AmTile, click Active:
 before STATUS (True,3,1) env on; after Disable (False,3,1) env off; after Enable (True,3,2) env on.  PASS
Hotkey Left Amiga + Left Alt + T (raw 0x14, qual 0x50): STATUS (False,3,2) env off, second press (True,3,3) env on.  PASS

### Regression (AmTile OFF then QUIT, port gone), amarchy_window_rig.py --fresh: passed 11 of 17
FAIL b2_redraw, b2_patterns (31 of 31 refused), b2_icons_selected, b2_viewname: `no port named "WORKBENCH"`. rexx_ports on rig 7:
 AMARCHY, AMARCHY.GUISRV, AMARCHY.MENUSRV, AREXX, ConClip.rendezvous, Directory Opus, DOPUS.1, REXX, WASABI, UPTERM.req.* -- no WORKBENCH port
 (DOpus replaces Workbench): rig environment, not the product.
FAIL menu_move_left: no menu window titled 'Amarchy'. `AMARCHY SHOW` rc 0 opens one window with EMPTY title, 608x352; the step looks for
 title 'Amarchy' (MENU_TITLE, window_rig.py:29), so likely a stale script title for the drawn menu. Not hunted further.
FAIL move_left: KeyError('box0') / box unchanged (72,40,320,120) with AmTile off: follows the menu_move_left failure (not hunted; it also failed in run 1).
