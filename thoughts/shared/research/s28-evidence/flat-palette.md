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

## 2026-11-01: who writes pens 4-7 after desk_dos_apply (rig 11, OS 3.2, AGA 256, branch build)

Probe: `tests/amiga/wbpens.c WATCH <seconds>` polls the Workbench screen every tick and prints a line when the
screen address, pens 4-7, `T:amarchy-revert/flatlive.rgb` (F) or `ENV:AMARCHY_WBCLOSES` (D) changes.
Pick = Style > Amarchy theme, tokyo-night (current) -> vantablack, driven by amarchy_desk_rig.py pick().

Baseline: 4-7 = 0000ff ff00ff 00ffff ffffff, 252-255 = ee4444 55dd55 0044dd ee9900.

Flat ON timeline:
- T0 stock, screen 0x100f0e00.
- T31 4-7 = 292e42 0e0e14 9ece6a f7768e: the Amarchy menu opens; its MT_ROLES pens come from vr_pen_obtain
  (ObtainBestPen), and on a 3.2 Workbench pens 4-7 are free, so the menu's own shared pens land there and recolour them.
- T279 vantablack preview (gt_pv saves pens 0-7, sets theme 0-7).
- T301 4-7 back to the menu colours: gt_dos_select's gt_dos_desk_restore, then theme_live re-obtains the menu pens.
- T729 4-7 = theme (1a1a1a 222222 b6b6b6 a4a4a4), F 1, D 1: dk_live_pens in desk_dos_apply.
- T732 NEW Workbench screen 0x10052f10 (3 ticks after apply returned); 4-7 stay theme there because amarchy-flat
  pens_sync (IP_SYNC_OBTAIN, commit 16504079) sets them on the new screen.
- Final: 0-7 = 000000 222222 bebebe 8d8d8d 1a1a1a 222222 b6b6b6 a4a4a4; 252-255 unchanged. PASS (pen 5 = 222222).

Flat OFF timeline (same pick):
- T814 F 0: dk_live_pens(0) restores flatlive.rgb, which holds the MENU colours 292e42 0e0e14 9ece6a f7768e, not the
  user palette: the capture at apply time read the menu's ObtainBestPen pens.
- T819 4-7 = stock: the GUI close (cleanup -> gt_preview_term -> gt_dos_preview_send -> gt_dos_desk_restore) writes
  gt_pv's saved pens back.
- Final 4-7 = 0000ff ff00ff 00ffff ffffff = baseline. PASS, but only by ordering.

Conclusion:
- The magenta pen 5 after a pick does not reproduce with this branch (0166e729 + 16504079): the post-apply writer was
  the Workbench reopen (fresh screen, stock pens 4-7); amarchy-flat pens_sync on the new screen covers it.
- Open defect (not fixed here): dk_live_pens' baseline capture (desk_dos.c) and the preview save (desk_preview) read
  pens 4-7 while the Amarchy menu holds them as shared ObtainBestPen pens. flatlive.rgb then stores menu colours, and
  flat OFF only lands on the user palette because the menu-close preview restore runs after it. A pick from ARexx
  (KA_GTHEME, no menu) or any other holder of the free pens 4-7 would restore the wrong colours. Root-fix direction:
  the baseline must come from the user's saved palette (the Palette prefs / pre-theme capture before any menu pen is
  obtained), not from the live ViewPort.
