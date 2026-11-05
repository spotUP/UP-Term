---
date: 2026-11-05
topic: TE-8 Meta: presets / Reset to Base, contrast warning, pen budget
verdict: host-proven (TE-8.1, TE-8.2, TE-8.3 tests pass and fail when stubbed); m68k build ok; four rig eye checks open
---

# TE-8 evidence

Branch te8-meta. Code in amarchy/gthemes_core.[ch], gthemes_dos.[ch], gui.c, leaf.c; tests in tests/test_gthemes.c.

## What each item does
- TE-8.1: `gt_preset_model` reads user file, then shipped file, then (name tokyo-night) the built-in; Duplicate Theme (`gt_duplicate`) takes its values from it. `gte_reset_to_base` gives the edit buffer the base theme's model, keeping its own name and base, going live like an edit (`gt_dos_reset_base` on `GTA_RESET_BASE`).
- TE-8.2: `gt_contrast_pairs` / `gt_contrast_warnings` / `gt_contrast_elem_warns` measure 13 text-on-ground pairs with `gt_contrast` against GT_FLOOR_TEXT / MUTED / SEL. Summary row "N warning(s): <text> on <ground> too faint"; Elements rows get " (low contrast)". Save stays allowed (D8).
- TE-8.3: `gt_pen_budget`, `gt_budget_text`, `gt_budget_share_text`; "Pen Budget" row plus 10 "Pen Share <group>" rows, shown only for groups sharing a pen on a shallow screen.

## Tests (suite gthemes)
test_gthemes_te8_budget (nord depth 3/4/8/24 and jade-horse depth 3/8, exact numbers and share lines), test_gthemes_te8_contrast, test_gthemes_te8_presets, test_gthemes_te8_reset_base, test_gthemes_te8_rows (310 rows built; Pen Share rows visible 4 at depth 3 on nord, 0 at depth 8).
Pinned: nord d3 and d4 colours 9, shared 27, needing 3, fold 3, text "Screen 8 colours: 3 of 9 colours fold to the nearest of 8 pens"; jade-horse d3 colours 8, shared 28, needing 2, fold 2; d8 held 27, free 229, fold 0.

## Fail before / pass after
Stub (scratchpad te8-stub.patch: gt_contrast_warnings returns 0, gt_pen_budget reports zero pens, gte_reset_to_base does nothing):
  [FAIL] gthemes: 2184 checks, 61 failed
Reverted:
  [OK] gthemes: 2194 checks, 0 failed
m68k: `make build/amiga/amarchy-gui` rc=0, build/amiga/amarchy-gui 506820 bytes.

## Found
Every shipped theme measures at least 2 contrast warnings as raw colours: Inactive title text (nord 142 under 220) and Button Face on the requester ground (100 under 150, the runtime pushes these off the surface). So a stock theme opens with "2 warnings". Not changed (behaviour beyond TE-8); owner decision whether the table should skip those pairs or measure the pushed colours.

## Searched / reused
Searched gthemes_core.h and test_gthemes.c for existing contrast, budget, preset helpers: none. Reused tm_fs/tm_init/tm_read/tm_write, te4_open_builtin, te4_row, read_file + THEME_DIR, gt_resolve_rgb, gt_builtin, gt_model_from_v1.

## Open rig eye checks
- Pen Budget row on the Theme page.
- Pen Share lines on a shallow (8 colour) screen.
- "(low contrast)" suffix on an Elements row.
- Reset to Base live preview on the desktop.
