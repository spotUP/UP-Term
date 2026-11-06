# S62 Tab / Return pixel checks (rig 167, fresh full install, origin/main 65ab6335)

Method: ami.py shot (640x256), PIL pixel colours only. Cell i at x = 5 + 8*i, 8x8 px. Window bg (26,27,38).
Colours: typed text (169,177,214); ghost (95,95,135); cursor (122,162,247); command word (158,206,106); prompt path (68,157,171).
Screenshots: ~/Desktop/upterm-shots/s62c-{calib,tab,return-x,cd-ghost,cd-after,final}.png

## A: single-match Tab -- PASS
Typed `Type RAM:tabprobe_z`; calibration shot shows ghost tail in (95,95,135).
After Tab: cells 37-39 hold `zz` plus space in (169,177,214); zero (95,95,135) pixels there.
Return printed glyph `x` at x=5, y=183-190 in (169,177,214).

## B: passive ghost not taken by Return -- PASS (by prompt), bare CD caveat
Typed `CD RAM:` without Tab: ghost tail (95,95,135) at cells 26-35, about 204 ghost pixels in the window.
After Return the new prompt line (y=199) reads `RAM Disk:` in (68,157,171), then typed `CD`.
It is 9 cells, not `RAM:rig-libs-done` (would be 17+ cells). So the ghost was not accepted.
Bare `CD` then returned the prompt to `Empty-12MB-FFS:` (y=207, 15 cells in (68,157,171)); it printed no directory line.
Caveat: the brief expected bare CD to print the dir; vsh bare CD appears to go home (Unix form). Unverified which; not a ghost effect.
Glyph reading is by eye from ASCII pixel maps; colour and cell counts are measured.

## Register row
2026-11-06 | rig 167 + origin/main 65ab6335 fresh full install | A PASS (zz+space 169,177,214); B PASS (prompt RAM Disk:, not rig-libs-done; bare CD goes to Empty-12MB-FFS:) | thoughts/shared/research/s62-evidence/tab-return-pixels.md
