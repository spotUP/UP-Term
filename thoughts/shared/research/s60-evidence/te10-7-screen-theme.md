# TE-10.7: drawn menu and theme windows take the theme of their screen

## Invariant
A window Amarchy draws itself (drawn menu amarchy/gui.c, theme windows amarchy/themewin.c) uses the theme the screen rules give
the screen it opens on; no rule, excluded, or no public name: the main theme. Before: both always read the main theme.

## Level of the fix
Pure rule in amarchy/gthemes_core.c `gt_screen_draw_theme` (built on `gt_screen_theme_of`). DOS side in amarchy/iconpen_dos.c:
`ipd_pub_screen_name_of` (name copied under LockPubScreenList, unlocked before return), `ipd_screen_theme_name` (name, then
rules + main theme read with the list unlocked), counter `ipd_screen_theme_asks`.
gui.c `pens_get(g, 0)` and the preview compare in `preview_sync` use the screen theme; `cur_label` and the font spec stay on the
main theme (D10-7). themewin.c `pens_get(scr)` keeps the screen of its first window (`g_scr`); the tick compares against the
screen theme of `g_scr`.

## Reused
`ipd_screen_rules_load`'s reader `rules_read` (rules and AM_GTHEME in one read, as the icon pens take them), `gt_screen_theme_of`,
`gt_dos_load`. bar.c left alone: its walker collects all screens at once, not pointer to name.

## Deviations from the design
- Main theme comes from `rules_read` (same AM_GTHEME read) instead of `ipd_screen_rules_load` + `gt_dos_current`: one read, and
  iconpen_dos.c has no gthemes_dos.c dependency, so no Makefile link-list change (link-list test passes).
- `preview_sync` now reads the rules file on each call (one small DOS read per selection move).

## Open points
- themewin keeps ONE pen set (`g_cm`): theme windows on two screens at once share the first screen's theme (existing limit).
- `g_scr` is not cleared when the last window closes; only used while `g_wins > 0`.

## Results
RED (test with a wrong stub, assertion fail): `FAIL tests/test_gthemes.c:2663 got [gruvbox] want [nord]`, make rc 2 (runtest printed `RUNTEST gthemes rc=2`).
GREEN: `RUNTEST gthemes rc=0 [OK] gthemes: 6674 checks, 0 failed`. m68k build of the seven targets rc=0. wb-lock-guard OK, link-list test OK.

## Rig check (not run here)
Public screen with its own theme entry (screen-themes: `NAME = othertheme`), open the drawn menu on it: `GetVar AMARCHY_THEME`
names that theme and menu pixels match its colours; on Workbench the main theme; select a Style > Theme preview on the themed
screen then leave: colours return to the screen theme; `ipd_screen_theme_asks` rises. Theme windows (leaf) likewise.
