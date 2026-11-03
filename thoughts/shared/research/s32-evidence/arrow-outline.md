---
date: 2026-11-03
topic: S32-ARROW-OUTLINE - which draw erases the flat outer ring next to the scroller arrows
status: root cause found, no fix
rig: 47 (OS 3.2, --max, amarchy-flat from main md5 45a4579e12de607738a5ab8319aceaeb, refreshed by rig.py start)
---

# S32-ARROW-OUTLINE

## Answer

The erasing call is amarchy-flat's own clone draw of the arrow image, not Intuition:
`flat_clone_draw` (amarchy/flat_glyph.c:299-304) fills the WHOLE image box with the box pen via `c2_fill`
(amarchy/flat.c:82-87, plain `RectFill`), called from the sysiclass hook at amarchy/flat.c:313.
For the down arrow of the UP-Term window: rect window-relative (622,214)-(639,224) (screen y 225..235), pen 0
(BACKGROUNDPEN = `frame_pen`, FLAT-SOLID-FILL, flat.c:300; band_rows 9999 so the single fill at flat_glyph.c:302).

Why the box hits the ring: the handler places each arrow at `GA_RelRight 1-sw`, `GA_Width sw`
(handler/sbar_gad.c:126-128 -> arrow(), sbar_gad.c:38-49), so the box spans x = W-sw .. W-1, i.e. it covers the
window's outer column(s). On this PAL hires screen `side=2` (FLAT-ASPECT-BORDERS), so the ring is x=638..639 and
both columns are inside the 18 px box (622..639).

Why it is not repainted: the ring exists only through the RectFill patch (flat_rf_c, amarchy/flat.c:759-768), which
recolours Intuition's four 1 px outer-strip fills (caller in Intuition ROM + exact strip match,
flat_outer_edge_fill flat_glyph.c:494). Intuition issues those strips only in a full frame redraw. A gadget-only
render (button press = SELECTED render + normal render; RefreshGList) never issues them, and c2_fill's own RectFill
fails both gates (caller is amarchy-flat, rect is not a full strip), so it runs stock. Nothing re-draws the ring.

## Measurements (PIL on full grabs, 640x256, ring pen colour (122,162,247), pen 0 = (26,27,38))

| shot | x=639 / x=638 rows 214..224 (up) | rows 225..235 (down) |
|------|------------------|------------------|
| before (window just opened) | pen 0 (gap) | pen 0 (gap) |
| after-click (up arrow) | gap | gap |
| after-refresh (activate other window, back) | ring | ring |
| after-click2 (down arrow) | ring | pen 0 (gap) |

Exactly the clicked arrow's box rows lose the ring; the other arrow keeps it. x=637 is pen 0 in all shots (border fill).
STATUS counters around one up-arrow click: `clone draws` 60 -> 62 (selected + normal render), rectfill `hits` (ring
recolours) 44 -> 44, rectfill calls +30. So the clone ran twice and no ring strip came after it.

Second trigger, same cause: at window open the handler's `AddGList` + `RefreshGList` (handler/sbar_gad.c:145-146)
draws the arrows AFTER Intuition's frame (and its outer strips), so a fresh UP-Term window already shows the gap
until the next frame refresh (activation change). The sys:size gadget (622,236 18x10) also overlaps the outer
column/bottom row; it is intact after a frame redraw only because it is drawn before the strips; a gadget-only
render of it would erase the same way (not measured).

## Fix direction (not implemented)

Fix in the clone path (the layer that erases), not in the handler: after `flat_clone_draw` returns FLAT_CLONE_OK
for a window-layer image (flat.c ~321), intersect the image box with the window's outer strips (reuse
outer_strip / flat_outer_edge_fill geometry with side_px(r, w->WScreen)) and refill that intersection with
`flat_edge_pen(&r->st, depth, active)`, through the saved RectFill. Equivalent alternative: clip the box fill to
exclude the outer side_px columns / bottom row. Either covers click, RefreshGList at open, and any
other gadget-only render, for every border gadget whose box reaches the window edge. Host test: flat_glyph test
with a box touching column W-1 and side 2 asserting the ring pen on those columns after the clone draw.

Shots: ~/Desktop/upterm-shots/s32-arrow-outline-rig47-{before,after-click,after-refresh,after-click2}.png
