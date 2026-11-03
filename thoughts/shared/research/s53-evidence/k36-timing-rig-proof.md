---
title: K36 timing rig proof (kitty animation, one switch per wake), AGA and RTG
date: 2026-11-03
session: vtcon-ee, rig 91, main f1f1fe5a
---
# K36 timing rig proof

Verdict: animation on plays and animation off stops, on AGA and on RTG. Strict tool verdict for "off" is FAIL
on both screens by a handful of pixels outside the picture (see below); the PROF switch count is 0 in all off runs.

Setup: main f1f1fe5a, `make amiga` clean, handler rebuilt with PROF=1 (514760 bytes) and pushed to rig 91.
Tool: tools/rig/kitty_anim_rig.py (64x64 four-frame picture, 100 ms gaps, 4 shots about 150 ms apart), whole run
held under paths.long_run(). Two cold starts: the first `rig.py start` hit the boot freeze (daemon silent 150 s);
`rig.py reboot` brought it up.

## Neighbouring-shot pixel diffs and PROF switches (PROF "kitty anim switches", first number, hex)
| case | screen shot | neighbour diffs | switches |
|---|---|---|---|
| RTG on (run 1) | 800x600 | 0, 31, 2112 | 0x53 = 83, then 84 |
| RTG off (run 1) | 800x600 | 0, 0, 8 | 0, 0 |
| AGA on | 640x256 | 4130, 4096, 4096 | 0x44 = 68, then 69 |
| AGA off | 640x256 | 777, 0, 5 | 0, 0 |
| RTG on (run 2, after rtg switch) | 800x600 | 4096, 4096, 3856 | 0x4f = 79, then 80 |
| RTG off (run 2) | 800x600 | 16, 0, 0 | 0, 0 |

The off diffs are not the animation: the 8 changed RTG pixels sit in a 5x5 box at x 456-460, y 592-596, outside
the 400x200 window (mouse pointer residue), and the AGA 777 differs once and then stays equal (window repaint
after the click), while the handler counted 0 frame switches. The tool's strict criterion (all shots equal) is
therefore not met in the off case, the property (no frame switches) is.
Run 1 RTG on shows 0 for the first pair: two shots inside one frame gap, 83 switches in the window life.

## Frame clock idle between frames at a 1 s gap
Not measured: the tool uses 100 ms gaps only. Unverified.

## Left up for the owner (rig 91, RTG 800x600)
Window "kitty animation" (XCON:0/20/400/200) plays the loop; two shots 0.3 s apart differ by 1984 pixels in it.
Window "24-bit picture" (XCON:410/20/660/460) runs `VTC:icat VTC:photo.png`; 52249 unique RGB values in its
area (screen is 800 wide so the window is clipped at the screen edge). icat on the rig is today's build (md5
e6abe4bb327b9cc60559ca6adf720887 equals build/amiga/icat). icat's exit code was not checked (unverified).
Replay: `Type VTC:kanim-stream.bin` in any XCON shell.
