# FP1-REVERT-THEME-LINE: Revert puts the previous theme back completely

## Invariant
Revert puts the previous look back with ONE source for the Workbench colours: the previous theme. Before: pens 0-3 came from the
Sys/ prefs snapshot (right), the prefs theme line kept the picked name, pens 4-7 came from `desk_live_baseline` (stock colours).
Root cause level: the revert data (snapshot + marker set) did not carry the theme name.

## Where the name is recorded
- `gt_dos_select` (amarchy/gthemes_dos.c) copies the theme line it loaded before overwriting it and hands it to
  `desk_dos_prev_theme` (amarchy/desk_dos.c) once the pick saved.
- `dk_snapshot` -> `dk_snapshot_theme` writes `T:amarchy-revert/prevtheme` (text from `desk_revert_theme_make`, desk_core.c) only when no
  unconfirmed apply is pending (FP-1: the first snapshot wins). No known name = the file is deleted (no stale name).
- The name file is the 4th member of `dk_mark_extra`, so the ENVARC marker set (reboot recovery) carries it. Keep deletes it.
- `desk_dos_revert`: reads it (`desk_revert_theme_parse`); writes the theme line through `amconf_load`/`amconf_save(&ap, 1, ..)`, the same
  writer `gt_dos_select` uses (ENV: and ENVARC:); loads the theme with `gt_dos_load`; `dk_restore_targets` then sets pens 4-7 through
  `desk_revert_role_pens` -> `dk_live_pens(previous theme)` (which goes through `ipd_role_write`, direct SetRGB32 only when no holder).
- No recorded name (older build's snapshot): theme line untouched, `dk_live_pens(0, 0)` baseline as before.

## Reused
`desk_live_follow` (gate and depth rules), `dk_live_pens` and its `ipd_role_write` path, `amconf_save`, `gt_dos_load`, the marker extras list.
Searched: desk_core.c, desk_dos.c, gthemes_dos.c, iconpen_dos.c for existing name/prefs-line writers: none other.

## Consequence
A timeout or Revert now also puts the theme line back, so consoles, menu and DOpus follow the previous theme (matches the requester text).

## Results
RED (stubs): `[FAIL] desk: 2333 checks, 14 failed`
GREEN: `RUNTEST desk rc=0 [OK] desk: 2333 checks, 0 failed`
m68k: seven-target build rc=0 (first compile of the DOS side; no warning in desk_dos.c, gthemes_dos.c, desk_core.c); `tests/test_make_link_lists.py` OK.

## Rig check (not run here)
Pick a theme, leave the Keep/Revert requester unanswered for 15 s. Measure: `ENV:Amarchy/prefs` and `ENVARC:Amarchy/prefs` theme line = the
previous theme; pens 4-7 of the Workbench screen on a deep screen = the previous theme's role colours (tokyo-night, flat: 3e4254 0e0e14
9ece6a 0e0e14), not 0000ff ff00ff 00ffff ffffff. Also: pick, reboot before answering, start Amarchy: same result via the marker set.
