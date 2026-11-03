---
date: 2026-11-03
topic: Kitty animation blinks, one frame shows only the top of the picture (K36-ANIM-BLINK)
tags: [kitty, animation, renderer, k36]
status: final
---

# K36-ANIM-BLINK: the picture blinks on a frame switch

## Symptom

Owner, on a rig, by eye (and on video): a looping kitty animation (earth-stream.bin: a=T root, 43 a=f
frames, every gap set to 90 ms with a=a,r=N,z=90, loop a=a,s=3) every so often shows one frame where only
the upper ~20% of the picture is drawn and the rest is background.

## Engine side: clean

- `kit_anim_next` / `vt_anim_tick` (engine/kitty.c ~1205-1260): the stream has no gapless frame (all 44 gaps
  90), the wrap 44 -> 1 returns frame 1, `cur` is never 0 or out of range.
- `kit_anim_show` (engine/kitty.c ~1078) copies the whole new frame into the placement's pixels in one go,
  then `mark()`s every screen row of the placement. No partial frame is ever visible to the renderer.

## Root cause: renderer, two passes over the damaged rows

render/amiga_render.c `draw_rows_now` (~3614): the row loop paints the text of every damaged row first --
which for a picture's cells is the background fill -- and only after the loop called
`draw_images(r, x0, y0, x1, y1)` for all rows, top down. A frame switch damages every row of the picture
(kit_anim_show's mark), so the window held background over the whole picture while the image runs went in
row by row. On the Amiga there is no double buffer: a display refresh landing during the image pass shows
the rows already written (the top) and background below. Intermittent because it depends on where the beam
is relative to that window; the bigger the picture (160 px = 20 rows here) the longer the window.

## Fix

`draw_rows_now` draws a row's images straight after that row's text (`row_images:` at the end of the row
loop; the VR_ASM fast row path's `continue` now jumps there). At most the one row being drawn is between
its background and its picture. Final pixels are unchanged (k37 golden hashes hold).

Residual (not fixed, not observed): within that one row the cells are still filled before the image
write; a beam catching exactly that row would show one 8 px band. Removing it means not painting the
background of cells a full-rectangle image write covers, which the RGB soft-blend path (reads back the
background) cannot do; left open.

## Test

tests/test_render.c `k36_a_frame_switch_never_shows_the_picture_half_drawn`: 3-frame 2x4-cell picture,
gaps 90, loop; 7 clock ticks (wraps included) through vt_anim_tick + vt_flush into the fake RastPort. The
log of each redraw is walked: a fill/text over a picture row blanks it, an image write draws it. Asserts
the clock switched 7 times (sentinel), the last row of the final frame has frame 2's colour, and at most
1 row was blank at any call (before the fix: 4).
