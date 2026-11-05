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
