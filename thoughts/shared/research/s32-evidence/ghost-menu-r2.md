# S32-GHOST-MENU round 2: which call dithers disabled menu ITEMS on 3.2 (rig 37, --os 32)

Status: answered. No fix designed or implemented.

## Answer
Disabled menu items (and the ghosted separator bar) are dithered by graphics.library **RectFill (LVO -306)**, called
from **Intuition ROM code at offset 102120** from the intuition Resident's rt_MatchTag (the SAME call site as the
round 1 bar-title ghost: one Intuition ghost routine serves titles and items, only rp->Layer differs), into a RastPort with **rp->Layer == NULL** (so no Layer->Window) whose BitMap is an **off-screen menu render
bitmap** (BytesPerRow 192, the same BitMap pointer for every item), with **AreaPtrn = {0x4444, 0x1111}, AreaPtSz 1,
FgPen 2, DrawMode JAM1 (0)**. One call per disabled item, rect x 15-176, 9 rows high. The menu is then copied to the
screen (that blit was not probed: not needed for this question).

These calls DO pass through our patch: flat_rf_c (amarchy/flat.c) receives every one. Round 1 missed them only
because its probe required rp->Layer != NULL. The existing outer-edge / scroller gate also requires rp->Layer and
Layer->Window, so today they fall through to the stock RectFill.

Gate line: in flat_rf_c, before the Layer/Window branch: rp->Layer == NULL && ret in [irom_lo, irom_hi) &&
AreaPtSz == 1 && AreaPtrn {0x4444, 0x1111} matches all item ghosts; the bar title ghost is the same call site
and pattern with rp->Layer = the screen bar layer (Window NULL). A match on (ROM caller, pattern words, AreaPtSz,
Layer NULL or Layer->Window NULL) covers both; no new LVO patch is needed.

## Probe (round 2)
Patch: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/286924bf-47a4-4018-b024-45e49a009a2e/scratchpad/ghost-probe-r2.patch
(round 1 patch widened, reverted from the tree). In flat_rf_c, right after `r->rcalls++`, every RectFill with
rp->AreaPtSz != 0 (any Layer, any Window, any caller) was logged to a 32-entry ring in flatshared: raw return
address, AreaPtrn[0], AreaPtSz, rect, Layer, Layer->Window, BitMap, FgPen, DrawMode, Layer bounds MinX/MinY,
AreaPtrn[1], BitMap->BytesPerRow. `amarchy-flat STATUS` prints the ring with irom_lo/irom_hi.
Binary build/amiga/amarchy-flat md5 8636c52561d6ee4232cfc425010c1cbf, put to RAM:, started with `Run >NIL:
RAM:amarchy-flat` (no prefs needed: the plain start installs the RectFill swap). Menu held: pointer (100,16) grab
space, right button down, STATUS read while held, screenshot, button up.

## Result (rectfill calls=60, pattern fills=10)
Raw STATUS lines: `mghost <total> <irom_lo> <irom_hi> <ret> <pat0> <sz> <x0> <y0> <x1> <y1> <Layer> <Window> <BitMap>
<fg> <drmd> <lx0> <ly0> <pat1> <bpr>`; irom_lo 16619460, irom_hi 16732788.

    mghost 10 16619460 16732788 16721580 17476 1 159   0 206   9 268777648 0 268541564 2 0  0  0 4369 640  <- disabled bar title, bar layer
    mghost 10 16619460 16732788 16721580 17476 1  15   2 176  10 0 0 268976308 2 0 -1 -1 4369 192          <- item
    mghost 10 16619460 16732788 16721580 17476 1  15  11 176  19 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15  20 176  28 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15  47 176  55 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15  65 176  73 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15  83 176  91 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15  92 176 100 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15 101 176 109 0 0 268976308 2 0 -1 -1 4369 192
    mghost 10 16619460 16732788 16721580 17476 1  15 110 176 115 0 0 268976308 2 0 -1 -1 4369 192          <- separator bar

ret 16721580 - irom_lo 16619460 = ROM offset 102120 for all 10. 17476 = 0x4444, 4369 = 0x1111. Item fills: Layer 0,
Window 0, one off-screen BitMap 268976308 (BytesPerRow 192; screen BitMap rows are 640 wide), FgPen 2, JAM1.
Only these 10 pattern RectFills ran while the menu was open (ring total 10).

## Pixel match (screenshot measured by script, no image read)
~/Desktop/upterm-shots/vtcon16-ghostmenu-probe2.png. Menu box screen x 93-258, rows 11-135. Ghosted bands = rows
where black text pixels have one x mod 4 residue empty (the 0x4444/0x1111 holes): 12-18, 21-27, 30-36, 57-63,
75-81, 93-99, 102-108, 111-117, separator 121-122. Solid (enabled) bands: 39-45, 48-54, 66-72, 84-90, 126-132.
Logged item rects (bitmap y 2, 11, 20, 47, 65, 83, 92, 101, 110) + 9 = the screen band tops 11, 20, 29, 56, 74,
92, 101, 110, 119: all 9 ghosted bands accounted for, none of the solid ones. Bitmap -> screen offset about
dx +82, dy +9.

## Candidates (tested / untested)
- RectFill, rp->Layer NULL, ROM caller: TESTED, this is the call (9 of 9 ghosted item/separator bands).
- RectFill with a Window layer: TESTED, no pattern fill with a Window while the menu was open.
- RectFill from a non-ROM caller: TESTED, none.
- BltPattern / BltTemplate / BltBitMap / BltBitMapRastPort / ClipBlit: NOT patched by amarchy-flat (it patches only
  sysiclass h_SubEntry, DrawBox -696, DrawFieldBox -702, Text -60, RectFill -306). UNTESTED and not needed for the
  dither; the off-screen menu bitmap reaches the screen through one of these (the copy carries the already-dithered
  pixels).
- Rig: one disabled-heavy Window menu (no drawer active, 8 items + separator disabled); 3.2.3 ROM, rig 37.
</content>
</invoke>
