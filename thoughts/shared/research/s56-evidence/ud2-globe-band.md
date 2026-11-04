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

## Rig hunt 3 (2026-11-04, rig 114, OS 3.2 AGA 060, branch fix/ud2-band-root)

Double buffered? Partly. Planar AGA composes with the CPU into back planes (planar_begin) and back_copy blits them beam-synced. But two paths bypassed it, so the globe switch was drawn in place on the window:

1. engine/vtwin.c:657 anim_tick shows a clock-animation switch outside any ?2026 frame. sync_frames never moves, so vr_compose_begin (render/amiga_render.c ~3255, sync_seen check) skips the compose.
2. Inside a compose pass, draw_images called cpu_refuses (render/amiga_render.c ~2844) before every image run. On planar that set cpu_fail; compose_abandon (~3157) then drew the rest in place: row fill, then 1-row WriteChunkyPixels runs. The fill/image gap under the beam is the one-cell-row band.

Fix commit bcd7dce9: engine/kitty.c kit_anim_show (~1091) bumps sync_frames for a shown frame outside VT_MODE_SYNC; new back_chunky() writes indexed pens into the planar back planes, img_write_idx uses it in a CPU pass; draw_images refuses CPU only for RGB (fmt 1); under_text still refuses.

Host numbers (test s56_planar_globe_switch_reaches_the_window_in_one_copy): unfixed 97 in-place window writes per switch, 30/30 switches with no back_copy (planar_left 0 and 13); fixed 0 and 0 (739 checks pass); fix reverted 5 failures.

Rig numbers (screen grab about 1 frame/s, flat-run detector, globe box):
- Before (stock main handler): 0 of 28 globe frames show a band.
- After (handler md5 5942c128, installed + reboot): 0 of 19 globe frames by the flat-run detector (two UPDemo 15 runs). Static-mask detector flags 1 frame, row 91, top colours ocean blue/black: known ocean false positive.
- The grab is not discriminating: the band is sub-frame and a 1 s grab missed it before the fix too. The 40-frame target was not reached (19). Proof of the fix is the host test; the owner eye check in a live rig decides.

Shots: ~/Desktop/upterm-shots/s56-ud2-band-rig114-before.png, s56-ud2-band-rig114-after.png.
## Rig hunt 2 (s56, rig 110, sentinel build)

- Build: ab9cc6b9 plus debug sentinels, patch kept at `s56-evidence/ud2-band-sentinel.patch`.
  - The back bitmap is filled with magenta at every vr_compose_begin, so any presented rect not redrawn in the pass would show magenta.
  - img_buf is pre-filled with green before every soft-blend ReadPixelArray, so a read that leaves the buffer would show green.
- Run: `UPDemo 15` in an XCON window. 125 screenshots: 60 mostly later scenes, plus 25 and 40 back to back on scene 15.
- Result:
  - magenta 0 and green 0 in all 125 shots;
  - band detector (per 16 px cell row, pixels in the globe rect that are static in >80% of shots, deviation >30) finds 0 band rows over 63 globe frames.
- Verdict:
  - Suspect 1 (undrawn back rect presented) is not seen.
  - Suspect 2 (failed or short ReadPixelArray) is not seen.
  - The band itself did not reproduce on rig 110, so the cause is NOT proven.
- Remaining candidates:
  - A raw_scroll or to_front mid-pass while the globe updates. raw_scroll scrolls only the window, so the back bitmap is left with old rows, and a later present could copy one stale row.
  - The owner's band may need a scrolling or jump state (r->jump != 0) that UPDemo 15 alone does not create in a fresh window.
- Next probe: run the demo after scrolling output in the same window, or repeat the run with the sentinels under the owner's exact window and screen mode.
