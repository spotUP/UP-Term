# S56-UD2-EARTH-OVER-PHOTO evidence

Branch s56-ud2-earth-photo, commit 6cdc696d (generator alpha, updemo layout, tests).
Tests: make test-one SUITE=updemo pass; tests/test_gen_updemo_earth.py pass (corner alpha 0, centre 255).

## Rig 107 (UPDemo 15, 76x20)
- Photo visible beside the globe block, text below it: layout PASS.
- Globe animates: pixel diff between shots ~1 s apart = 8852 non-zero px: PASS.
- Black corners: FAIL. Black px in bbox corners: TL 472/529, TR 480/529, BL 488/529, BR 486/529.
  Pixels 6 px left/right/above/below the globe block are (0,0,0): the photo does not show through the transparent corners.

## Cause (read from source, not yet proven on the rig)
render/amiga_render.c: when the renderer has no CGX (vt_set_image_rgb(t, r->cgx != 0), line 642) the picture is
stored as indices (engine/kitty.c kit_index: alpha 0 pixel -> index 0). img_colours sets ctab[0]/map[0] to the
background and img_run writes every pixel including index 0 (WriteChunkyPixels / write_lut), so the transparent
corners paint opaque background over the z<0 photo. The RGBA path (fmt 1, img_run_rgb) blends correctly.
Fix needs: vt_image_view gets a flag for kitty pictures; img_run skips index-0 runs for z >= 0 kitty pictures
(sixel keeps bg). Engine + renderer + Amiga rebuild + regression test: not done (out of budget, scope beyond the brief).
Which path rig 107 used (CGX or not) is unverified.

## Fix (commit b5a59af6) and rig 108 check
Cause measured by host test: with the fix reverted the globe corners read the background (268435456), with the fix the
photo (268435711). Fix: vt_image_view.kitty; img_run skips index-0 runs of kitty pictures (img_write_idx_keep); sixel keeps bg.
Host test s56_transparent_kitty_pixels_keep_the_picture_beneath_on_the_palette_path (CGX LUT path) fails before, passes after.
Rig 108 (dist built from this branch, assets copied by ami.py put into UP-Term:share/updemo, UPDemo 15 in XCON 760x440):
UNVERIFIED. The demo ran and scenes advanced, but no shot showed globe plus photo together; the corner counts
(TL/TR/BL/BR of 529) were NOT measured. Shots: ~/Desktop/upterm-shots/s56-ud2/. Needs a rerun with a fresh install
(rig.py install / full kit) so share/updemo is present from boot, and a shot taken in the scene's globe phase.
