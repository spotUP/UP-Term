---
date: 2026-11-02
topic: flat window gadgets invisible on rig 8 (3.2, vantablack, flat on)
status: root cause found (static + pixel evidence); rig ordering proof open
main: 67bdb62a
---

# Flat window gadgets invisible: root cause

## Pixel evidence (scratchpad rig8-ram2.png, PIL)
- Active RAM Disk window x 113..533, title bar rows 25..34: only two colours, #8d8d8d (band) and #000000 (title text,
  "Ram Disk ..." starting at x 144). Columns 113..143 and 490..533 are pure #8d8d8d in every title row: no gadget box,
  no glyph pixel, no bevel. Interior black from row 35.
- Black text on grey = the FLAT-TITLE-ACCENT path (band pen_outline = DK_ACCENT, text pen_ptext = DK_BG), not stock
  Intuition (stock flat map draws the title in FILLTEXTPEN = DK_FG on FILLPEN = DK_BG).

## Premise correction
The grey title bar is not a violation of FLAT-SOLID-FILL: FLAT-TITLE-ACCENT (todos.md:3334, owner s27 "accent on title
bar ONLY", landed c036836a / 3f47e564 / fcf305ef) superseded "frames BG in both states" for the active title bar.
The pen map (SHINE=SHADOW=FILL=BG) is not involved.

## (a) Who draws what on 3.2 with flat on
- Gadget glyphs: the amarchy-flat sysiclass hook, c2_draw (amarchy/flat.c:197-262): clone route
  (flat_clone_draw, amarchy/flat_glyph.c:197-222) or block fallback (flat_glyph_draw).
- Title band + title text: the amarchy-flat graphics Text patch, flat_tx_c (amarchy/flat.c:402-436), which paints the band
  with the saved RectFill before the title string.
- Frame fill/sides: stock Intuition (FILLPEN/BACKGROUNDPEN via the flat pen map).

## (b) Pens
- Active gadget box = band pen (flat.c:233-237: frame_pen = flat_frame_pen = pen_outline = DK_ACCENT), glyph =
  pen_ptext = DK_BG (flat.c:237, flat_glyph.c:209). Box and band are equal on purpose; glyph (black) differs from both.
  So the pens would give a black glyph on a grey box. The pens are NOT why the glyph is invisible.
- Title band = pen_outline (DK_ACCENT, flat_core.c:518-521), title text = pen_ptext (DK_BG).

## ROOT CAUSE: the band erases the gadgets
flat_title_decide sets the band to the FULL title bar width: amarchy/flat_core.c:516
`t->bx0 = 0; t->by0 = 0; t->bx1 = win_w - 1; t->by1 = bar_h - 1;`
and flat_tx_c fills it on every title Text call (amarchy/flat.c:415-419). The close, depth and zoom gadgets live inside
that rectangle. Any title Text call that comes after the gadgets were drawn (activation redraw order on 3.2, and every
SetWindowTitles, e.g. Workbench updating "Ram Disk  x% full") wipes them to solid band grey; Intuition does not redraw
the gadgets after the title, so they stay erased. The route note itself flagged this as an unchecked assumption:
thoughts/shared/research/s28-evidence/title-accent-route.md:20-21 "Order assumption (border, title text, then gadgets)
is checked by the rig pixel read" -> the rig pixel read now falsifies it. The host test encodes the bug:
tests/test_flat_core.c:1032 expects bx1 == 199 (full width).
Inactive windows draw no band (flat_core.c:522-524), so the bug shows only on the active window: matches the symptom.
Unverified: which exact call (activation vs SetWindowTitles) erased them on rig 8; rig 8 is unreachable now.

## (c) Hook installed? STATUS hang?
- The sysiclass hook is installed: rf_install/tx_install run only after hook_install returns FLAT_OK
  (amarchy/flat.c:1162-1171), and the Text patch demonstrably ran (black text on accent band).
- "frame hook calls=0 flat=0" is the C8 frameiclass hook line (flat_frame_status_line, flat.c:735-738; counters
  fcalls/fflat from c8_skip flat.c:301-345), not the sysiclass hook. calls=0 means no frameiclass IM_DRAW reached it
  (GadTools bevels on 3.2 not via that slot, or none drawn); unrelated to window gadgets. The sysiclass counter is
  st.calls (the "flat=on N calls" field).
- STATUS hang: print_status (flat.c:652-740, ipd_status_line iconpen_dos.c:229-247) takes no lock and waits on nothing
  (Forbid/FindSemaphore/copy/Permit/PutStr). The hang is not in amarchy-flat's STATUS code. At 2026-11-02 rig 8's
  wasabid refuses connections (`rig.py status` = down) while its fs-uae runs at ~100% CPU: the Amiga side (or its
  wasabid) is wedged, which also explains the exec not returning. Cause of the wedge unverified (rig 8 not touched).

## (d) Fix (root-cause level)
The band must own only the pixels Intuition gives to the title, never the gadget boxes. flat_title_decide should take
the title span between the window's title-bar system gadgets (left edge = right end of the close gadget, right edge =
left end of the leftmost depth/zoom/iconify gadget; GTYP_WDRAGGING excluded, GFLG_RELRIGHT resolved against Width) and
flat_tx_c should pass those from w->FirstGadget, so the band fill [bx0,bx1] never intersects a GTYP_SYSGADGET rect in the
title bar. The gadget boxes already get the same band pen from c2_draw (flat.c:233-237), so the bar still reads as one
accent band with black glyphs. Host test: band span excludes each gadget rect (replaces test_flat_core.c:1032), fails on
revert; rig proof: active window title-bar end columns contain DK_BG glyph pixels after activation AND after a
SetWindowTitles (Workbench "% full" refresh).

## Live hunt (2026-11-02, rig 25, OS 3.2, flat-system-look + flat-window-gadgets on)

Question: what paints the active window's close/depth/zoom boxes as one solid colour with no glyph?

Evidence (diagnostic counters added to flatshared, read with `amarchy-flat STATUS` after opening a CON: window):

- `clone draws=13 fallback=0` and `clone diag zero=13 which=2 glyph=0 mask=1`: every clone draw of a window
  gadget produced an EMPTY glyph map. The offscreen grid held only value 0 (untouched).
- The readback works: SetRast 9 then ReadPixel gives 9. The stock sysiclass dispatcher returns 1 for IM_DRAW (MethodID 0x202).
- Passes tried, all drew nothing (value 0 on all 13 draws): layerless offscreen rastport + marker DrawInfo; a layered
  rastport (NewLayerInfo + CreateUpfrontLayer LAYERSIMPLE) + marker DrawInfo; the layered rastport + the original
  DrawInfo; the offscreen rastport with imp_Offset (0,0) + original DrawInfo. Geometry of the last clone: image 18x10
  at 0,0, real imp_Offset 403,55, imp_State 0.
- So OS 3.2's sysiclass returns success for the window gadget images but renders nothing into a foreign rastport.
  flat_clone_draw then filled the whole box with the box pen, found no glyph spans, and returned FLAT_CLONE_OK.
  That is the solid box. The title band (fixed in 770f4c1b) is not the cause of this symptom.

Fix: flat_clone_draw treats a map with zero glyph pixels as FLAT_CLONE_FAIL before it paints anything, so c2_draw
falls back to the block glyph drawing (counted as clone fallback). Host test: test_clone_arrows, blank_render case.

Open: why 3.2 sysiclass draws nothing offscreen is not known (cached imagery tied to the screen is the likely reason,
unverified). Rig pixel proof of the fallback glyph after activation and re-activation is not yet measured.
The diagnostic patch is kept out of the tree.
