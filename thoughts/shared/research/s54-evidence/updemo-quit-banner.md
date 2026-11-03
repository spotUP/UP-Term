---
topic: UPDEMO-QUIT-BANNER-REPAINT evidence
rig: 93 (fresh-all install from main 1b2abd22, kit from upterm-ports main d94a685, reboot, then a normal `open` window)
date: 2026-11-03
---

# UPDemo quit: is there garbage in the banner after q?

Verdict: NO colour-streak garbage found on rig 93 after `q`, from scene 30 or from `UPDemo 16`.
The ledger claim ("colour-streak garbage until cleared", rig 96) is NOT reproduced; no post-quit
screenshot of rig 96 exists to compare with.

## What the banner is
Plain text: the ASCII-art logo the shell prints (system `tutorial` hint line included). No kitty/sixel/icat image.
In `before.png` the window area (x 4..796, y 22..420) has only 6 colours (background 14,14,20; white/blue text pens).

## Measured (screenshots in build/rig93/shots, copies in ~/Desktop/upterm-shots/rig93-quitbanner-*.png)
Window region x 4..796, y 22..420 (the 800x400 UP-Term window):
- scene16 (Kitty truecolour picture): 6428 distinct colours (the picture itself).
- scene30: 12 distinct colours.
- after-q (1 s) and after-q-5s: identical, 8 distinct colours, all pen colours (14,14,20 / 102,136,187 / 170,170,170 /
  0,0,0 / 255,255,255 / 128,128,128 / 0,205,205 / 122,162,247). No truecolour residue from the picture scene.
- after-q16 (`UPDemo 16`, q) and after-q16-5s: identical, 9 distinct colours, same pens plus (0,205,0).
- Pixels differing before vs after-q: bbox (4,22)-(796,239), 14834 px. These are not garbage: the window now holds the
  demo's own main-screen text (the reflow scene prints a paragraph to the MAIN screen on purpose: "this is the main
  screen, where a shell's text stays"), which scrolled the logo out of view. Text of the window by `screen_text`:
  "System: % resize / UP-Term keeps a line that wrapped as one line. ... (the size is in the title bar; Q quits the demo)".
  Cyan (0,205,205) at x5..56,y34..49 (heading), grey (128,128,128) at y146..161: all text-shaped, pen-coloured.

## Which path shows it
Neither quit path shows streaks. Both leave the reflow scene's main-screen paragraph on screen (by design) where the
logo was; after q the shell prompt follows it.

## Notes
- Tour ends itself after the last scene (Space on the last scene exits); q has to be pressed inside a scene.
- Keys were sent with `ami.py key 0x44` (Return) and `key 0x40` (Space); `ami.py type "..\n"` did not submit the line.
- A first, unclean run (Return not delivered) is discarded; shots in build/rig93/shots/old are not evidence.
- Not covered: rig 96's own state (not touched); visual judgement by eye (numbers only).
