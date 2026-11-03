---
date: 2026-11-03
topic: zoom/depth flat clone looks clipped on the right (OS 3.2)
status: measured (rig 23, --max --os 32, intuition 47.53), cause proven; fix not written
---

# Clone clip hunt: zoom and depth

Probe: thoughts/shared/research/s32-evidence/clone-clip-zprobe.c (vbcc, same flags as build/amiga/dripens). It opens a
window with close/depth/zoom/size, prints each system gadget's box and image geometry, and draws (a) the real image and
(b) a private sysiclass object built exactly like amarchy/flat.c clone_object (marker DrawInfo, SYSIA_Which, IA_Width,
IA_Height) into an 8 plane bitmap at offset PAD=4, i.e. clone_render's offset 0 shifted 4 columns right.

## Geometry (rig 23)
```
== gad type 8015 flags 801C box le=-17 w=18 te=-9 h=10 | image which=2 le=0 te=0 w=18 h=10
   private image le=0 te=0 w=18 h=10
== gad type 8045 flags 8014 box le=-22 w=24 te=0 h=11 | image which=0 le=-1 te=0 w=24 h=11
   private image le=-1 te=0 w=24 h=11
== gad type 8065 flags 8014 box le=-45 w=24 te=0 h=11 | image which=1 le=-1 te=0 w=24 h=11
   private image le=-1 te=0 w=24 h=11
== gad type 8085 flags 8004 box le=0 w=20 te=0 h=11 | image which=3 le=0 te=0 w=20 h=11
   private image le=0 te=0 w=20 h=11
```
Depth (Which 0) and zoom (Which 1): real image LeftEdge = -1, and the PRIVATE object also gets LeftEdge = -1 from 3.2
sysiclass (IA_Width/Height respected, w=24 h=11 both). Close and size: LeftEdge 0 (why they look whole).

## Private object, offset PAD (image starts at col PAD+le = 3, not 4)
```
PRIVATE, offset PAD (= clone_render offset 0 shifted by PAD) (cols 0..31, image origin at col 4)
  00044444444444444444444444400000
  00054666666666666666666666500000
  00054666555555555556666666500000
  00054666588888888856666666500000
  00054666588855555555555666500000
  00054666588854444444445666500000
  00054666555554444444445666500000
  00054666666654444444445666500000
  00054666666655555555555666500000
  00054666666666666666666666500000
  00055555555555555555555555500000
```
clone_render draws at imp_Offset 0,0, so the image lands at x = -1: image column 0 goes off the left of the bitmap
(x=-1, a layerless bitmap: no clipping, the bit lands in the previous row's padding or before plane 0 for row 0), grid
column k holds image column k+1 and grid column 23 is empty. flat_clone_draw then paints the map at
ox = im->LeftEdge + imp_Offset.X, i.e. at the true image origin. Result, host flat_clone_map on the grids:

Depth, grid as clone_render reads it today (cols 4..27 of the dump):
```
n=98
........................
......................#.
....###########.......#.
....###########.......#.
....###############...#.
....###############...#.
....###############...#.
........###########...#.
........###########...#.
......................#.
........................
```
Depth, grid with offset -LeftEdge (cols 3..26):
```
n=89
........................
........................
.....###########........
.....###########........
.....###############....
.....###############....
.....###############....
.........###########....
.........###########....
........................
........................
```
Zoom today:
```
n=63
........................
......................#.
.....#############....#.
.....#######.....#....#.
.....#######.....#....#.
.....#######.....#....#.
.....#...........#....#.
.....#...........#....#.
.....#############....#.
......................#.
........................
```
Zoom with offset -LeftEdge:
```
n=54
........................
........................
......#############.....
......#######.....#.....
......#######.....#.....
......#######.....#.....
......#...........#.....
......#...........#.....
......#############.....
........................
........................
```

Today: the whole glyph is painted 1 px left of the stock position, and the image's right bevel column (image col 23,
SHADOW marker 5) arrives at grid col 22 where neither the right peel (col 23 is all 0, so edge_line stops at once) nor the
ring rule (x != w-1) removes it: a spurious full-height glyph-pen line at col 22 (rows 1-9). The right peel loop never
runs because the column it inspects is the empty one.

Candidates: (a) refuted as stated (private layout = real layout, same pixels, same le); the cause is the shared
LeftEdge = -1 that clone_render ignores. (b) refuted (the peel does not strip glyph columns; it fails to strip the
bevel because of the shift). (c) confirmed in part: le = -1 on depth/zoom; Width 24 equals the gadget box width.

Not checked: the owner's exact on-screen pixels with flat on (no amarchy-flat install on rig 23, no screenshot).

## Fix
amarchy/flat.c clone_render: draw the private image at m.imp_Offset.X = -((struct Image *)po)->LeftEdge,
m.imp_Offset.Y = -((struct Image *)po)->TopEdge, so image column 0 lands at grid column 0 (the same origin
flat_clone_draw paints at). The "its edges are 0" comment is wrong on 3.2.

## Second finding
With marker pens the depth back-window interior is marker 8 (BACKGROUND), not 0, so flat_clone_map maps it as glyph:
the back box paints solid (n=89 vs 74 on the real-pen fixture g_os32_depth). The fixture uses real pens (interior 0), so
test_clone_no_dividers does not see it.
