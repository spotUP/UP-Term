# S57-TITLE-EMPTY-AFTER-UPDEMO: cause (host analysis, no rig)

Cause: the engine never learns the window's open title. The open string lives in
vtwin.title (handler/vtcon_handler.c:1780/1826 -> render/vtwin.c w->title); the
engine's own t->title starts "" (engine/vtengine.c, vt_new / RIS at ~6285).
UPDemo's `\033[22;0t` (demo/updemo.c:2741) pushes t->title = "" (engine/vtengine.c:3477),
then `\033]2;UP-Term demo\007`, and `\033[23;0t` (updemo.c:2857) pops "" and calls
cb_title (render/vtwin.c:148) which copies "" into w->title and shows it
(vtwin_show_title). So the bar is "" after `q`. Not UPDemo, not amarchy-flat.
Existing test window_reports_and_title_stack only used a title set via OSC, so
the open-string case was uncovered.

Before: title after UPDemo + q = "" (uitree, run 4 rig 143). After (fix): the open
string, e.g. "vtcon shell".

Fix: vt_set_title(t, s) (engine/vtengine.c/.h) seeds t->title without a callback;
vtwin_attach calls it with w->title after vt_new (render/vtwin.c).
Test: tests/test_xterm.c title_pop_restores_window_open_title (make test ONLY=xterm).
Rig: not run (cause shown in code). Open: RIS (`\033c`) still resets t->title to ""
and does not re-seed it from the window; separate question.

## S58-TITLE-RIS: RIS blanked the seeded title

- Invariant: the engine title and the window's title bar agree. `vtwin_attach` seeds the engine with the open string (`vt_set_title`); a full reset (RIS, `\033c`) must not change that, because xterm's RIS does not touch the window title and `cb.title` is not called by `vt_reset`, so the bar keeps the open string.
- Cause: `vt_reset` (engine/vtengine.c) did `t->title[0] = 0` and nothing seeded it again, so the engine held "" while the window showed the open string; a later CSI 22 t pushed "".
- Level: the engine, the same pattern as `bs_default` (`vt_set_backspace_bs`) and `cursor_dflt` (`vt_set_cursor_style`): a stored host default (`title_dflt`, written by `vt_set_title`) that `vt_reset` restores. The title stack is emptied on RIS as before (`n_titles = 0`); a title pushed by a program does not survive RIS.
- Side effect on purpose: `vt_set_personality` also resets, so it too now keeps the host title.
- Test: `ris_keeps_window_open_title` in tests/test_xterm.c (after `vt_set_title`, OSC 2, RIS: open title; push, OSC 2, pop: open title; without `vt_set_title`: empty after RIS).
- Fail before (engine file reverted by copy): `FAIL tests/test_xterm.c:453: strcmp(vt_title(t), "vtcon shell") = -1, want 0`, `FAIL tests/test_xterm.c:455: ... = -1, want 0`, `[FAIL] xterm: 475 checks, 2 failed`
- Pass after: `[OK] xterm: 475 checks, 0 failed`
