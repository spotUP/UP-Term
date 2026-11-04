---
date: 2026-11-04
row: R3C-UD3-PLACEMENT-STALE
status: fixed on host; rig eye check open
---

# R3C-UD3-PLACEMENT-STALE: kitty a=p move / a=d delete leave the old picture drawn

## Root cause

`image_free()` in engine/vtengine.c bumps the slot's `gen`, so every placement
of the old image stops drawing (`place_image()` returns 0 on a gen mismatch),
but it damaged no cells. The front end only repaints damaged cells, so the old
pixels stayed on screen until something else dirtied those cells.

Callers that hit it:
- engine/kitty.c `kit_show` ("same placement again" path): a=p with an existing
  i/p pair frees the old placement image and places a new one; only the new
  cells are damaged.
- engine/kitty.c `kit_delete` / `kit_delete_where`: a=d frees the image; nothing
  is damaged.

## Fix

- New `image_mark(t, slot)` in engine/vtengine.c: walks every line of the active
  screen and `mark()`s the cells of each live placement of the slot.
- `image_free()` calls it before freeing, when `refs > 0` and `!t->img_nomark`.
- `vt_term.img_nomark` counter: raised in `free_screen()` and across the screen
  rebuild in `vt_resize()`. Without it, K29 child recursion during
  vt_free -> free_screen -> line_free -> place_release -> image_free walked
  already-freed lines (ASan heap-use-after-free seen on the first version).
- `kit_anim_show` in engine/kitty.c now reuses `image_mark()` instead of its own
  inline copy of the same loop.

## Regression test

tests/test_kitty.c `moved_and_deleted_placements_damage_their_old_cells`, using
new harness damage recording (`h_damage_clear`, `h_damaged` in tests/harness.c/.h).

- Before fix: kitty suite FAIL, 4 checks (test_kitty.c:359, 360, 365, 366).
- After fix: kitty 470 checks, 0 failed.
- Fix reverted (`&& 0` on the gate): the same 4 FAIL. Restored: OK.
- Other suites after fix: sixel, reflow, updemo, xterm, render all 0 failed.
  `make amiga` rc=0.

## Unverified

- Placements on the inactive screen or in scrollback are not marked; only
  `t->scr` is walked. Inactive screen repaints in full on switch, so likely fine.
- Cells keep their `VT_CELL_IMAGE` pad flag after the image is freed
  (render/amiga_render.c:2749 tests it). Not checked whether the Amiga renderer
  skips text in such cells after a delete.
- No rig eye check on 3.2 (rig 96 UD3); host damage test only.
