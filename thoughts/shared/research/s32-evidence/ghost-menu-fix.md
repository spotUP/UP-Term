# S32-GHOST-MENU steps (b)-(d): disabled menu items and titles in the dim colour on 3.2

Inputs: ghost-menu-r2.md (the call: ROM offset 102120, RectFill, AreaPtrn {0x4444, 0x1111}, AreaPtSz 1, FgPen 2, JAM1; items into
an off-screen interleaved menu bitmap with rp->Layer NULL, the title into the screen bar layer with Layer->Window NULL),
ghost-dither.md (case 6).

## Design (decided before coding)
- The ghost is drawn OVER the finished text, in the fill pen: JAM1 pattern pixels in FgPen punch period-4 holes into the label.
  So FgPen IS the menu background pen (evidence: FgPen 2 on every call; on screen the holes show the menu fill). No DrawInfo or item
  lookup is needed: background = rp->FgPen.
- Replacement: skip the stock fill; every pixel in the rect whose pen != background (label, shortcut, Amiga-key glyph, checkmark,
  separator line) becomes the dim label pen (pen_dimtext, DK_MUTED, the same pen the disabled-button label uses via
  flat_dimtext_ok); background pixels stay. A solid fill would erase the label, so a pure fill is wrong.
- Separator bar: its ghost rect holds the stock bar line; it is recoloured the same way, so it reads as a plain dim line like the
  other flat separators (no dither, no raised two-tone look). No special case.
- Guards: ROM caller, no Window on the layer, AreaPtSz 1, the exact pattern words, depth 3..8 with pen_dimtext a pen of the depth
  and != FgPen (flat_gm_pen); bitmap must be BMF_STANDARD (planar; RTG chunky keeps stock); the recolour core refuses a rect that
  is under half background (not a menu fill). Every refusal falls through to the existing stock path.
- Items (Layer NULL): recolour the bitmap planes in place (interleaved works: plane p row y at Planes[p] + y * BytesPerRow).
  Title (bar layer): ClipBlit to a scratch planar bitmap, recolour, BltBitMapRastPort back, so layer clipping holds when a window
  covers part of the bar.
- Sentinel: flatshared.mhits, printed as `mhits=` on the `rectfill` STATUS line.

## Reused (searched: flat_dim_pen, flat_dimtext_pen, flat_dimtext_ok, flat_gh_match, scroller_pen, the rsw.saved stock call)
- flat_dimtext_ok / pen_dimtext (the button ghost's label pen) for the pen and its depth rule.
- The irom_lo/irom_hi ROM caller gate and the fall-through to the stock RectFill already in flat_rf_c.
- No existing planar recolour helper in amarchy/ (grep Planes[ / BytesPerRow: only clone paths that read glyph masks); the core is
  new, pure, in flat_core.c.

## Code
- amarchy/flat_core.c: flat_gm_pen (match/decision), flat_gm_recolour (planar recolour core).
- amarchy/flat.c: ghost_menu() called first in flat_rf_c; STATUS mhits.
- tests/test_flat_core.c: test_ghost_menu_pen, test_ghost_menu_recolour.

## Host tests (2026-11-03)

`make test-one SUITE=flat_core`: `[OK] flat_core: 1949 checks, 0 failed`.

The first run caught a real bug through ASan (global-buffer-overflow in gm_pen_at): the
recolour core clipped x to `bpr * 8`, but in an interleaved bitmap BytesPerRow is the row
modulo across every plane, so the clip ran up to depth times past the bitmap's right edge.
Fix: flat_gm_recolour takes the pixel width explicitly; flat.c passes GetBitMapAttr(bm, BMA_WIDTH).

Revert proof (flat_gm_pen forced to return -1, and the recolour plane write skipped):

    FAIL tests/test_flat_core.c:1628: flat_gm_pen(&s, 8, 1, 0, 1, 0x4444, 0x1111, 2) = -1, want 1
    FAIL tests/test_flat_core.c:1629: flat_gm_pen(&s, 3, 1, 0, 1, 0x4444, 0x1111, 2) = -1, want 1
    FAIL tests/test_flat_core.c:1674: flat_gm_recolour(pl, D, bpr, ROW * 8, H, 4, 1, 15, 4, 2, 6) = 0, want 3
    FAIL tests/test_flat_core.c:1675: gm_px(pl, D, bpr, 5, 2) = 1, want 6
    FAIL tests/test_flat_core.c:1676: gm_px(pl, D, bpr, 6, 2) = 1, want 6
    FAIL tests/test_flat_core.c:1677: gm_px(pl, D, bpr, 9, 3) = 5, want 6
    [FAIL] flat_core: 1949 checks, 6 failed

Restored; suite green again.

## m68k build

`make build/amiga/amarchy-flat` (vbcc, -warnings-as-errors): clean, 124688 bytes.

## Rig proof (rig 37, OS 3.2.3, 2026-11-03)

Commit 24691a61 build, put to RAM:amarchy-flat, `Run >NIL: RAM:amarchy-flat`, pointer (100,16) grab space,
right button held, STATUS read while held, screenshot, button up. Started with
`--allow-stale-base` (branch lacks 2 origin/main commits; only RAM:amarchy-flat is under test).
Script: scratchpad gm_proof.py (same bands as round 2). Shot: ~/Desktop/upterm-shots/vtcon16-ghostmenu-fixed.png.

    STATUS: rectfill installed=yes calls=60 hits=0 shits=0 mhits=10 side=0 looks=0

mhits=10 = all 10 ghost RectFills of round 2 (9 item/separator rects + the bar title) took the new path
(so the off-screen menu bitmap does report BMF_STANDARD; the RTG guard did not block it).

Measured (menu bg (255,255,255)):
- 9 disabled bands (rows 12-18 ... 111-117, separator 121-122): every band has all four x mod 4 residues in its
  label pixels (round 2: one residue empty = the 0x4444/0x1111 holes). No period-4 pattern left. PASS.
- Disabled label pixels: one colour only, (0,0,0), 2190 pixels. pen_dimtext is pen 1 (DK_MUTED); this rig runs
  the stock palette (no Amarchy theme applied), where pen 1 is the black text pen. So the labels are the dim pen
  colour, but on this palette that colour equals the enabled text colour. PASS for "label = dim pen colour";
  the visible dimming needs a theme where DK_MUTED differs from text (not measured here; pen 1 RGB read from
  the enabled text, not from the colour map).
- 5 enabled bands: (0,0,0) text plus (170,170,170) shortcut/amiga-key pixels, same colours as before. Unchanged. PASS.
- Disabled bar title (x 159-206, rows 0-9): 138 pixels, one colour (0,0,0), all four residues. PASS (same caveat).
