---
title: s28 flat palette stays stock (rig 11, 3.2, depth 8) - cause measured, fix not landed
status: cause-measured
date: 2026-11-01
---

# Why the Workbench palette stays stock with flat on

Rig 11 (060, OS 3.2, `rig.py aga256`, WB DEPTH 8), build from origin/main 69e24c8a. `amarchy_gui_rig.py --setup`,
no ENV:Sys/Palette.prefs at start, then `put_flat_prefs(True)` + `pick()` (tokyo-night -> vantablack), read 5 s later
with T:wbpens (extended in this commit to also print the last four colours of a deeper screen).

## Numbers

Before (no palette.prefs): RGB 0-7 = aaaaaa 000000 ffffff 6688bb 0000ff ff00ff 00ffff ffffff, RGB 252-255 = ee4444 55dd55
0044dd ee9900 (the 3.2 default "colours 4-7").

After the pick:
- ENV:Sys/Palette.prefs = T:amarchy-desk/palette.prefs (434 bytes, md5 dd183b14), entries
  0 aaaaaa, 1 000000, 2 ffffff, 3 6688bb, 4 0000ff, 5 222222, 6 00ffff, 7 ffffff.
- Screen: RGB 0-7 unchanged (pen 5 still ff00ff); RGB 252-255 = 0000ff 222222 00ffff ffffff.
- DrawInfo pens = flat map 0 1 2 0 0 0 2 0 2 2 0 1 2. AMARCHY_WBPENS 1, AMARCHY_WBREFUSED 0.

## Cause (two defects)

1. Colours 0-3: the theme never reaches palette.prefs. Desktop-theme is off, so dk_apply writes nothing and
   gt_dos_select falls back to wb_pens_apply (SetRGB32 of bg/fg/dim/accent on pens 0-3; AMARCHY_WBPENS=1 proves it ran).
   But dk_flat_pens (desk_dos.c, FLAT-PENS-LIVE) runs first inside desk_dos_apply and writes a palette.prefs whose
   colours are a snapshot of the screen BEFORE the theme (no ENV file -> desk_palette_live from GetRGB4 = stock).
   IPrefs applies that file, so the stock colours win over the live SetRGB32. "The user's colours stay" (the
   FLAT-PENS-LIVE design) is what freezes the stock palette.
2. Pens 4-7 on a screen deeper than 3: IPrefs maps palette.prefs ColorSpec entries 4-7 to the LAST four pens
   (entry i -> 2^depth - 8 + i; measured entry 5 -> pen 253 = 222222). desk_core assumes entry index == pen
   (palette_fill, palette_entry/desk_palette_set_rgb, desk_palette_live). So:
   - the backdrop colour desk_darker (entry 5) lands on pen 253; pen 5 (DK_MUTED, backdrop, inactive dim) keeps the
     graphics default ff00ff;
   - desk_palette_live copies live pens 4-7 into entries 4-7, which then overwrite pens 252-255 (stock ee4444 55dd55
     0044dd ee9900 -> 0000ff 222222 00ffff ffffff);
   - the desktop-theme path (desk_palette, 8 colours) has the same mapping: DK_SELECTION/DK_MUTED/DK_GREEN/DK_RED go to
     pens 252-255, never to pens 4-7 that the flat map, the button fill and the backdrop tile use.
   At depth 3 the two mappings coincide (2^3 - 8 + i = i), which is why depth-2/3 rigs never showed it.
   A WBPattern tile has 3 planes (pens 0-7 only), so on a deep Workbench pens 4-7 can not be coloured through
   palette.prefs at all.

## Fix direction (not landed: needs an owner decision and more than this hunt's budget)

- Decision (product): with flat on and desktop-theme off, should the pick write the theme colours (desk roles 0-3, the
  mapping the flat pen map is built on) into palette.prefs, overriding FLAT-PENS-LIVE "the user's colours stay"? Today
  the pick already recolours 0-3 live (wb_pens_apply, with a different role order bg/fg/dim/accent that does not match
  the flat map: TEXTPEN 2 would get the theme dim) and then loses it to the stock snapshot.
- desk_core: one mapping "palette entry for pen p at depth d" (p 0-3 -> p; p = 2^d-4..2^d-1 -> 4..7; else none) used by
  palette_fill / palette_entry / desk_palette_live, so the live snapshot reads pens 2^d-4..2^d-1 for entries 4-7 and
  set_rgb refuses pen 5 on a deep screen instead of hitting pen 253. Host test: set_rgb(pen 5) at depth 8 = 0 bytes,
  live snapshot entry 5 = live pen 253.
- Pens 4-7 on a deep Workbench need a live route after IPrefs settles (SetRGB32 on the ViewPort, or amarchy-flat holding
  them like the icon pens 32-47, iconpen_core), re-applied after every IPrefs reset.

Rig proof wanted after the fix: pick with flat on, GetRGB32 pens 0-5 = desk_role(theme, 0..4) and desk_darker(theme) on pen 5,
pens 252-255 unchanged from before the pick.
