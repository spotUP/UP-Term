# Kitty T-E (K30 z-index) evidence

Branch s51-kitty-te, worktree vtcon-s51-kitty-te.

## Host tests
- make test ONLY=kitty: `[OK] kitty: 249 checks, 0 failed`
- make test ONLY=render: `render: 618 checks, 0 failures`
- Two test fixes, no engine or renderer change: k30_row_images_go_by_z_then_id put four pictures in one cell, so
  each replaced the previous one; they now sit in four cells of one row. k30_far_below_stays_under_a_coloured_background
  compared a fill-path cell with a Text() cell and the cursor repainted the cell with bg pen 1; the green cells now hold
  the letter A and the cursor is moved away.

## Rig 63, RTG 800x600
- Rig ran this worktree's build. md5 of build/amiga/vtcon-handler = 5bf2ca38399622a2572da172a5eed65d, identical to
  build/rig63/vtc/vtcon-handler on the rig.
- Start needed --allow-stale-base (HEAD 8a4f7483 lacks origin/main commits; the main session rebases the branch).
- tools/rig/kitty_z_rig.py (red picture z=-1, C=1, text MMMMMMMM over it):
  - picture box x 20..83 y 66..81: red 480, glyph 544 (glyph colour 192,192,192)
  - first cell x 20..27: red 60, glyph 68
  - PASS (red and glyph both visible: the picture is under the text)
- Shot: ~/Desktop/upterm-shots/kitty-te-rtg.png

## AGA
NOT DONE. `rig.py aga256` timed out after 400 s (wasabi recv TimeoutError), the rig came back down, and a later
`rig.py start` reported "already running" while status said down and the wasabi connect timed out. No AGA pixel numbers.
The K30 renderer path is shared by both screens (render/amiga_render.c); AGA remains to be checked.
Rig 63 was stopped and cleared afterwards.
