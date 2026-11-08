---
title: Flat RectFill / Text hook stack budget (S62)
status: partial - gate extended, c2 + shared helpers fixed, flat_rf_c / flat_tx_c restructure open
date: 2026-11-08
branch: worktree-agent-ad60c5bcd9f17745f (base 4e5cf2e5)
---

# Flat RectFill / Text hook stack budget

## Defect

The graphics.library RectFill and Text patches (amarchy/flat_rf.s -> flat_rf_c,
amarchy/flat_tx.s -> flat_tx_c in amarchy/flat.c) run on the CALLER's stack.
tests/test_flat_budget.py only covered the two c2 image hooks, so these two
entries had no budget at all. Limits (unchanged): entry frame <= 64, any
reachable frame <= 80, chain <= 384 with >= 64 headroom (target <= 320), no
non-flat externs.

## Gate changes (tests/test_flat_budget.py, Makefile)

- ENTRIES now `_c2_disp_reg, _c2_disp_stk, _flat_rf_c, _flat_tx_c`.
- Stub bytes count in the chain: `stub_bytes()` counts `move.l x,-(a7)` before
  the `jsr <entry>` in the stub source plus the 4-byte return address
  (rf stub 28, tx stub 16).
- Static `lNNN` labels are namespaced per unit (`flat.s:l1136`). vbcc numbers
  statics per translation unit, so the old parser merged unrelated functions
  with the same label. That collision HID the real c2 glyph chain: with the fix
  the old code shows c2_disp_reg at 344 (headroom 40, FAIL), not the passing
  number the gate printed before.
- `make flat-budget` now compiles iclabel_core.c and render/emoji.c to .s too
  (FLAT_BUDGET_ASM), because flat_tx_c reaches them. No whitelist entries added.

## Before (new gate on base 4e5cf2e5 asm; FAIL)

| entry | own frame | chain |
|---|---|---|
| _c2_disp_reg | 20 | 344 (headroom 40) |
| _c2_disp_stk | 4 | 328 |
| _flat_rf_c | 600 | 1232 (stub 28) |
| _flat_tx_c | 352 | 680 (stub 16) |

Frames over 80: flat_frame_draw_round 280, flat_scroller_cuts_box 228,
icl_cache_verify 172, icl_coverage 92, raa_frame_draw 92, border_button_at 552,
icl_label 124, scroller_pen 104, ghost_menu 180. Extern: `_ip_any_free`.

## Fixed in this branch

- flat_glyph: per-index `flat_button_corner` and `flat_scroller_cut_at`
  replace the array-filling callers; flat_frame_draw_round no longer holds big
  arrays; the glyph plan struct geometry fields are `short`.
- flat_core: `flat_button_role_add/of` (fold form of flat_button_role) and
  `flat_title_span_add` (fold form of flat_title_span), ready for flat.c.
- iclabel_core: icl_cache_verify calls `is_free` directly (same semantics as
  ip_any_free, NULL is_free returns 0) - removes the `_ip_any_free` extern;
  icl_coverage uses a static per-pixel `icl_corners` helper.
- roundaa_core: `bgpen[4]` is `unsigned char`.

## After (current branch; still FAIL on rf/tx)

| entry | own frame | chain |
|---|---|---|
| _c2_disp_reg | 20 | 312 (headroom 72) PASS |
| _c2_disp_stk | 4 | 296 (headroom 88) PASS |
| _flat_rf_c | 600 | 1252 (stub 28) FAIL |
| _flat_tx_c | 352 | 576 (stub 16) FAIL |

Remaining frames over 80: flat_rf_c 600, border_button_at (flat.s:l1136) 552,
flat_tx_c 352, ghost_menu (l1816) 180, icl_label (l1493) 124,
scroller_pen (l1746) 104, raa_frame_draw 88. No externs left.

Host suites flat_core (2724 checks) and iclabel (256 checks) pass;
build/amiga/amarchy-flat builds clean.

## Open: flat.c restructure (designed, not written)

- border_button_at: drop `flat_box sib[32]`; pass 1 finds the exact box
  (caps i<64, n<32), pass 2 folds `flat_button_role_add` over border buttons
  with the same caps, then `flat_button_role_of(seen)`.
- flat_rf_c becomes a small dispatcher; each branch a helper:
  border-button fill passes `st_scr(...)` to raa_frame_draw instead of copying
  `flat_state st = r->st` (verify the fill path reads only pen_button,
  pen_back, pen_text, pen_line); disabled dim without `int keep[4][4][4]`;
  scroller via `flat_scroller_cut_at` loop, sring walk in its own helper;
  outer edge and side fill helpers.
- ghost_menu: the `struct RastPort trp` goes to AllocVec or a helper.
- flat_tx_c: tx_title uses `flat_title_span_add` with one flat_gadrect
  (no `gr[8]`); icl_label split into box / semaphore / run / write helpers.
- raa_frame_draw (88): split the corner-blend loop into a helper.
- New host tests: role_add/of == flat_button_role, title_span_add ==
  flat_title_span, flat_button_corner == flat_button_corners,
  flat_scroller_cut_at == cuts_box.

## Gate weaknesses (not fixed)

- Argument bytes pushed at call sites are not counted.
- Calls through a register (`jsr (aN)`: c2_fill, c2_read, c2_rgb, put's fill
  callback, icl_pen_for via icl_blend_pens) are not followed.
