# S56-UD5-SPLIT-SCENE fix (2026-11-04)

Invariant violated: when the splits scene ends, the demo owns the whole window and has the keys.
Root-cause level: the demo scene, not the handler. /pane focus (MENU_PANE_FOCUS) is by design "the sender takes the keys";
/pane close closes the SENDER's pane (pane_close(c, from)), so the demo cannot close the pane it opened. The scene
unzoomed at the end, which left two ~46-column panes (demo needs 76 x 20) and handed the keys to the new pane.
Fix (demo/updemo.c sc_splits): /split right, /pane focus, /pane zoom, and stay zoomed; no second /pane zoom;
splits_leave no longer unzooms. The new shell runs behind; /pane zoom shows both, exit closes it.
Regression test: tests/test_updemo.c the_splits_scene_ends_zoomed_so_the_demo_pane_owns_the_window
(command log must be "/split right|/pane focus|/pane zoom|"; before the fix it was "...|/pane zoom|/pane zoom|").

## Version 2 (2026-11-04): /pane close other

Version 1 (zoomed end) rejected: after quit the tab stayed zoomed with a hidden pane; a re-run split a split tab.
Root cause unchanged in level: /pane close only closes the sender's own pane. Fix at the handler: `/pane close other`
(MENU_PANE_CLOSE_OTHER, parsed in handler/slash.c like `/tab close N`, error "pane: close [other]") closes every pane of
the sender's tab but the sender's, through the existing pane_close (the close-gadget path, so a shell ends as before).
Scene: /split right, /pane focus, zoom, unzoom, /pane close other, /pane focus; splits_leave finishes an early-left scene.
Tests: tests/test_slash.c (parse + error forms), tests/test_updemo.c scene log.
Fail before (v1 code, new tests): slash FAIL test_slash.c:71 parse("/pane close other") = 2, want 1 (2 failed);
updemo FAIL test_updemo.c:769 (1 failed). After: slash 426 checks 0 failed, updemo 8689 checks 0 failed. make amiga rc 0.
Rig 106 (v2, b7d62363, --allow-stale-base: missing origin/main commit cb830707 is evidence-only): UPDemo 8 ran, q and the
prompt came back; 8 shots in ~/Desktop/upterm-shots/s56-ud5/ (r1-r8, a-c, d-after-q). Density-map of the window region
showed no mid-window divider column and no "make the window bigger" text in r4/r8, but the shots did not catch the split
frame (timing), so the split-and-close sequence is NOT proven on the rig; `/pane close other` by hand NOT run. Open.
