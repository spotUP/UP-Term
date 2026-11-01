---
title: s29 rig 8 shows an orange backdrop and black icon boxes with flat on - cause measured, fix not landed
status: cause-measured
date: 2026-11-01
---

# Why rig 8 does not look flat

Rig 8: FS-UAE, OS 3.2, Workbench 640x256 depth 8 (256 pens), build main a586f6c9. Prefs: desktop-theme on,
flat-system-look on, flat-window-gadgets on, amarchy-gui-theme vantablack. Shot: ~/Desktop/upterm-shots/s29-flatvis-r8a.png.

## Hypotheses

- (a) shot palette differs from the screen: NO. wasabid GRAB (build/wasabi/src/wasabid.c 5207, 5577) and tests/amiga/dripens.c
  both read `GetRGB32(screen->ViewPort.ColorMap)`; same source.
- (b) the WBPattern: YES, this is the backdrop cause (below).
- (c) IPrefs did not reset: NO. Pens 0-7 carry the theme, DrawInfo pens are the flat map `0 1 2 0 0 0 2 0 2 2 0 1`.
- (d) amarchy-flat not running: NO. Icon pens 32-47 hold the theme ramp (909090 BEBEBE BEBEBE 8D8D8D 808080 A7A7A7 ...),
  which only the amarchy-flat pen holder sets.

## Numbers

Shot histogram (640x256 raw): EE9900 133886 px, 000000 23955, BEBEBE 3556, 222222 1825, A7A7A7 359, 909090 259.
First orange row y=22 (menu bar + Amarchy bar above are black).

`VTC:dripens Workbench` palette: pens 0-7 = 000000 222222 BEBEBE 8D8D8D 1A1A1A 7A7A7A B6B6B6 7A7A7A;
pens 249-255 = A4A4A4 B6B6B6 505050 EE4444 55DD55 0044DD EE9900. EE9900 occurs at pen 255 only (checked over all 256
entries), so every backdrop pixel is pen 255.

ENV:Sys/WBPattern.prefs ROOT chunk: depth 3, all 96 data bytes FF = tile pen 7 (desk_backdrop_pen_at(3) = DK_RED = 7).
ENV:Sys/Palette.prefs ColorSpecs: 0 000000, 1 222222, 2 BEBEBE, 3 8D8D8D, 4 EE4444, 5 55DD55, 6 0044DD, 7 EE9900.

## Cause 1: backdrop orange

Workbench maps a 3-plane WBPattern tile's pens 4-7 the same way IPrefs maps palette.prefs entries 4-7: to the LAST four
pens, 2^depth - 8 + i. Tile pen 7 is drawn as screen pen 255. Amarchy colours screen pen 7 live (dk_live_pens /
desk_live_follow, here 7A7A7A = muted because desktop-theme is on, desk_pen_colour own=0) and leaves palette entry 7 at the
user's stock 3.2 EE9900 (desk_palette_theme keeps entries that reach 252-255). Nothing draws pen 7, so the theme never
shows; pen 255 = EE9900 fills the desktop. At depth 3 the two coincide, which is why depth-3 reasoning holds and depth 8 fails.
desk_core.h:65 ("a WBPattern tile is 3 planes, so it is below 8") assumes tile pen == screen pen; that assumption is wrong
on a deep Workbench. The same mistake flat-palette.md found for palette.prefs (defect 2) applies to the pattern.

## Cause 2: black box around every icon

The icons ARE the flat lucide images (ASCII render of the hard drive icon at 26,99: outline in A7A7A7/909090/BEBEBE,
32x16, tall-pixel variant; amarchy-icons skipped-done=304 is correct, the files on disk carry the written image crc).
Their background pixels are image colour 0 = pen 0 = theme bg 000000. A classic planar image has no transparency:
Workbench draws colour 0 as pen 0 over the backdrop. FLAT-BACKDROP-SAME-AS-WINDOW deliberately gives the backdrop a pen
other than pen 0, so pen 0's box shows on any backdrop colour that is not the theme bg (orange now; still a visible
000000 box on the intended 7A7A7A muted / darker tone). The AA ramp (ip_ground) is built for the backdrop colour, not for
pen 0, so the edge pixels are also tuned for the wrong ground.

## Rig 22 PASS blind spot

flat-colour-ab.md (rig 22, depth 8) checked GetRGB32 of pen 7 and pen 44 only, never a backdrop or icon pixel; pen 7 is
not the pen the backdrop is drawn with at depth 8, so that PASS does not cover what is on screen.

## Fix direction (owner decisions, not landed)

1. Backdrop colour on depth > 3: the tile pen p >= 4 lands on 2^d-8+p. Either colour that pen (palette.prefs entry 7 /
   SetRGB32 pen 2^d-1, overriding the user's colour 4-7 slot that 8-colour MagicWB-style icons use) or draw the ROOT tile
   with a pen 0-3. Needs D: may flat take palette slot 7 (pen 255) on a deep Workbench?
2. Icon box: icon ground = pen 0. Either the backdrop takes pen 0's colour (backdrop == theme bg, dropping the separate
   darker tone), or the icon images get a transparent background (icon.library / FORM ICON mask route, not a planar image).
Rig proof wanted: a shot pixel count, backdrop pixels = intended colour and icon-box pixels = backdrop, not pen reads.

## FLAT-VIS-2 FORM ICON

Owner decision: on OS 3.2+ (icon.library v47) the icon background is transparent through a FORM ICON tail with the theme
colours baked into its palette; a theme change rewrites the icons. OS 3.1 keeps the classic planar image with its
background on pen 44 (IP_SPARE12, slot 12).

Layout source (no NDK header describes it; read from stock 3.2 files build/rig/os32/media/AmigaOS3.2CD/GlowIconsCollection,
"Video/CDXL Player.info" and "Disk.info", hexdumped):
- FORM <u32 size> ICON; FACE (6 bytes): u8 width-1, u8 height-1, u8 flags 0, u8 aspect 0, u16 MaxPaletteBytes-1.
- Two IMAG chunks (normal, selected): u8 transparent index, u8 NumColors-1, u8 flags (bit0 transparent, bit1 palette),
  u8 ImageFormat (0 raw, 1 RLE), u8 PaletteFormat, u8 depth, u16 NumImageBytes-1, u16 NumPaletteBytes-1, image bytes,
  palette RGB bytes. Chunks padded to even length.
- Verified field-by-field on the stock sample (30 colours, 90 palette bytes).

Our encoder (amarchy/iconfile_core.c li_form_build): 16 colours, depth 4, ImageFormat 0 (raw, depth bits per pixel packed
MSB first, no row padding), palette raw. Index 0 = transparent = slot 12 (the ground); slots 0..11 -> index 1..12; slots
13..15 -> index 13..15; any pen outside 32..47 -> 0. Palette comes from ipd_icon_rgb (pens_theme + ip_colours + ip_ground),
so a theme change changes the palette bytes.

Manifest design: ia_imgcrc now covers the image region plus the FORM tail, so a changed palette changes the wcrc and APPLY
rewrites the file once; unchanged second APPLY writes 0. On OS 3.1 (form_rgb NULL) the tail is dropped, so the crc equals
the old image-only value (old manifest entries stay valid). RESTORE returns the original bytes exactly (form_src).

UNVERIFIED (rig must prove): the stock RLE (ImageFormat 1) did not decode as byte PackBits and no uncompressed sample exists
on the CD, so ImageFormat 0 and the bit packing are from the format description only. Real icon.library v47 acceptance and
pixel layout are not proven on a host.

ip_fold finding, pen slot 12 (stored pen 44) on shallow screens (amarchy/iconpen_core.c ip_fold): at depth >= 6 it returns
IP_FIRST + slot. At depth 4/5 and slot >= 8 it returns the slot itself, so slot 12 is held as its own pen 12 and carries
the ip_colours value for slot 12 (the ground). At depth <= 3 every slot folds to the nearest role pen among DK_BG..DK_MUTED
(DK_ACCENT at depth 2), and the distance is computed from ip_colours drawn on desk_role(DK_BG), the panel tone. Slot 12
equals that ground, distance 0, so it folds to pen 0 = DK_BG. Consequence for the classic stand-in: on depth <= 3 the icon
background is the panel tone (DK_BG), not the darker backdrop colour; on depth 4/5 it is pen 12 whose RGB is whatever the
holder was given. Read from code, not measured on a screen.

What the rig pixel proof must check (measured screenshot pixels, never pen reads):
- Backdrop: RGB of desk_pen_colour (darker backdrop colour; flat owning the backdrop, else muted role) on desktop.
- OS 3.2, desktop and drawer window: pixels around an icon glyph equal the surrounding surface (desktop backdrop RGB on the
  desktop; the drawer window panel/backdrop RGB inside a drawer), i.e. no box.
- OS 3.1 / classic: icon box background = pen 44 colour = backdrop on the desktop; inside a drawer window it differs from the
  window panel tone, so a box may show (expected, note it).
- Depth <= 3 screen: background folds to DK_BG as above.
- A theme switch rewrites the icons (OS 3.2): palette changes, glyph background still transparent.
