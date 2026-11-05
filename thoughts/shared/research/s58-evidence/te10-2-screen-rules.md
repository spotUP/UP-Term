# TE-10.2 screen rules model (host only)

Files: amarchy/gthemes_core.[ch] (model), amarchy/amconf_core.h + prefs/settings_def.[ch] (key), tests/test_gthemes.c, test_amconf.c, test_settings_def.c.

## API (gthemes_core.h)
- gt_screen_rules { enabled, def (GT_SCR_ALL|WORKBENCH|NONE), n, rule[GT_SCR_MAX=32] }; rule = { pat[140], theme[GT_NAME], exclude, takeover }.
- gt_screen_rules_parse(buf,len,&r) (len 0 = default workbench, no rules, enabled 1); gt_screen_rules_write(r,buf,cap) (returns length, -1 too small); gt_screen_rules_add / _remove; gt_screen_exact; gt_screen_match.
- int gt_screen_theme_of(r, name, main_theme, char *theme, int cap, int *takeover): 1 = themed (theme filled), 0 = untouched. The spec's "&theme" is a char buffer plus cap here.
- Key: AM_SCREENTHEMES = "screen-themes", off/on, default on, hidden (no Prefs gadget; sd_hidden row, SD_NHIDDEN 14 -> 15). The caller copies the key into r.enabled (TE-10.3); no caller wired yet.
- File: line 1 "default all|workbench|none", then "<pattern> = <theme>|exclude [takeover]". Bad lines skipped; ';' starts a comment line.

## Decisions
- Matcher: searched the tree (shell/sh_expand.c sh_match = Unix glob, no #?; handler/complete.c and amarchy/gthemes_dos.c call dos.library; shell/sh_amiscript.h only mentions #?). No pure AmigaDOS matcher exists, so a small one is local in gthemes_core.c: #? (any run), ? , #c (run of c), case-insensitive. No (a|b), ~, % yet. No Makefile change needed (gthemes_core.c is already in every target that links it).
- Workbench: a theme rule on exact "Workbench" is dropped on parse and not written; in memory theme_of answers the main theme for it. Exclude on exact Workbench is stored; remove it and the main theme answers again (AM_GTHEME never touched).
- Exact-name rules (no # or ?) are written before pattern rules; each group keeps its order.
- Unusable theme name (empty, '/' or ':', too long; also empty main theme) resolves to GT_DEFAULT. Whether a name is an INSTALLED theme is not known here (pure); gt_load already falls back to the builtin for a missing file.
- Pattern max 139 chars; 32 rules max (add returns 0 when full).

## Proofs
- RED (final test code, functions absent): `tests/test_gthemes.c:601:28: error: unknown type name 'gt_screen_rules'` ; make: *** [build/host-obj/tests/test_gthemes.o] Error 1.
- GREEN: RUNTEST gthemes rc=0 [OK] gthemes: 6344 checks, 0 failed (baseline 6279, +65). Also amconf 899/0, settings_def 1779/0, help_core 160/0.
- m68k: make build/amiga/{Amarchy,amarchy-gui,amarchy-menu,amarchy-index,amarchy-icons,amarchy-flat,upprefs} rc=0 (only pre-existing warnings).

## Open points
- TE-10.3 wiring: set r.enabled from AM_SCREENTHEMES; read/write the file (ENVARC + ENV copy); installed-theme check.
- Pattern syntax beyond #? ? #c if the owner wants (a|b), ~.

## Review fixes

Resolver `gt_screen_theme_of`: exact-name rules first, then pattern rules, each in list order (matches the order `gt_screen_rules_write` stores, so resolve(r) == resolve(parse(write(r)))). The Workbench screen always gets the main theme unless an Exclude rule matches (D10-10). The existing hand-written-file check (DOPUS#? before DOPUS.1) now expects the exact rule to win.

- Red/green (tests T1-T5 kept, resolver reverted to d6f7cca8): `RUNTEST gthemes rc=2 [FAIL] gthemes: 6374 checks, 8 failed`; first line `FAIL tests/test_gthemes.c:640`.
- Green with fix: `RUNTEST gthemes rc=0 [OK] gthemes: 6375 checks, 0 failed` (before restore also 6375 / 0 failed).
- Other suites: amconf 899 / 0 failed; settings_def 1779 / 0 failed; help_core 160 / 0 failed.
- m68k build of Amarchy, amarchy-gui, amarchy-menu, amarchy-index, amarchy-icons, amarchy-flat, upprefs: rc=0.
