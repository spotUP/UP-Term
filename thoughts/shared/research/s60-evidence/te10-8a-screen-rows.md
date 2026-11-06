# TE-10.8a: pure row model of the Theme Editor Screens page

Invariant: the Screens page edits the same rule list the file holds; a rule must be able to say "the main theme, whatever it is" for a named screen. Fix level: gthemes_core (pure), no page code.

## Change
- GT_SCR_MAIN ":main" token (a colon cannot be in a theme name). parse/write carry it unchanged; gt_screen_theme_of / gt_screen_draw_theme return main_theme for it (was GT_DEFAULT via scr_name: the red case).
- New static scr_pick (the one decision: themed?, which rule) now serves gt_screen_theme_of and the row model; scr_is_main shared. `skip` index lets normalisation ask "themed the same without this rule".
- gt_screen_rows, gt_screen_row_tick/theme/takeover, gt_screen_rows_all/none/invert. No local gt_screen_rules or row array in new code.
- Reused: gt_screen_rules_add/_remove, gt_screen_exact, gt_screen_match, cmp_ci, is_wb_exact, gt_name_ok, scr_name. Searched: grep of gt_screen_, scr_name, is_wb_exact in amarchy/ for an existing row/tick helper; none existed.

## Results
- RED (new API as stubs returning 0, unchanged logic): `RUNTEST gthemes rc=2 [FAIL] gthemes: 6849 checks, 105 failed`
- GREEN: `RUNTEST gthemes rc=0 [OK] gthemes: 6874 checks, 0 failed`
- m68k: make of Amarchy amarchy-gui amarchy-menu amarchy-index amarchy-icons amarchy-flat upprefs rc=0 (warnings only in untouched files); test_make_link_lists.py OK; test_wb_lock_guard.py OK.
- A first run died on a -Wdeprecated sprintf in the new test (compile error, no tests ran); fixed with snprintf.

## Deviations
1. Row takeover on a pattern-themed screen adds `name = <pattern's theme> [takeover]`, not `:main takeover` (the latter would change the screen's theme). Takeover off on a pattern-takeover screen adds an exact rule with takeover 0.
2. Workbench tick on under Default None returns 0 (no rule can theme it; the file cannot store a Workbench theme rule), nothing changed.
3. row_theme refuses the theme name "exclude" (would turn into an Exclude rule).
4. Normalisation only drops exact `:main` rules without takeover; an exact rule equal to the pattern's non-main theme stays (as the brief states).
5. Duplicate names in the open list are handled once by All/None/Invert (Invert would otherwise flip twice).

## Open points / rig check later (TE-10.8b)
- Page must pass the open-screen name list; names with wildcard chars (# ?) give rows whose ticks return 0.
- Rig check must measure: tick on a new public screen colours it with the main theme at once; Workbench tick off leaves Workbench colours alone; a :main rule follows a changed main theme live; written file in ENVARC:Amarchy/screen-themes holds `:main` lines and an older Amarchy reading them would treat ":main" as an unusable theme name (falls to GT_DEFAULT).
