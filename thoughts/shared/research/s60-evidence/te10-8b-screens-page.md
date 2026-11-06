# TE-10.8b Screens page of the Theme Editor

Branch te10-8b. Commit 1 b46c8cf7 (pure model + tests), commit 2 (glue).

## Changes
- amarchy/gthemes_core.c/.h: row model gt_se_* (24 screen rows, 16 pattern rules), gt_screen_pat_*, gt_se_loader hook (called once by se_ensure before the first row shows).
- prefs/prefs_ui.h PG_PAGE_THEME_SCREENS, prefs/settings_def.c page name "theme-screens", dist/keymap line settings.theme.screens.
- amarchy/iconpen_dos.c:184 ipd_pub_screens: THE sampler (LockPubScreenList, copy names, unlock). bar.c sample_screens_on calls it; no second copy.
- amarchy/gthemes_dos.c:338 gt_dos_screens_load (loader) and gt_dos_screens_save; gt_dos_load_rows installs the loader.
- amarchy/leaf.c theme_action: GTA_SCR_* cases (Add Rule asks a pattern through ui->ask_value).

## Evidence
- Fail-before: "[FAIL] tests/test_gthemes.c:2879: total = 400, want 309" at first ER_MAX; new tests failed before the model existed (undefined gt_se_*).
- gthemes: 6924 checks, 0 failed. iconpen: 2524 checks, 0 failed. test_wb_lock_guard OK.
- m68k: Amarchy, amarchy-gui, amarchy-menu, upprefs, amarchy-flat, amarchy-icons all rc 0.

## GUI change to row model
Row click -> lf_setting_enter -> gte_row_* (routed by page PG_PAGE_THEME_SCREENS to se_*) edits gt_se state; buttons -> theme_action -> gt_se_action / gt_se_add_rule.

## Save to pen holder
Save Screen Themes -> gt_dos_screens_save: gt_se_text (gt_screen_rules_write) -> ENVARC:Amarchy/screen-themes and ENV: copy; AM_SCREENTHEMES set to on/off from gt_se_enabled (off only for default workbench with no rules); then ipd_rules_changed() sets rules_dirty on the pen holder record; its next sync runs rules_read (ENV:Amarchy/screen-themes + AM_SCREENTHEMES + AM_GTHEME). Workbench row theme stays in AM_GTHEME.

## Deviations
- Workbench row refuses theme and Take Over Palette (as 8a). All/None/Invert return OK even if Workbench cannot change.
- Caps 24 rows, 16 patterns; row total 309 -> 483; ER_MAX 512.
- Loader runs once per GUI start (gt_dos_load_rows), so screens opened later need a GUI restart to show; no refresh hook.
- tests/test_keymap.c not changed; keymap suite not run. Older Amarchy without ':main' token: open point.

## Open for rig proof and TE-10.9 Restore Original
- Rig: open page, tick a screen, Save, check both files and that the pen holder repaints; Add Rule prompt; Take Over Palette effect.
- Restore Original must also restore the screen-themes files and the AM_SCREENTHEMES key.
