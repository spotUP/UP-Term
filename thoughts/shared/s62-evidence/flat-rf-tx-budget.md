---
title: Flat RectFill / Text hook stack budget (S62)
status: gate PASS on all four entries (rf chain 320, zero slack); rig eye-check and border-button wait lock open
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

## After step 1 (shared helpers only; base e200e3e2)

| entry | own frame | chain |
|---|---|---|
| _c2_disp_reg | 20 | 312 (headroom 72) PASS |
| _c2_disp_stk | 4 | 296 (headroom 88) PASS |
| _flat_rf_c | 600 | 1252 (stub 28) FAIL |
| _flat_tx_c | 352 | 576 (stub 16) FAIL |

Frames over 80 then: flat_rf_c 600, border_button_at 552, flat_tx_c 352,
ghost_menu 180, icl_label 124, scroller_pen 104, raa_frame_draw 88.

## Step 2: flat.c restructure (this commit)

- border_button_at: no `flat_box sib[32]`; folds `flat_button_role_add` per
  sibling (same caps), returns `(role << 8) | ids`.
- Border-button fill split into bb_claim / bb_draw / bb_done. bb_claim takes a
  single static `flat_state bb_st` under Forbid with an owner task (bb_task);
  a second task that hits a border-button fill while another task is inside
  it gets the stock fill (behaviour change, only under contention).
- flat_rf_c is a dispatcher (frame 64): ghost, border button, rf_disabled
  (dim fill without `keep[4][4][4]`: corner rows per pixel, middle as spans),
  rf_scroller (scroller_pen / scroller_cuts / scroller_paint, sring_note),
  rf_edge / rf_side.
- ghost_menu: ghost_title uses an AllocVec RastPort.
- flat_tx_c: tx_title / tx_span fold `flat_title_span_add` over one
  `flat_gadrect` (no `gr[8]`); icl_label split into icl_box / icl_run /
  icl_write (lskip and return semantics unchanged).
- roundaa_core: raa_frame_draw corner-blend loop moved to static raa_corners.
- flat_glyph: flat_frame_draw_round keeps pens as `unsigned char pen[12]` and
  an `unsigned got` bitmask.

## After step 2 (`make flat-budget`)

| entry | own frame | chain |
|---|---|---|
| _c2_disp_reg | 20 | 312 (stub 0, headroom 72) PASS |
| _c2_disp_stk | 4 | 296 (stub 0, headroom 88) PASS |
| _flat_rf_c | 64 | 372 (stub 28, headroom 12) FAIL |
| _flat_tx_c | 32 | 304 (stub 16, headroom 80) PASS |

No helper frame over 80; no externs outside `_flat_*`. Overall gate: FAIL
(rf only).

rf deepest path: flat_rf_c 64 -> bb_draw 28 -> raa_frame_draw 52 ->
raa_corners 76 -> raa_nearest 52 -> pm_dist 36 -> 12, + stub 28 = 372.

Host suites pass: flat_core (2755 checks, incl. new fold tests
role_add/of == flat_button_role over every self and sibling set, and
title_span_add == flat_title_span over every gadget run), flat_role_core
(48), iclabel (256), richaa (13), flat_icl_pens (test updated for
`icx.cm`), flat_on_hang. build/amiga/amarchy-flat builds clean.

## Open (exactly what is left)

1. rf chain: 52 bytes over (372, must be <= 320). Candidates, in order:
   - raa_corners (76 = 36 locals + 10 saved registers): move the per-corner
     loop state into a small struct or split the per-pixel blend out, so the
     raa_nearest call happens from a frame under ~40.
   - raa_nearest 52 / pm_dist 36 (render/pen_match.h, shared with the
     terminal renderer): inline pm_dist or reduce saved registers; needs
     its own host test run (pen_match users).
   - flat_rf_c 64 (11 saved registers): call bb_draw from a thinner frame.
2. Host tests not written: flat_button_corner == flat_button_corners,
   flat_scroller_cut_at == cuts_box, flat_frame_draw_round pen bitmask (only
   0..255 pens taken).
3. Rig eye-check (not done; no rig in this task): installer border buttons
   (fill, rounded corners, labels), disabled button dim corners, scroller
   track and knob corner cuts, window outer edge and side strips, ghosted
   menu items and disabled menu titles, title accent band between system
   gadgets, icon labels (antialiased edges). Contention: two tasks filling
   border buttons at the same moment - the second draws stock.

## Gate weaknesses (not fixed)

- Argument bytes pushed at call sites are not counted.
- Calls through a register (`jsr (aN)`: c2_fill, c2_read, c2_rgb, put's fill
  callback, icl_pen_for via icl_blend_pens) are not followed.

## Step 3: raa_frame_draw split + pen_match (2026-11-08, worktree agent-ad60c5bcd9f17745f)

- `raa_frame_draw` split into `raa_frame_box` (box half, returns NONE/PLAIN/BLEND and the packed corner bg pens) and `raa_frame_corners` (corner blend, one pixel per `raa_corner_px` call so vbcc cannot hoist loop invariants into stack temporaries). Arguments travel in a `raa_job` struct. `raa_frame_draw` keeps its signature and calls both halves.
- `bb_draw` uses a static `bb_job`, held by the same `bb_task` claim as `bb_st`, so no `raa_frame_draw` frame sits under the corners.
- `render/pen_match.h`: `pm_dist` computes the opponent axes through `PM_OPP_U` / `PM_OPP_V` macros instead of calling `pm_opp`. Same arithmetic, one call level less.
- Drawing: pixel order (corner, row, column) and pen choice are unchanged by construction. Not verified on a rig.

`make flat-budget` after step 3 (stub included):

| entry | chain | headroom | result |
|---|---|---|---|
| _c2_disp_reg | 312 | 72 | PASS |
| _c2_disp_stk | 296 | 88 | PASS |
| _flat_rf_c | 332 | 52 | FAIL |
| _flat_tx_c | 304 | 80 | PASS |

No frame over 80. `flat_rf_c` children (rf 64 + 4 + child, stub 28 excluded; target <= 292):

| path | chain | needed |
|---|---|---|
| rf_scroller: l2079 48, l2031 56, l1785 32, flat_scroller_knob_img 32, flat_scroller_knob 28 | 304 | -12 |
| rf_disabled 64, dim_fill 76, dim_keeps 40, raa_cover 40 | 300 | -8 |
| bb_draw 24, raa_frame_box 68, flat_frame_draw_round 68, flat_frame_draw 44, flat_button_side 4 | 292 | 0 |

Tried, no gain: `b[]` in the stock call and one guarded block instead of the `on` flag in `flat_rf_c` (frame stays 64 = 20 locals + 11 registers).

Still open: the two paths above; host tests for `flat_button_corner`, `flat_scroller_cut_at` and the `flat_frame_draw_round` pen bitmask (pen > 255) are not written.

## Step 4: rf closes (2026-11-08, worktree agent-ad60c5bcd9f17745f)

- Scroller: `scroller_paint` returns the cut count and `rf_scroller` calls `sring_note` after it, so the sring chain no longer sits under the paint frame. `scroller_cuts` takes one cut per `scroller_cut` call (wraps `flat_scroller_cut_at`), so vbcc has no loop invariant to hoist (frame was 80).
- Dim: new `raa_partial_mask(w, h, r)` (roundaa_core) gives bit `y*4+x` set where `raa_cover < 255` in the corner square. `dim_corner` computes radius and mask once, from `rf_disabled`; `dim_fill` and `dim_keeps` test the bit instead of calling `raa_cover` per pixel. Same pixels: the four corner squares are mirror images (`rr <= w/2, h/2`), and `dim_keeps` is still called in the same order.
- `flat_frame_draw_round` pens over 255: the old `int pen[]` code put back any read pen >= 0; the byte array had narrowed that to 0 .. 255 and left larger pens as drawn. Restored: every pen >= 0 is put back as its low byte. On the Amiga that is the pen `c2_fill`'s SetAPen keeps anyway (RastPort FgPen is a byte), so drawing equals the old code. Only a host fill callback sees `p & 255` instead of `p`. Whether ReadPixel ever returns > 255 (RTG deep screens) is unverified; planar depth <= 8 gives 0 .. 255.
- Host tests (tests/test_flat_core.c): `test_round_cut_at` compares `flat_button_corner`, `flat_scroller_cut_at` and `flat_scroller_cuts_box` with reference copies of the 4e5cf2e5 array code (w, h 0 .. 40; knob boxes against shifted fill boxes); `test_round_partial_mask` checks the mask against `raa_cover`; `test_round_pen_byte` checks a read pen 300 is put back as 44, a read 3 as 3, an unreadable -1 left as drawn. With the `p <= 255` guard put back, `test_round_pen_byte` fails (7 corners wrong).

`make flat-budget` after step 4 (stub included):

| entry | own frame | chain | headroom | result |
|---|---|---|---|---|
| _c2_disp_reg | 20 | 312 | 72 | PASS |
| _c2_disp_stk | 4 | 296 | 88 | PASS |
| _flat_rf_c | 64 | 320 (stub 28) | 64 | PASS |
| _flat_tx_c | 32 | 304 (stub 16) | 80 | PASS |

rf children (stub excluded, target <= 292): bb path 292, scroller look-up path 292 (rf_scroller 28, scroller_pen 24, scroller_pen_of 60, prop_knob 32, knob_img 32, knob 28), dim path 268. Zero slack on rf: any new frame byte on the bb or scroller path fails the gate.

Host suites flat_core, richaa, iclabel, flat_role_core pass; `build/amiga/amarchy-flat` builds with no warnings.

Still open: rig eye-check of scroller knobs, disabled gadgets and border buttons (not run: no rig in this task); the border-button wait lock below.

## Border-button fill: wait instead of stock fallback (owner decision 2026-11-08) - NOT implemented

Owner asked that a second task wait for the shared border-button state instead of drawing stock, with the lock held only around reads/writes of the shared state, never across a graphics/layers call or anything that can wait.

Why not implemented:
- The gap is not closed yet (rf 332), and the decision was to be taken after it closes.
- The shared state (`bb_st` 1 x flat_state, `bb_job`) is not read once and dropped: `c2_fill`, `c2_read` and `c2_rgb` read it through `bb_job.s` during every RectFill/ReadPixel of the box and corners. So the lock would have to be held across graphics calls (forbidden: input.device can draw while a lower-priority task holds it, holder blocks on a layer lock input.device holds, screen deadlocks).
- The only deadlock-free form is copy-under-lock: copy the fields the draw needs to the caller's stack, release, draw. That puts a flat_state plus a raa_job on the stack of a path that is already 12 bytes over budget. The statics exist precisely to keep them off the stack.

Lock design if the budget later allows (for whoever implements it):
1. `ObtainSemaphore(&bb_sem)`; copy `r->st` fields used by c2_fill/c2_read/c2_rgb plus side and the four pens into a caller-owned block; `ReleaseSemaphore`. No graphics call between Obtain and Release.
2. Draw from the private copy only; `bb_job.s` points at the copy, never at `bb_st`.
3. The `bb_task` self-check in `flat_rf_c` (line ~1569) stays: it routes the fill's own RectFills to stock, independent of the semaphore.
4. Host test: fake Obtain/Release counters plus fake RectFill/LockLayer that assert the semaphore is not held when they run.

Until then the existing behaviour stays: a second task while the claim is held draws the stock look.
