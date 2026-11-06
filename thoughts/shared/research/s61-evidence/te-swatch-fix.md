# Theme Editor swatch bugs A and C: fix

## Change
- amarchy/menu_tree.c: new `next_page_row(page, &i)` (about line 70) is the one walk over rows `0 .. sd_nsettings + gte_nrows()`, skipping rows of other pages and rows `gte_row_visible` hides. `setting_rows` and `choice_setting` both use it. Before, `choice_setting` stopped at `sd_nsettings` and also ignored visibility.
- tests/test_gthemes.c: `test_gthemes_swatch_and_palette_rows_apply` (3a, 3b).

## Other `sd_nsettings` loops (grep amarchy/*.c)
Only two occurrences exist, both in menu_tree.c (setting_rows: already correct bound, now shared; choice_setting: the defect, changed). No other loop with the old bound. Other `gte_row(...)` users (leaf_core.c:637,928,932; menu.c:502; gui.c:1034; leaf.c:411; menu_tree.c:272) take a row id from an item and need no bound: correct as is.
Not grepped: loops written as `SDI_COUNT` or `sd_settings[...]` arrays outside amarchy/*.c (out of scope).

## Tests
- Before fix (HEAD d9a5cfbe): keymap FAIL at test_keymap.c:3485 (nb = 0, want 17), :3488 (na = 0, want 17), :3501 (np = 0, want 16).
- With fix: keymap 21387 checks 0 failed; gthemes 6968/0; settings_def 1779/0; prefs 10189/0.
- Fix reverted (menu_tree.c from HEAD): same 3 FAIL lines, "[FAIL] keymap: 21387 checks, 3 failed". Fix restored.
- m68k build of Amarchy, amarchy-gui, -menu, -index, -icons, -flat, upprefs: rc=0. Warnings in the log are pre-existing and unrelated (atiming.h unused functions, win_core.c -Wmain); none from menu_tree.c.

## 3a swatch apply
Built-in swatch0 and an added swatch: `lf_setting_set_colour(.., SD_PICK_CUSTOM, hex)` changes the swatch (read back through gte_row_text). Passes; the apply path was not broken (the existing TE-4 test already covered the first swatch row, the new test adds the added swatch).

## 3b Terminal Palette
The reused row has store SD_UPTERM, kind SD_PALETTE and the same `off` as sd_settings[SDI_PALETTE]. A pick through it lands in the profile file; lf_palette_hex reads it back, and a pick through the real Settings row lands in the same file. No second bug found.

## Unverified
Amiga runtime (menu level actually showing 17 / 16 entries and the colour change on screen) is unverified; no rig run in this brief.
