---
date: 2026-10-17
topic: cursor pass split into mask, off, on
status: measured on rig 2 (68060, --max), PROF=1 handler, branch feature/cursor-pass-split
---

# Cursor pass split

Question: where does the per-frame cursor pass cost go (mask, off, on), how many gfx calls does it make, and how often is the cursor cell unchanged?

## Method

- PROF slot 2 (whole pass) kept. New slots: prof_part[4] mask (vr_mask_begin + vr_mask_end), [5] off (vr_cursor_off), [6] on (vr_cursor_on). Extra EClock reads around each block.
- Counters in render/amiga_render.c (vr_cp[]): Text, RectFill, SetAPen/SetBPen/SetABPenDrMd, draw_rows calls, split into off and on; unchanged cell = same position, style, glyph and attributes as the previous on-draw.
- Counted only inside the two pass blocks. Cursor draws inside the flush (blink, scroll path) are counted separately and are not in the tables.
- Run: `UPTERM_RIG=2 UPTERM_RIG_FLAGS=--max python3 tools/rig/phase_rig.py` (rig 2 only). Log: scratchpad phase.log.
- Host test: tests/test_render.c `the_cursor_pass_counts_its_gfx_calls_and_unchanged_cells` (make test ONLY=render).

## Results (per frame, ms; calls per frame)

| shape | frames | mask | off | on | sum | whole (old slot 2) | off Text/Rect/Pen/rows | on Text/Rect/Pen | unchanged cell |
|---|---|---|---|---|---|---|---|---|---|
| sync-line | 200 | 0.00 | 0.03 | 0.04 | 0.07 | 0.07 | 0.05/0.95/1.00/1.00 | 1/1/2 | 0 of 200 (0%) |
| bytewise | 2 | 0.01 | 0.02 | 0.13 | 0.16 | 0.18 | 0.50/0/0.50/0.50 | 1/1/2 | 0 of 2 |
| scroll-nl | 2 | 0.00 | 0.06 | 0.10 | 0.16 | 0.16 | 0/1/1/1 | 1/1/2 | 1 of 2 (50%) |
| plain-lines | 1 | 0.00 | 0.06 | 0.00 | 0.06 | 0.06 | 1/0/1/1 | 1/1/2 | 0 of 1 |
| clear-page | 2 | 0.00 | 0.06 | 0.06 | 0.13 | 0.13 | 1/0/1/1 | 1/1/2 | 0 of 2 |
| wrap-long | 1 | 0.00 | 0.00 | 0.06 | 0.06 | 0.06 | 0/0/0/0 | 1/1/2 | 0 of 1 |
| sgr-colour | 3 | 0.00 | 0.02 | 0.03 | 0.05 | 0.05 | 0/0.67/0.67/0.67 | 1/1/2 | 0 of 3 |
| sgr-perchar | 1 | 0.00 | 0.00 | 0.06 | 0.06 | 0.06 | 0/0/0/0 | 1/1/2 | 1 of 1 |

Only sync-line has a usable frame count (200). The other shapes render 1 to 3 frames, so their ms values are single EClock quanta (about 0.06 ms) and are noise.

## Sum check

mask + off + on equals the old slot 2 within rounding on every shape (sync-line 0.07 vs 0.07; scroll-nl 0.16 vs 0.16; bytewise 0.16 vs 0.18, one EClock quantum, from the clock reads between blocks that fall outside the three parts). PASS within noise.

## Finding that contradicts the earlier note

The earlier ledger and 2026-10-16 note recorded about 2.7 ms per frame for the cursor pass. On the 68060 rig (--max) the pass costs 0.07 ms per frame on sync-line (15 ms over 200 frames). The 2.7 ms figure came from a slower configuration or a different divisor; it is not reproduced here. The cursor pass is about 1 percent of the 1585 ms sync-line run on this CPU. The 68020 (--stock without --max) was not re-measured.

## Which cut the numbers favour

On this data the cost is split roughly 40 percent off, 55 percent on, mask negligible. Skipping an unchanged cell would save nothing in sync-line (0 of 200 unchanged) and little elsewhere, so it is not favoured by these shapes; the cursor moves every frame when lines arrive. Skipping Text on a blank cell is also weak: on-draw makes exactly 1 Text per frame and the off path makes almost none. Folding off into the flush favours most: off makes 1 RectFill + 1 pen set + 1 draw_rows per frame that the flush repaints anyway, and the flush already redraws the row. The on-draw cost (1 Text, 1 RectFill, 2 pen sets) is the larger part but is unavoidable once the cursor moves. Recommended cut: fold off into the flush. The absolute gain is at most 0.03 ms per frame on 060, so the whole item is low value unless the 68020 figure is much larger; measure that before cutting.

## Caveats

- draw_rows internals that blit directly are not counted as gfx calls.
- The unchanged-cell snapshot is one static, valid with one window.
- Shapes other than sync-line have too few frames for ms values.
