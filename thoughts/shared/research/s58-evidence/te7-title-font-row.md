---
date: 2026-11-05
topic: TE-7-TITLE-FONT drop the Window Title Font row from the theme editor
verdict: done; row dropped, model slot / theme key / wbreset untouched
---
# What changed
- amarchy/gthemes_core.c: `font_label[GT_FONT_TITLE]` is now 0 and the Theme Type row builder skips slots without a label. Rows keep `aux = slot + 1`, so Screen, Menu, Icon, Shell still edit their own slots (no shift).
- tests/test_gthemes.c: expected row list loses "Window Title Font"; row total 310 -> 309; new test `test_gthemes_te7_no_title_font_row` (no title row; four remaining rows map to their slots; theme file with title-font loads and survives save + reparse).
- Kept: GT_FONT_TITLE, gt_fontkey "title-font", wbreset_core.c:166.

# Proof
- RED (before fix): `[FAIL] gthemes: 6135 checks, 2 failed` (te4_row("Window Title Font") == 0 failed at line 1488; the other failure was a bug in my first round-trip assertion, string match on save text, fixed to reparse).
- GREEN: `RUNTEST gthemes rc=0 [OK] gthemes: 6136 checks, 0 failed`.
- m68k: make of the 7 Amiga targets rc=0.

# Menu / Shell font finding
Same defect: no consumer. `grep GT_FONT_MENU|GT_FONT_SHELL amarchy/` hits only the enum (gthemes_core.h:197); `look.font[` is read only for SCREEN and TITLE (wbreset_core.c:164,166) and via gt_look_font for ICON and SCREEN (desk_core.c:798,805). Menu Font and Shell Font rows only store a value that nothing applies. Not dropped, per brief.
