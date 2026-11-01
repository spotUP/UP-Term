# BAR-FONT-MISMATCH (s29), rig 12 (060, overscan, 800x600 screen)

Fix (branch s29-bar-font): tw_open SetFont's the window RastPort to the screen font (themewin.c); bar height comes from bar_core `bar_height(ysize)`.
Choice: SetFont in the tw_ glue (single source for every tw_ window), not measuring with the window font.

Setup: font.prefs WBFONT and screen font = helvetica.font 13, system font left topaz 8 (as on the A1200; a first run with
the system font also 13 masked the bug: both builds drew rows 2..10). Bar window "Amarchy bar" 800x15 at top 585 both runs (height = 13 + 2).

Pixel rows with ink inside the 15 row bar (grab of the bar window, text-only rows):
- before (installed main build): rows 1..7 (topaz 8), rows 8..14 empty (7 rows) under the text.
- after (this branch): rows 2..10 (helvetica 13 cap/x-height, faint descenders 11..13): no empty band.

Host: make test ONLY=bar -> "[OK] bar: 3382 checks, 0 failed" + tests/test_bar_font.py (2 tests). Reverting themewin.c to origin/main fails
test_tw_open_sets_window_font_to_screen_font (1 failure); restoring it passes.
