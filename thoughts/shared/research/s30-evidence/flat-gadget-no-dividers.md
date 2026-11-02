---
date: 2026-11-02
topic: Flat title-bar gadgets still show the stock divider / border lines (owner 2026-11-02)
status: fixed on branch s30-nodividers (flat_clone_map edge-line peel); rig proof on OS 3.1 and 3.2 below
---

# Flat gadgets: no dividers, no borders

## Cause
The divider is not drawn by Intuition's title bar code; it is part of the stock sysiclass image and
survives the glyph clone.

- amarchy/flat.c:258 (`c2_draw`) routes every window gadget to `flat_clone_draw`
  (amarchy/flat_glyph.c), which renders the stock image offscreen (`clone_render`, flat.c:160)
  and keeps as "glyph" every pixel `flat_clone_map` (flat_glyph.c:156) does not call ground.
- `flat_clone_map` dropped only pixels that are (a) the dominant inside pen or (b) on the 1 px outer
  ring AND in the marker SHINE / SHADOW / BACK pen. The 2 px separator is not on the ring:
  the OS 3.2 close image (20x11, measured in glyph-clone-probe-run.md, gadget 8085) is
  ```
  22222222222222222222
  23333333333333333312   <- columns 18 (shadow) and 19 (shine) = the separator
  23333331111133333312
  ...
  21111111111111111111
  ```
  Column 18 is inside the ring, so it is "glyph"; column 19 is on the ring but OS 3.2 sysiclass
  draws in its own pens, not our marker DrawInfo (probe B == E), so the pen test misses it too.
  Both columns are then painted in the glyph pen.
- Measured on ~/Desktop/upterm-shots/s30-glyph-clone-32.png (before, rig 55, OS 3.2, active
  window, band pen (102,136,187)): close image x 40-59, y 40-50; glyph square x 47-51 y 43-47
  (25 px); separator x 58-59, y 41-49 = 18 px in the glyph pen (170,170,170). The depth image has
  the mirror pair at its left (columns 0-1: shadow + shine, 9 rows each).

## Fix
`flat_clone_map` peels full-span edge lines: from each side inward, a column (row) whose every
pixel between the outer rings is drawn and not ground is a divider / bevel line and becomes box,
at most a third of the box per side. Pen-independent, so it holds whether sysiclass honours the
marker DrawInfo (3.1) or not (3.2). A glyph never spans the whole box, so no glyph pixel is lost.

Test: tests/test_flat_core.c `test_clone_no_dividers` (suite flat_core), the measured 3.2 close
and depth grids through `flat_clone_draw`: separator columns in the band pen, close glyph 25 px,
depth glyph 74 px.

## Rig proof
Regression check: with the two flat_glyph.c hunks reverted, `test_clone_no_dividers` fails
(tests/test_flat_core.c:1289, separator columns in the glyph pen); with the fix, flat_core passes
1172 checks, 0 failed.

Rig 56, fs-uae, worktree build amarchy-flat md5 ec0c98e07d0625722e1a6aa7f75da629 (read back from
VTCX:Apps/bin on the rig: same md5), prefs flat-system-look = on, flat-window-gadgets = on,
`amarchy-flat BOOT`, then `NewShell "CON:40/40/420/160/no dividers/CLOSE"` (active window).
Measured with a Python pixel dump of each shot, band pen (102,136,187), glyph pen (170,170,170).

OS 3.2 (Kickstart 47, 640x256), STATUS `clone draws=13 fallback=0`,
~/Desktop/upterm-shots/s30-no-dividers-32.png:
- close separator x 58-59, y 41-49: 18 / 18 px band pen (before: 18 px glyph pen)
- close glyph x 47-51, y 43-47: 25 px, all glyph pen
- depth left edge x 436-437: 18 / 18 band; depth glyph 74 px (equal to the host test)
- zoom left edge x 412-413: 18 / 18 band; zoom glyph 54 px

OS 3.1 (Kickstart 40.68, 800x600, title band y 40-52), STATUS `clone draws=5 fallback=0`,
~/Desktop/upterm-shots/s30-no-dividers-31.png:
- close separator x 58-59: 26 / 26 band; close glyph 30 px
- zoom left edge x 412-413: 26 / 26 band; zoom glyph 62 px
- depth left edge x 436-437: 26 / 26 band; depth glyph 86 px; depth right column x 459: 13 / 13 band

Rig 56 stopped and cleared after the runs.
