# TE-7-FONT-MAP

Violated invariant: the theme model is the single source of the look; the font.prefs / icontrol.prefs writers read gtheme.font (v1 key) and a fixed thin switch instead.
Cause: amarchy/desk_core.c desk_font_name (read t->font only, WBFONT only, no size); amarchy/desk_dos.c dk_icontrol -> desk_geometry(thin) ignoring look.bar_height.
Fix: gt_look_font / gt_look_bar_height (amarchy/gthemes_core.c) read th->model->look (the model gt_load_model attaches); desk_font_name writes WBFONT from the icon font and SCREENFONT from the screen font (name + size); desk_geometry_bar maps bar height to IControl byte 1 (signed delta to stock BorderTop 11); both dk_icontrol callers pass the theme.
Test: tests/test_desk.c test_desk_model_font_bar (suite desk), asserts written bytes.
Fail before (writers blind to model): 7 FAIL, e.g. "desk_font_name(...) = 0, want 1", "gt_look_bar_height(&t) = 0, want 14", "geom[1] = 255, want 3", "[FAIL] desk: 2311 checks, 7 failed".
Pass after: desk 2311/0 failed, gthemes 2043/0, wbreset 85/0.
Open: title font has no own FONT chunk (not written); bar-height to byte-1 delta is measured for -1 only (rig check); no rig run.
