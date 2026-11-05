# TE-7-APPLY-TEST

Question: do two themes differing only in the model's look font, or only in the bar height, replan the font / icontrol part when fed the other theme's bytes, and plan nothing when fed their own?

Result: font YES already. Icontrol NO: a real bug. desk_icontrol_decide returned DK_IC_NONE for any non-stock geometry ("ours"), so a changed bar height never rewrote icontrol.prefs at start or in dk_apply's start path.

Invariant violated: the written icontrol.prefs must equal desk_geometry_bar(1, gt_look_bar_height(theme)); the plan is "none" only when it does.

Fix (amarchy/desk_core.c): desk_icontrol_decide takes bar_height; an already-written file with another geometry gives DK_IC_WRITE with no new snapshot (the pre-thin snapshot is kept); same geometry gives NONE. desk_start_icontrol takes the theme. Callers in amarchy/desk_dos.c (dk_apply start decision, start plan) pass the theme.

Test: test_model_font_or_bar_height_change_replans_font_and_icontrol (tests/test_desk.c, suite desk_start_apply). Cases: font B over A's file writes 518 bytes equal to B's file; B over B writes 0; font part off 0; bar 18 over the file for 14 plans DK_IC_WRITE with snapshot 0; 18 over its own bytes DK_IC_NONE; icontrol part off with nothing to restore DK_IC_NONE.

Fail before (old behaviour, restored by `return 0 ? DK_IC_WRITE`):
  FAIL tests/test_desk.c:1879: desk_start_icontrol(all, &t, ia, ian, 1, &snap) = 0, want 1
  [FAIL] desk_start_apply: 52 checks, 1 failed
Pass after: [OK] desk_start_apply: 52 checks, 0 failed (make test ONLY=desk_start_apply); also [OK] desk: 2311 checks, 0 failed.
Font mapping: dropping the SCREENFONT contribution in desk_font_name left the test green (the icon font size still differs between A and B); the icon font is covered by the 9 vs 8 difference. Not separately broken-tested for icon.
Not built: the Amiga cross-compile of desk_dos.c (host only task); the two call sites are simple argument additions.
