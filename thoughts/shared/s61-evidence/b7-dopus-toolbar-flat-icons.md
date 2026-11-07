# B7 DOPUS-TOOLBAR-FLAT-ICONS: flat Lucide toolbar bank for Directory Opus 5.104

## Facts
- ARexx verbs (dopus5 rexx_lister.c): `lister query <h> toolbar` (:613), `lister set <h> toolbar <path>` (:1269), `lister query all` lists the handles (`all` is not allowed for set). Paths with `-` need single quotes in ARexx.
- Upstream default reads `PROGDIR:Buttons/toolbar` (measured on the rig, os3 5.104 archive, md5 0800b7b3bf02c83bd45a84d038ee483e).
- Compare rule (amarchy/menu_theme.c dopus_toolbar_flat, dopus_same_path): case-insensitive equality with `PROGDIR:Buttons/Toolbar` or `DOpus5:Buttons/Toolbar`, trailing blanks and newlines ignored. Any other path (custom bank, already switched) is never touched. After a set the path is re-queried; if it is not ours (a missing bank clears the toolbar) the old path is put back.
- Bank format: FORM EPUS { BTNW (132 bytes), BUTN..., FUNC... }. BTNW flags at chunk offset 128 (file offset 0x94): upstream 0x06, ours 0x16 (BORDERLESS 0x10). Upstream bank 2734 bytes, 55 chunks, 22 unique images; ours 2926 bytes (only the image paths `dopus5:images/X.small` -> `dopus5:images/Amarchy/X.small` and the flag differ; same verbs, same order).
- Images: 24x24 ILBM, depth 2, CMAP of 4 pens (default Tokyo colours), pens 0-3 only. Glyph map: dist/dopus-toolbar.txt (22 rows, read by tools/gen_dopus_toolbar.py).
- Install: `DOpus5:Images/Amarchy/` and `DOpus5:Buttons/Amarchy-toolbar` (dist/addons/dopus/install.dos, removed by uninstall-body.dos; Files/toolbar in parts.txt and packages.txt).

## Limits and deviations
- The environment default toolbar cannot be changed, so listers opened later keep upstream's toolbar until the next theme apply.
- The 4 pens are the default Tokyo colours and do not follow later themes.
- No `_s` selected images ship: ILBM has no selected state.

## Tests
- tests/test_dopus_toolbar.py (OK): 22 image paths, each file decodes 24x24 depth<=2 with 12-byte CMAP and glyph pixels, Borderless clear upstream and set in ours, chunks identical to upstream once path and flag are restored.
- tests/test_keymap.c dopus_theme_send_switches_only_a_default_toolbar: `make test ONLY=keymap` 21603 checks, 0 failed. Fail-before: run with `git show HEAD:amarchy/menu_theme.c` in place, the new test fails (no toolbar lines sent). `make test ONLY=addons` OK.

## Rig 94 (--os 32, DOpus screen DOPUS.1 1344x1080)
- Fresh listers A, B both `PROGDIR:Buttons/toolbar`. A set to `DOpus5:Buttons/Custom-bank`. Theme pick (white).
- After apply: A = `DOpus5:Buttons/Custom-bank` (never-touch rule holds), B = `DOpus5:Buttons/Amarchy-toolbar`.
- Tooling note: an earlier run did not switch later listers because the rig still held stale amarchy-gui and amarchy-menu (built before the change; the theme pick runs there). After rebuilding and installing them the switch is reliable. No code bug.
- Screenshot question "flat line glyphs without bevels, or MagicWB-style shaded icons?": haiku answer: thin blue outline glyphs on white/grey, no bevels or shading, not MagicWB; every visible button has a glyph (coarse low-resolution view). Screenshot deleted (~/Desktop/upterm-shots/b7-toolbar.png and the scratchpad copy).

## git diff --stat
 Makefile (my hunks only) 17+, amarchy/menu_theme.c 84+, menu_theme.h, dist/README.txt, install.dos 30+, parts.txt, uninstall-body.dos, packages.txt, tests/test_keymap.c; new: dist/dopus-toolbar.txt, tests/test_dopus_toolbar.py, tools/gen_dopus_toolbar.py, tools/rig/amarchy_dopus_toolbar_rig.py
