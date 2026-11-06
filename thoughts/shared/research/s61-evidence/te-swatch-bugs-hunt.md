---
date: 2026-11-06
topic: Theme Editor swatch bugs A, B, C (root-cause hunt, no fix)
branch: te-swatch-bugs (from origin/main d79cf2bd)
status: A and C confirmed by failing host tests; B UNVERIFIED
---

# Theme Editor swatch bugs: hunt

## A. No swatch can be recoloured (built-in or added)

- On main: yes. A host probe on origin/main d79cf2bd shows that every swatch row opens an empty level.
- Cause: amarchy/menu_tree.c:99, `choice_setting`:
  `for (j = 0; page >= 0 && j < sd_nsettings; j++) {`
  Theme Editor rows have ids >= SDI_COUNT (`sd_nsettings` == SDI_COUNT, prefs/settings_def.c:242) and are reached with
  `gte_row(id)` (amarchy/gthemes_core.c:2455). The loop stops before the editor rows, so `choice_setting` returns
  NULL for every editor picker row and `choice_rows` (menu_tree.c:149-165) emits nothing. The user enters the
  picker level (gui.c:1025-1053 `activate_setting`, menu.c:502-503) and finds no colours, so no
  `lf_setting_set_colour` call can happen. The bound has not changed since ebf71d91. Line 100 `gte_row(j)` was
  added in b10b5c8d (TE-4), but the bound was not widened.
- The apply path itself works: leaf_core.c:422-487 `lf_setting_set_colour` goes to `gte_row_set_text` and then to
  `gte_swatch_set` for SD_GTHEME rows, and test_gthemes.c:2112 sets swatch0 OK. Swatch names are well-formed
  (gthemes_core.c:984-995, 2075).
- Violated invariant: every row that `setting_rows` lists on a page can be resolved by id by every tree lookup.
  `setting_rows` (menu_tree.c:75) loops `sd_nsettings + gte_nrows()`, but `choice_setting` loops `sd_nsettings`.
  Two lookups over one row set have two bounds.
- Level: the pure menu-tree core (amarchy/menu_tree.c). It is not in the front end, the picker or the theme model.
- Failing test: tests/test_keymap.c `every_swatch_row_opens_its_colour_list`
  - `FAIL tests/test_keymap.c:3485: nb = 0, want 17` (built-in swatch0 "background")
  - `FAIL tests/test_keymap.c:3488: na = 0, want 17` (swatch12, added with `gte_swatch_add`)
- Scope: the same bound hides the lists of ALL editor picker rows (Groups, Elements, swatch refs), not only Swatches.
- Fix: in `choice_setting`, iterate the same id range as `setting_rows` (`j < sd_nsettings + gte_nrows()`), and skip
  rows that are not visible (`gte_row_visible`) so that a hidden swatch slot cannot be matched. Better: put both
  loops on one shared row-range helper in menu_tree.c, so that the bounds cannot drift again.
  Files: amarchy/menu_tree.c only. Tests are already in tests/test_keymap.c.
  UNVERIFIED: that this alone makes the tests pass (product code was not patched in this hunt).

## C. "Terminal Palette" shows one entry named "palette"

- On main: yes. Same probe: `settings.theme.swatches.palette` has 0 children.
- Cause: the same line, menu_tree.c:99. The row is `er_reuse(SDI_PALETTE, ... "Terminal Palette")`
  (gthemes_core.c:2374, 2296-2308). It keeps key "palette" and kind SD_PALETTE, but it has an editor id, so
  `choice_setting` never finds it. The empty level is titled by `mg_header` (menu_geom.c:55) with the last node
  segment, capitalised: "Palette". UNVERIFIED (no rig run): that this header over an empty list is the
  "single entry named palette" the owner sees.
- What it was meant to show: plan 2026-11-05-amarchy-theme-editor.md line 192, "Terminal Palette (the 16 v1
  terminal colours, reused rows)". These are the 16 SD_PALETTE entries (`choice_rows` nv = 16).
- Failing test: tests/test_keymap.c `terminal_palette_row_lists_its_16_colours`
  - `FAIL tests/test_keymap.c:3501: np = 0, want 16`
- Fix: the same as A. Caveat for the fix agent: this row has an editor id but store SD_UPTERM. Check that
  `lf_setting_set_colour` writes through lf_set_upterm to the real palette, and not through `gte_row_set_text`
  (UNVERIFIED). Files: amarchy/menu_tree.c, and possibly amarchy/leaf_core.c (store dispatch).

## B. Browsing a colour list turns every swatch on screen the same colour

- On main: UNVERIFIED. On main the swatch lists are empty (A), so the owner cannot browse a swatch colour list on
  the Swatches page there. The list the owner browsed is one of these: Settings > Palette (the real SDI_PALETTE
  page), a Groups/Elements list, or an older build. Ask which page and which build.
- No cursor-move colour preview exists in the core. `on_move` (menu.c:152-177) and `preview_sync`
  (gui.c:766-783) only preview themes. `gte_live` (gthemes_core.c:1726-1734) sends the model to the desktop only
  on an edit.
- Candidate (front end, UNVERIFIED): the drawn-menu chips in gui.c:552-566. Each chip calls `vr_pen_obtain`
  (render/pens_amiga.c:26, best-match shared pen), then RectFill, then `ReleasePen` right after on non-direct
  screens. A released shared pen can be reused and recoloured by the next obtain, so chips that were already drawn
  with that pen index change colour together. That fits "all swatches the same colour". Owner of the logic should
  be a pure per-frame pen set (obtain every chip pen for the frame, release on the next redraw), like the existing
  aa_ink scratch-pen pattern (render/aa_ink.h:11).
- Host test: none possible. The pen lifetime is in gui.c (Amiga graphics.library). A test belongs at the
  pure pen-set helper once one exists.
- A and B same cause: no (A is the menu_tree.c lookup bound; B, if the candidate holds, is front-end pen
  lifetime). UNVERIFIED for B.
