# B8b: Amarchy theme on a Directory Opus 5.104 own screen

## Root cause

Invariant violated: after a theme apply, the DOpus own screen's palette carries the theme.

- amarchy/menu_theme.c `dopus_theme_commands` sent `dopus set palette` x4 (own screen only), 10 `dopus set pens`, then `dopus refresh lister`.
- dopus5 rexx_dopus.c:587-640: `set palette` / `set pens` write only environment->env fields.
- rexx_dopus.c:960-982: `refresh lister` = CONFIG_CHANGE_LIST_DISPLAY, listers redraw only.
- display.c:267-269 + palette_routines.c:59: the screen palette is loaded only in display_open -> load_screen_palette.
- rexx_dopus.c:948 + main_reset.c:40-46: `refresh all` = CONFIG_CHANGE_DISPLAY, close_display + display_open(0), which loads the palette.

Fix level: data (last FLAT-D2 line). `refresh all` when palette != 0 (own screen), `refresh lister` when palette == 0 (Workbench screen, no flicker).

## Host test

- Before fix (menu_theme.c from HEAD, no git stash): `FAIL tests/test_keymap.c:3944: (long)n = 341, want 338` and `[FAIL] keymap: 21575 checks, 3 failed`.
- After fix: `[OK] keymap: 21575 checks, 0 failed`.
- Test: tests/test_keymap.c `dopus_theme_commands_use_the_theme_directly` (palette=1 ends `dopus refresh all`, palette=0 ends `dopus refresh lister`) and `dopus_theme_send_goes_to_the_dopus_port` (`r.line[12]` = `dopus refresh all`).

## Rig 94 measurement

Script: tools/rig/amarchy_dopus_rig.py. Mode set: EXPLICIT screen mode 1344x1080 on an own screen, via `rig.py dopus --own-screen` (writes the mode into DOpus5/Environment/workbench). Clone mode NOT measured.

Palette (`dopus query palette`, first 4 entries):

- Old command sequence (sent by hand, `refresh lister`): palette unchanged (stock `AAAAAA/000000/FFFFFF/6688BB` stayed the same).
- New build, real entry point `d.pick()` (Amarchy DESK THEME pick): palette changed to `0x000000 0xBEBEBE 0x7A7A7A 0x8D8D8D` (vantablack).
- Weakness: the final run's baseline was already tokyo-night (left over from an aborted run). The "old unchanged" claim rests on the earlier stock-palette run.

## Bevel half (measure only, flat_dopus_decide unchanged)

Explicit mode, 1344x1080, DOPUS.1, vantablack, `amarchy-flat STATUS` after pick and listers: `calls=100 flat=100`, then `calls=108 flat=108`, `calls=116 flat=116`. calls and flat grow together: the bevel IS skipped in this case. This contradicts the hypothesis that explicit mode (Intuition default pens, SHINEPEN 2 vs BACKGROUNDPEN 0) always stays STOCK. SHINEPEN / BACKGROUNDPEN not read (STATUS does not print them). Possible cause: the decision sees equal pens for this theme or screen; not resolved.

Clone mode: not measured. Open owner decision (flat_core.c:193-201): (a) RGB compare of the two pens' colours, or (b) skip the pen test for dopus5.library DrawBox on any non-Workbench screen. Needs a clone-mode measurement and a pen read before choosing.

## Diff

```
 amarchy/menu_theme.c | 10 ++++++++--
 tests/test_keymap.c  | 14 +++++++++-----
```
Plus new tools/rig/amarchy_dopus_rig.py and this file.
