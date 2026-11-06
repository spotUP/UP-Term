# TE-COLOUR-LIST-ALL-SWATCHES

Cause: amarchy/gui.c obtained a pen for each colour chip and for the theme preview (fill_rgb), drew, and released it at once. On a pen screen the next obtain reuses the number and recolours every earlier chip.
Repro (rig 172, 3.2, 256-colour pen screen, Settings > Colors > Text colour): 1 distinct chip colour of 7, following the cursor.
Fix: reuse amarchy/pen_cache.h (chip_pens, prev_pens); release in draw_full, pens_release.
After: 7 distinct of 7 in 4 shots, matching the list rgb. Shots: ~/Desktop/upterm-shots/chip-pens-a0..a3.png.
Tests: test_gthemes_chip_pens_held_per_frame (host), tests/test_gui_chip_pens.py (source guard, fails on HEAD).
Open: Theme Editor swatch row count (17) not checked on the rig.

## Follow-up: scroll path
Defect: the release sat in draw_full and pens_release only. A scroll calls draw_list only, so after PC_MAX (24) distinct chip colours pc_of returned -1 and chips vanished (regression of 3b903f8d; a picker list is longer than 24).
Fix: chip_pens_release (both caches) now runs at the start of draw_list, after the await return and paint_begin. draw_full no longer releases (it calls draw_list: one release per frame). pens_release keeps its call. Comments updated.
Single-row redraws (cursor move pair, edit row at the K_BS/K_CHAR sites): they repaint rows the last list draw already held a pen for; chip colours change only when lf_setting_rechip runs on a level reload (not by a single-row redraw), so they add no colours. A frame holds at most MN_OSD_MAX_ENTRIES (10) chip colours: cannot reach 24. UNVERIFIED: that every rechip path is followed by a list or full draw (read only the two sites near gui.c:812 and :1956).
prev_pens: TPV_COUNT 26, text items 5 (Active/Inactive Window titles, menu header, disabled item, icon label) so box items 21 <= PC_MAX 24; pinned by test_themeprev_boxes_fit_the_pen_cache (counts filled items from themeprev_layout).
ColorMap: pens_release callers (preview change, theme_live, screen unbind near gui.c:1833) run with g->cm still the ColorMap the pens came from; g->cm is set at screen bind (gui.c:1804) and zeroed only after the release at the unbind (gui.c:1835).
Tests: test_gthemes_chip_pens_scroll_frames (host), tests/test_gui_chip_pens.py (two new guards), test_themeprev_boxes_fit_the_pen_cache.
