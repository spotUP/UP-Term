---
date: 2026-11-08
session: s63
status: research
---
# DOpus 5.104 flat toolbar: size and blend

DOpus source used: /Users/spot/Code/Reference/dopus5/source (5.9x fork; 5.104 behaviour assumed equal, UNVERIFIED for 5.104 binary).

## Q1 install path and sizing logic
- Theme apply never picks brush files. It only switches each lister's bank path by ARexx: amarchy/menu_theme.c:186-232 (dopus_toolbar_flat, DOPUS_TOOLBAR_FLAT = "DOpus5:Buttons/Amarchy-toolbar", menu_theme.h:88). The brushes (DOpus5:Images/Amarchy/*) and bank are installed by dist/addons/dopus/install.dos; one fixed 24x24 set (tools/gen_dopus_toolbar.py:SIZE=24).
- It already holds the screen facts: menu_theme.c:~246 sends `dopus screen`, reply is `DOPUS.1 <w> <h> <depth> ...` (thoughts/shared/research/2026-10-29-amarchy-dopus-desktop-probe.md:27, e.g. 640 512 8). Width, height, depth are known there; the pixel aspect is not in the reply.
- Aspect code to reuse: menu_font.c:17 menu_screen_aspect(scr,&rx,&ry) (DisplayInfo Resolution, needs a struct Screen, LockPubScreen("DOPUS.1")); flat_core.c:915 flat_side_px(rx,ry) (ratio -> integer pixel multiple); flat.c:1224-1250 per-screen slot table.
- Sizing logic for icons: iconfile_core.c:416 li_pick_variant(w,h) (w >= 2h -> tall-pixel variant) and gen_lucide_icons.py:VARIANTS=((32,32),(32,16)); chosen at apply time, icons.c:498. Coverage is area-averaged to the target grid (gen_lucide_icons.coverage; fit_stems_y for non-square).

## Q2 how DOpus maps brush colours to pens
- Library/images.c:1629-1935 (RemapImage, L_RemapImage). Per distinct brush pen: key from CMAP RGB, then ObtainBestPen(screen ColorMap, r,g,b, OBP_Precision, p) (images.c:1879); fallback FindColor. Result pens are shared and refcounted per remap.
- Precision: images.c:1629 default PRECISION_IMAGE; toolbar remap struct is zero-cleared (Program/toolbar.c:418 RemapImage(func->image, GUI->screen_pointer, &toolbar->remap), ir_Flags 0) so IMAGE precision (tightest short of exact; near-exact RGB match, else allocates a new pen).
- Pen 0 is NOT remapped (images.c:1749 start = IRF_REMAP_COL0 ? 0 : 1; flag unset): brush pen 0 stays the screen's pen 0. DOpus palette pen 0 = theme GR_SURFACE (menu_theme.c:104), i.e. the ground already follows the theme. If the brush CMAP already equals the screen colours at the same indexes, no remap at all (images.c:1760-1770).
- So: CMAP + ObtainBestPen (not "first N pens as-is"). Pens 1..3 are matched by RGB to the screen. The CMAP in the shipped brushes is the fixed Tokyo set (gen_dopus_toolbar.py:CMAP), so on any other theme every glyph pen is a new allocation of the wrong colour.

## Q3 reuse of the reserved blend pens
- No by number. Pens 32..47 (iconpen_core.c:15 IP_FIRST, ip_colours :24-45) live only on the Workbench screen colour map. DOPUS.1 is its own screen with its own map (probe doc line 27); DOpus's remap never sees those indexes, and a brush byte is a brush pen index, not a screen pen.
- Smallest change that blends: write the brush CMAP with the same blend the icon path uses. ip_colours (iconpen_core.c:36-38) blend(rgb, bg, fg, 33/66/100) with bg = ground. For DOpus: bg = col[0] (GR_SURFACE), fg = col[1] (GR_TEXT) as in menu_theme.c:104-107. Then DOpus's ObtainBestPen allocates (or, on a Workbench-hosted DOpus, finds the identical reserved pens 33/37/32 when the RGBs match, which they do only if ground equals the desk backdrop) the exact blended pens. DOPUS.1 depth is 8 (probe doc), so free pens exist; a shallow custom screen would get the nearest pen (images.c:1897 FindColor).
- Brush change: levels map to pens 0,1,2,3 = ground, 33%, 66%, 100% (replace PEN_OF_LEVEL (0,2,1,1) with (0,1,2,3) using gl.level and the gl.LEVELS thresholds); depth stays 2, the accent pen goes away (or depth 3 with 4..7 reserved). Pen 0 stays ground so masking stays correct.
- Delivery: CMAP is 12 bytes at a fixed offset in the uncompressed ILBM (ilbm() in gen_dopus_toolbar.py), so theme apply can patch the 12 bytes in each of the 22 files from a new pure function (ip_colours blend of col[0], col[1], 33/66/100), run before the existing `dopus refresh`. Alternative: ship no CMAP patch and generate per theme at install: rejected (23 themes). UNVERIFIED: whether DOpus caches the opened brush (L_FlushImages, images.c:260; L_OpenImage cache) so the patched file is re-read; needs a rig check (`dopus refresh all` recreates the screen on own-screen DOpus; refresh lister alone may keep the cached image and remap, images.c:1760 early-out compares to screen).

## Q4 per-screen sizing proposal
- Sets: generate 3 pixel heights, selected by DOpus screen height band from `dopus screen` (already parsed at apply time): h < 400 -> 16 (raw 640x256/200 and laced half), 400 <= h < 800 -> 24 (today's), h >= 800 -> 32 (1344x1080 rig 94 case: owner's "way too big" complaint is the opposite scale of this; the bands must be measured against the DOpus button text height, UNVERIFIED which size the owner wants at 1080, measure on the rig before fixing numbers). Not guessed here beyond the structure.
- Aspect: brush width x height per variant like lucide.dat: square (w=h) vs tall pixel (h=w/2) when li_pick_variant(w,h) is true (640x256 hires non-laced), generated by the existing coverage + fit_stems_y path. A general rx:ry (flat_side_px, flat_core.c:915) needs DisplayInfo: LockPubScreen("DOPUS.1") then menu_screen_aspect; only needed if more than the two variants shipped.
- When: generation time makes the sets (Images/Amarchy/<size>/ or a size suffix; Buttons/Amarchy-toolbar-<size> because the bank holds the image path, gen_dopus_toolbar.py:OLD/NEW_PREFIX). Theme apply makes the choice (screen size is only known live; the user can change DOpus screen mode) by setting the lister toolbar to the right bank (extend dopus_toolbar_flat to compare against all Amarchy banks) and patching CMAP. Reuse: li_pick_variant for the tall/square choice, the gen_lucide_icons coverage/level code for rendering, menu_screen_aspect/flat_side_px for any non-2:1 ratio.
- Open: DOpus toolbar image display size is the brush size (no scaling found in Program/toolbar.c, UNVERIFIED by full read); BTNW size field (gen_dopus_toolbar BTNW offsets) not checked.
