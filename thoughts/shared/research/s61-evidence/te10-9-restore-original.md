# TE-10.9 Restore Original (worktree vtcon-te10-9, branch te10-9)

Changed
- amarchy/gthemes_core.h: enum GTA_SCR_RESTORE (after GTA_SCR_REMOVE).
- amarchy/gthemes_core.c: row `scr-restore` "Restore Original" (SD_GACTION) after "Save Screen Themes" (last row of the Screens page); gt_se_action case GTA_SCR_RESTORE = parse "default workbench\n" into the rules model (no rule, Workbench only), marks dirty.
- amarchy/leaf.c: case GTA_SCR_RESTORE: gt_se_action(GTA_SCR_RESTORE,0) then gt_dos_screens_save (both files, AM_SCREENTHEMES key from gt_se_enabled, ipd_rules_changed). AM_GTHEME and dk_orig_off untouched.
- tests: test_gthemes.c (new test + row-count pin 483 -> 484), test_iconpen.c (test_restore_original_releases_every_screen), tests/test_amarchy_restore_original_wiring.py.
- dist/keymap and dist/menu-icons: no entry; scr-save and the other action rows have none.
- Row cap: GT_SE_ROWS limits screen rows, not action rows; page rows live in ER_MAX 512 (484 total now).

Red then green
- gthemes: red = `error: use of undeclared identifier 'GTA_SCR_RESTORE'` (test_gthemes.c:3236, 3240; cc -fsyntax-only); green = [OK] gthemes 6947 checks, 0 failed.
- python guard: red before code (`leaf.c has no case GTA_SCR_RESTORE`), green `ok` after.
- iconpen: new test passes without new product code (the sync already releases on hold 0); no earlier test covered exactly this chain (rules parse -> theme_of -> sync -> one release per pen per slot), so it is added as a pin, not a red test. iconpen and settings_def green.
- m68k build of the 7 targets rc 0.

Exit path (design line 82): present. amarchy/amarchy.c iconpens_stop() (called at amarchy.c:681) -> ipd_owner(IP_OWNER_MAIN, 0) -> pens_sync (amarchy/iconpen_dos.c:380): hold[i] = owner && theme_of for every slot, so owner 0 releases all slots. Not changed.
