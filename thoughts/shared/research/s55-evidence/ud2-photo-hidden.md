# R3C-UD2-PHOTO-HIDDEN: UD2 photo (z=-1) never draws under the earth

Cause: engine, not demo or renderer. `place_add()` in engine/vtengine.c dropped every image on a line that the new
image covered whole, in any layer. UD2 places the fjord photo (z=-1, i=6, 20 columns) and then the earth (z=0, i=1,
same columns, same row). The earth covered the photo whole on each row, so the photo's placement was released.
The photo's UNDER cell flags stayed, but no image view for it reached the renderer.

Measured on the host (demo stream through vt_write, cell 8x16):
- after the fjord transmit: 2 images, rows 3-9 carry a z=-1 view (col0=5 w=160 h=100).
- after the first earth a=T: z=-1 views gone, only the z=0 view (w=160 h=160) remains.

Fix: `place_add()` replaces only images in its own layer (image_under(o) == image_under(im)). Frame-after-frame
replacement in one layer is unchanged.

Test: `the_kitty_animation_scene_leaves_the_photo_under_the_text` in tests/test_updemo.c drives the UD2 scene through
the scene runner, feeds the capture to a fresh engine, and asserts z<0 views exist, their 20 cells per row carry
VT_CELL_UNDER, and the z>=0 earth is still there. Before the fix: `photo_rows > 0` fails (8684 checks, 1 failure).
After: 8684 checks, 0 failures. test_kitty (470 checks) stays green.

Unverified: `make amiga` build and the revert run of the final test file (the before-fix failure was measured with the
same test body before the debug probes were removed). Rig look on 3.2 not done (no rig use allowed).
