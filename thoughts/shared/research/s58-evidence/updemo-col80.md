# UPDemo text past column W (s58)

Invariant: no scene writes a printable cell at or past column W (the width UPDemo laid out for) at any width UPDemo accepts (UPDEMO_MIN_COLS = 76 and up).

## Measurement (before the fix)
Probe: tests/test_updemo.c `no_scene_writes_past_the_window_width_at_80_79_78_and_the_minimum`. A 200-column engine (nothing wraps) with a faked `CSI 8;rows;Wt` reply, so every cell at column W or beyond is a character a real W-wide window would have wrapped. Run in full and ANSI colour modes. `UPDEMO_COLS_LOG=1` prints each offender.

Offenders (scene: source line before the fix; widths hit):
- unicode: updemo.c 1364 closing grey line (80, 79, 78, 76)
- splits: 1464 command list (all), 1465 menu line (76), 1484 "Sent /pane close other" (all)
- tabs: 2375 (all)
- links: 1754 (78, 76), 1770 notification line (76)
- kitty truecolour: 2073 heading sub (all)
- kitty picture: 2143 caption (76), 2147 (all)
- kitty placement: 2188 (78, 76), 2199 (all)
- scroll regions: 692 (76)
- help: reply of /help ask drawn at column 2, 76 wide (76)
- reflow: deliberate autowrap paragraph, exempt in the test
- resize (DECCOLM 132): wide mode on purpose, not flagged

## W source (coordinator point a)
W is never assumed. updemo_run sets it per scene via set_size() from follow_size()/ask_size(), which sends CSI 18 t and parses the reply CSI 8;rows;cols t. The Amiga frontend (updemo_amiga.c size()) takes the first size from a cursor position report. The only fixed widths are the layouts that centre on W or the DECCOLM scene.

## Amiga console width at 640x256 (point c)
No measured figure in thoughts/. Indirect note in thoughts/shared/research/s34-lesson12-ramiga-t.md: a task line "wraps at 77 columns" on a 640x256 screen, Topaz 8. So a full-width window is around 77 to 80 columns depending on borders; hence the 78 and 79 widths in the test.

## Fix
Each long line shortened or split to fit 76 columns (grey_line starts at column 2, so 75 characters). The help scene draws the handler's reply from column 1. Fixed in demo/updemo.c; tests/test_updemo.c updated for two renamed strings.

## Result
Suite updemo: 9259 checks, 0 failed. With demo/updemo.c reverted to origin/main the new test fails (58 failures).
