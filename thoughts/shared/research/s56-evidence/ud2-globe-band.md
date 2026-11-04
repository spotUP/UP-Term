---
date: 2026-11-04
topic: UD2 globe horizontal band (UPDemo 15 kitty animation)
status: cause NOT proven; host cannot reproduce; rig check not run (tool budget)
branch: fix/ud2-globe-band (guard test only, no fix)
---

# UD2 globe band: findings

Owner report: in some frames of UPDemo 15 one band, one text cell row tall, crosses the whole
globe picture width including the transparent corners. Inside it the content is different
(whitish and fuzzy on the left, a bright blue diagonal streak on the right). Intermittent.
The asset is ruled out.

## Host repro: negative

Test `s56_globe_frames_over_a_photo_composite_exact_every_switch` (tests/test_render.c).
It uses a 160x64 opaque RGBA photo at z=-1, C=1, and a 12-frame 64x48 RGBA animation with
binary disc alpha at row 3, column 7, looped with s=3. It runs 30 clock ticks and compares
every globe pixel against the exact composite after each switch. It covers:
- soft blend (gf_cgx_alpha(0), the rig's P96 42.7 path) and WritePixelArrayAlpha;
- the switch inside an open ?2026 frame, just before an empty ?2026 frame (UPDemo's
  loop: demo/updemo.c:2774-2783), and alone (in place).

Result: 0 bad pixels in every case (render: 729 checks, 0 failures). So these are correct
on the host fake:
- the engine path: kit_anim_show engine/kitty.c:1091, image_mark, vt_row_image;
- the per-row draw order: draw_rows_now render/amiga_render.c:3644, then row_images and
  draw_images at 3921-3928;
- img_run / img_run_rgb at 2616-2760;
- the composed pass into the back bitmap.

## What the band content implies

A soft-blend fault limited to the read (stale img_buf because ReadPixelArray fails or reads
the wrong target) would change only the corners. Disc pixels have alpha 255 and replace
whatever was read. The owner sees different content across the disc too. That points to one
of two things: a cell-row rectangle reaching the window from somewhere that was not drawn
this pass, or the frame source being wrong for that row.

## Open suspects, ranked (all unverified)

1. Rig-only back-bitmap path: back_present / back_copy (render/amiga_render.c around 3060-3110),
   with blits of rects from r->back->bm (friend bitmap) whose row was not redrawn in this pass.
   That would show stale content from an earlier scene in the back bitmap. The host fake
   shares one canvas model and may not model a P96 friend bitmap that keeps old pixels.
   Check this first on the rig.
2. ReadPixelArray return value ignored (amiga_render.c:2650). A failed or partial read leaves
   the previous run's img_buf. A run of a different width gives sheared (diagonal) content.
   This explains the corners and the diagonal streak, but not the disc (see above).
3. The anim tick render (render/vtwin.c anim_tick around 656, which calls vtwin_render)
   colliding with a held ?2026 frame or a scroll jump offset (r->jump in gy).

The planar VR_ASM fast path (amiga_render.c:3729) is ruled out: it is planar-only and the rig is RTG.

## Next step (rig 110+, per brief)

Fill the back bitmap with a sentinel colour (for example 0xFF00FF) right after it is
allocated and after each scene change. Run UPDemo 15 on rig 110. Grab frames via
wasabi and count sentinel pixels and band rows inside the globe rectangle in a script.
- Sentinel pixels in the band prove suspect 1.
- Sheared copies of the previous row prove suspect 2. Log the ReadPixelArray return
  value next to each frame grab.
