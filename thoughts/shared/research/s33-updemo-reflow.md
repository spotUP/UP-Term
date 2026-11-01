# UPDEMO-REFLOW-RESIZE (session 33)

Branch fix/updemo-reflow-resize, worktree /Users/spot/Code/UP-Term/vtcon-reflow.

## Invariant and root-cause level (written before the edit)

Invariant: a window size change is an input to the demo, never a reason to end it. The reflow scene
asks the user to resize; the demo has to follow any size the user picks.

Level: the demo's scene loop (demo/updemo.c play()), not the handler.

## Cause 2 (demo ends after the resize)

demo/updemo.c play(), before each scene: `if (ask_size() && !set_size(ev_cols, ev_rows)) break;`
set_size() returns 0 for a window under UPDEMO_MIN_COLS x UPDEMO_MIN_ROWS (76 x 20), and the break ends
the whole tour. The user obeys "make it narrower", the window is below 76 x 20, the next scene's size
question fails the minimum and the demo exits. The old test `a_window_made_too_small_ends_the_demo`
pinned this as intended. The resize event itself is fine: it arrives as the CSI 8;rows;cols t answer
(EV_SIZE in read_event), not as a signal, EINTR or a quit key.

Fix: follow_size() (demo/updemo.c). A too small window shows "make the window bigger, 76 x 20 needed,
Q quits", asks the size again about five times a second and goes on with the next scene when the window
is big enough; Q, Esc, Ctrl-C end the demo from there.

## Cause 1 (window can only be made smaller)

Not confirmed on a rig: see "Open" below. What the code shows: the handler opens windows with
WA_MaxWidth/WA_MaxHeight = ~0 (handler/vtcon_handler.c:2955), so there is no limit there. The tab
window is the host window, grown by tab_spawn via vtwin_set_size (ChangeWindowBox, clamped to the
screen). A window already as big as the screen can only be made smaller, and with cause 2 the demo then
ended before the user could grow it back, which matches the report. The program has no way to ask for
a smaller window (the engine ignores CSI 8 t from the program).

## Tests

- tests/test_updemo.c: `a_resize_in_the_reflow_scene_does_not_end_the_demo` (window made 60 x 20 during
  the reflow scene, back to 80 x 24 later: the scene after the reflow one must have drawn frames),
  `a_window_made_too_small_waits_and_q_ends_it` (replaces the "ends the demo" test).
- tools/rig/demo_rig.py `resize` : reflow scene, drag the size gadget smaller then bigger
  (winch_rig.drag), demo still runs, window grew again, next scene animates.

## Results and commits
See the end of this file (filled when the runs finished).

## Results
- Host test `make test ONLY=updemo` with fix: `[OK] updemo: 8452 checks, 0 failed`.
- Fix reverted (demo/updemo.c only): `FAIL tests/test_updemo.c:462: frames > 0`, `FAIL tests/test_updemo.c:463: vt_cols(t) == 80`, `[FAIL] updemo: 8452 checks, 2 failed`.
- Commits on fix/updemo-reflow-resize: see `git log` in /Users/spot/Code/UP-Term/vtcon-reflow (3 commits: fix, test, rig check).

## Open
- tools/rig/demo_rig.py `resize` is written and compiles but NOT run on rig 6 (budget used up by slow sanitizer builds).
- Cause 1 (cannot grow window) unconfirmed on a rig. Code shows no max limit in the handler; hypothesis is a screen-sized window. Needs a window-size measurement during the reflow scene.

## Rig check (s33 agent 4)
Rig 3, worktree UPDemo built (md5 ff01282f, same in VTC:), `UPTERM_RIG=3 python3 -I tools/rig/demo_rig.py resize`:
- First run: check bug, not fix bug. The demo sets the window title (OSC 2) to "UP-Term demo..." so `ami.window('rzdemo')` was None. Fixed in commit "Find the demo window by its OSC title prefix" (ami.window(prefix=True)).
- Second run: `demo_rig: PASS`. Lines: demo starts at reflow scene ok; demo still runs after shrink ok; window could be made bigger again ok; still runs after grow ok; scene after reflow animates (two frames differ) ok; Esc ends the demo ok.
- Cause 1 measured: screen "Workbench Screen" 640x256 depth 8; demo window at the reflow scene (0,0,640,256) = screen-sized (the XCON 760x440 request is clamped). After dragging the size gadget -200,-120: 440x136 (55 cols, under 76 x 20: follow_size waited, demo kept running). After drag +200,+120: back to 640x256. So the window CAN grow after a shrink; it only looks "smaller only" because it starts at screen size. Handler limits are WA_MaxWidth/MaxHeight = ~0 (vtcon_handler.c:2955), no WindowLimits calls: no handler bug, no second cause. demo/updemo.c unchanged.
