---
topic: dividers between window gadgets in DOpus 5 lister windows (flat theme)
status: static code reading only; no rig run, nothing measured on a lister (U = unverified)
---
# Findings

## Where dividers come from
- Stock images, not Intuition: sysiclass close/depth/zoom draw a 2 px SHADOW+SHINE separator column inside the image itself
  (OS 3.2 close cols 18-19 of 20; depth cols 0-1). s30-evidence/flat-gadget-no-dividers.md. V (measured there on 3.1/3.2).
- Already fixed for sysiclass gadgets: flat_clone_map edge-line peel, amarchy/flat_glyph.c:232-245 (edge_line) and :279-296
  (peel l/r/t/b up to a third of the box). Test: tests/test_flat_core.c test_clone_no_dividers (:2744; grids :2634-2650).
  Routed via flat_clone_route / flat_glyph_window_gadget (flat_glyph.c:206-229) from c2_draw (flat.c:258, c2_which_31 :598).
- Workbench windows use only sysiclass gadgets -> peel applies -> no dividers there by design. V by s30 rig proof.

## Why DOpus listers still show them (inferred, U)
1. DOpus iconify gadget (and lock) is a dopusiclass image (image_class.c IM_ICONIFY l.307; boopsi.c:470-510), not sysiclass.
   flat's hook does not see it: no UPIMAGE/SYSIA_Which, so the clone never runs; the image draws SHINE/SHADOW box lines with
   Move/Draw directly (research 2026-11-09-dopus-lister-flat.md l.39; flat.c header l.15-20 lists dopusiclass as unreached).
   flat_glyph.c:84 g_iconify (a plain bar) and FLAT_W_ICONIFY in flat_glyph_window_gadget exist but only for a sysiclass-style
   iconify gadget (Workbench/OS 3.2 has none by default) -> likely dead for DOpus. U.
2. DOpus lister system gadgets (close/depth/zoom) made by DOpus itself: if created before amarchy-flat started they stay
   ROM-drawn (s69 dopus-arrow-measure.md l.57,73: "images created before flat started stay ROM-drawn"). U.
3. Lister border arrows/size: listview_border THICK 2 px pairs (Library/listview_class.c:1435-1452) drawn unhooked; arrows are
   already handled by the s69 UPIMAGE table fix (dopus-arrow-measure.md). Title-bar dividers are separate from this.
Which of 1/2 the owner sees: not measured. Needed first: one lister on the rig, uitree dump of its title gadgets (type, class).

## Proposed fix (root-cause layer)
- Same layer as the existing fix: flat hook (amarchy/flat.c c2 dispatcher + amarchy/flat_glyph.c clone), NOT DOpus (vendor
  binary, Makefile:2140) and not the theme. For case 1: hook dopusiclass dispatch for IM_ICONIFY (and lock) and draw via
  flat_glyph (FLAT_W_ICONIFY, g_iconify with g->side for pixel aspect) with the clone/peel; for case 2: the existing
  UPIMAGE table path (flat.c c2_which_31) so the peel runs. Do not add a new peel; flat_clone_map already removes any
  full-span edge line, so a dopusiclass image routed into flat_clone_draw gets it free (square and hires: peel is in grid
  pixels, glyph scaling by g->side only).
- Test file: tests/test_flat_core.c (test_clone_no_dividers for the peel; add one routing case for the new which-resolution
  and a reachability test through c2_draw with sentinel). Rig check on a lister is the owner-visible proof.

## Rig 68 measurement (s69)

Setup: `rig.py reboot` exited 1 (its inner `start --keep-sys --no-solidwin --no-dopus` failed) but the rig came up anyway: wasabid 0.11b2 answered, DOpus5:DirectoryOpus process 11 started BEFORE amarchy-flat process 12. `VTC:amarchy-flat STATUS`: flat=on, dopus patched=7 flat=16, rectfill side=1. ENVARC:Amarchy/flat-starting deleted. No lister was open at boot (uitree after boot: only the Amarchy bar and the DOpus backdrop windows), so cause (2) was NOT testable on this boot. Earlier "OLD lister" data (dopus-arrow-measure.md) still stands: pre-flat lister keeps ROM pen p1 glyphs.

Fresh lister (`command DeviceList NEW` after flat), screen 640x256, window 0,14 444x229. Pens: A=(122,162,247) title fill/accent, d=(26,27,38) ground, k=(14,14,20).
Title-bar gadgets (uitree; no image class available, uitree gives only sys: type or "custom"):
| gadget | x,y,w,h | type |
|---|---|---|
| close | 0,14,20x14 | sys:close |
| drag | 0,14,444x13 | sys:drag |
| id 11 | 349,14,24x14 | custom |
| id 10 | 373,14,24x14 | custom |
| id 13 | 398,14,24x14 | custom |
| depth | 421,14,24x14 | sys:depth |
Border: size 426,233,18x10 sys:size; up 426,211 18x11 / down 426,222 18x11 custom; left 394,233 16x10, right 410,233 16x10 custom; prop 430,30 10x179 and 5 (4,235 388x6) custom.

Divider measurement (ami.grab, per-column scan of rows 14..27):
- Title bar x=335..420 (gadgets 11/10/13 and the boundaries 349, 373, 398, 421): every pixel is title fill A on rows 14..27 except nothing; NO vertical line at any boundary, and the three custom gadgets show no glyph at all (blank fill). Only the depth glyph (dark squares, x425..439) and, at x=422..424/443..445, the window right frame (d column at x=443/444 rows 28+ and k edge).
- Close boundary (x=20): no vertical line; only row 27 (title-bar bottom line, d) is different from fill over x=1..19.
- So: NO divider lines between title-bar gadgets in a lister opened after flat.
- Border gadgets: lines present are frame lines, not gadget-to-gadget dividers: vertical accent line at x=425 (y=29..232, left edge of the right border strip), horizontal accent line at y=232 (x=388..425, top edge of the bottom strip) and y=242 (bottom), right edge x=443 accent + x=444 k. Plus 4-pixel stubs at x=422..425 on rows y=218 and y=232 (ends of the frame lines at the arrow/size boundary). No vertical line between left arrow (394) and right arrow (410) and none between up and down arrow (no full-width line at y=222 either).

Verdict: neither cause (1) nor (2) is demonstrated in a fresh lister: the title bar has no dividers at all. The lines the owner may see are the frame lines above (accent x=425 / y=232 and the stubs), drawn by DOpus lister border rendering (not identified further; inferred, not traced to code). If the owner sees dividers between title gadgets, it is in a lister that existed before flat started (cause 2, not testable here: none was open at boot) - request: boot with a lister open (DOpus startup / `rig.py` with a lister in the DOpus config) to test.

Workbench redraw note: with a lister open the DOpus backdrop right of the lister (x=446..555, y=14..242) is one uniform colour (14,14,20, 25190 px), no icons exist on this rig's screen, so a failed redraw of icons is not observable here; a stale-redraw could not be reproduced or ruled out. A second lister was opened to compare; closing it failed: gadget click on its sys:close (gclick) only produced an extra untitled 79x26 window at 153,58 and the lister stayed; `command Close`, `Close ALL`, `CloseLister` returned rc 0 without closing it. Two listers are open at the end (not one).
