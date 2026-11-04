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
