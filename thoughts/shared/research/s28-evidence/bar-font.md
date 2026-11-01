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

## A1200 check (2026-11-01, s29): origin/main 02723ad5 deployed and measured
- Built in a worktree off origin/main 02723ad5 (includes 6342584d). Deployed to UP-Term:bin: Amarchy eca772db, amarchy-gui fe3caba2, amarchy-menu 20e5c4be, amarchy-index 09094707, amarchy-flat 212d0517, amarchy-icons af886b9e (MD5Sum on the A1200 == build). Backups beside each: *.bak-s29barfont. Restart: AMARCHY QUIT via ARexx, copy via `ami.py long` script from RAM:, then `Run >NIL: UP-Term:bin/Amarchy`.
- Screen Workbench 680x270 (unchanged from Part 2 of bar-gap.md, helvetica 13). Bar window 'Amarchy bar' at 0,255 680x15, flush to the bottom (255+15 = 270).
- Bar RastPort font not read directly (no probe binary kept); measured from pixels instead. Screenshot ~/Desktop/upterm-shots/s29-barfont-a1200.png, bar rows 255..269: row 255-256 empty, ink rows 257..268 (12 rows, 45..137 ink pixels each; 266-268 are descenders, 5-7 px), row 269 empty. Before the fix: ink only in rows 256..263 (topaz 8), rows 264..269 empty.
- PASS: text now helvetica-sized and fills the bar; no 6-row empty band.
