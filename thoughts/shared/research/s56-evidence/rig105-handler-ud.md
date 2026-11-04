# rig105: handler worker fix + updemo UD2/UD5 recheck (2026-11-04, main contains cf49f6e6)

Rig 105, fresh install from `make dist` (installer_rig fresh-all, all ticked, rebooted), non-PROF handler.
Shots (raw RGB) in ~/Desktop/upterm-shots/s56-105/.

## 1 Handler worker fix: PASS
- (a) kitty t=f, 64x64 PNG on T: (f=100): drawn; (b) t=t, T:tty-graphics-protocol-7.png: drawn. Screen pixels
  red 4096 + green 4096 = two 64x64 images (2048+2048 each). T: before [.., tty-graphics-protocol-7.png, kfA.png],
  after [kfA.png only]: the t=t file was deleted by the handler, the t=f file kept.
- (c) `/screen-text RAM:st.txt` in the XCON window: file written, holds "New Shell process 15 / 15.BOOTX:> Type VTC:kfs.bin ... /screen-text RAM:st.txt".
- No requesters, no guru/alert/87000004 in build/rig105/serial.log.
- PROF counters (kitty files read/deleted): not available, need a `make amiga PROF=1` handler; not built here.

## 2 UD5 splits: FAIL (keys still go to the new pane)
- `UPDemo 8` alone: the scene splits, zooms, unzooms and leaves the split open. Afterwards the demo window shows
  "UP-Term demo: make the window bigger / 76 x 20 needed. Q quits." (follow_size: left pane too narrow).
  Pane widths: right pane wraps at 46 columns (vsh `qEcho 123..` line broke after 46 chars); divider at x=380 of a 780 px window,
  so left pane about 46 columns too (8 px font); window about 95 columns unsplit.
- Key q after the scene: pixel diff left 866 / right 38888 changed px, and the right pane's screen text holds
  "SYS: % qEcho ..." / "vsh: qEcho: not found": the keys (q, and the echo line typed later) went to the new right pane, the demo never got Q.
  `exit` in the right pane let the demo go on to scene 9; then q quit it.
- Plain window: `/split right`, `/pane focus` typed in the new pane (sender = that pane, a no-op by design: it takes keys
  for the sender), then WWWW: left 0 px, right 460 px changed: keys in the right pane. The left pane's shell
  cannot send /pane focus from the keyboard; only the demo (left pane) can. So the demo-side focus is what needs checking: it did not hold after the scene.

## 3 UD2 scene 15 (kitty animation, fjord + earth): PARTIAL, not a clean PASS
- Status line from handler text: "Demo 15/35  Kitty graphics: an animation the terminal plays", scene 16 at 25 s.
- Shots at +7.9 s and +14.0 s after the Return (window open took the first 3 s; 5 s target missed), +25.1 s is already scene 16.
- Earth + photo block: x 44-203, y 82-241 (160x160): 6428 and 4136 distinct colours, 30384 non-background px in both shots; frames differ
  (animation running). Right of x 420: only text colours (5 colours, 2522 non-bg px), no picture: the photo sits only at the globe
  (c=20, row 4 col 6), not across the canvas. Photo colours present under/around the globe in the left block; 12176 px pure black in the block.
  Visual judgment of "fjord behind earth" is unverified (no image read, pixel counts only).
