# S56-UD5-SPLIT-SCENE fix (2026-11-04)

Invariant violated: when the splits scene ends, the demo owns the whole window and has the keys.
Root-cause level: the demo scene, not the handler. /pane focus (MENU_PANE_FOCUS) is by design "the sender takes the keys";
/pane close closes the SENDER's pane (pane_close(c, from)), so the demo cannot close the pane it opened. The scene
unzoomed at the end, which left two ~46-column panes (demo needs 76 x 20) and handed the keys to the new pane.
Fix (demo/updemo.c sc_splits): /split right, /pane focus, /pane zoom, and stay zoomed; no second /pane zoom;
splits_leave no longer unzooms. The new shell runs behind; /pane zoom shows both, exit closes it.
Regression test: tests/test_updemo.c the_splits_scene_ends_zoomed_so_the_demo_pane_owns_the_window
(command log must be "/split right|/pane focus|/pane zoom|"; before the fix it was "...|/pane zoom|/pane zoom|").
