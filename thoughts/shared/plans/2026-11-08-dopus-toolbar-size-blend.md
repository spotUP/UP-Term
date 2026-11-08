---
date: 2026-11-08
session: s63 (amarchy)
status: planned
---

# DOpus flat toolbar: size per screen, edges blend into the ground (s63)

Owner feedback 2026-11-08 (rig 93 eye check): the flat toolbar icons are far too big for the resolution and must adapt to
screen resolution and pixel aspect; they do not blend/antialias into the background; "we have code for that".

Research: thoughts/shared/research/2026-11-08-dopus-toolbar-size-blend.md (DOpus 5.9x source, not the 5.104 binary).

## Facts the design rests on

- Theme apply never picks brush files: it switches each lister's bank path by ARexx (amarchy/menu_theme.c:186-232). One
  fixed 24x24 set, installed by dist/addons/dopus/install.dos.
- The `dopus screen` reply at apply time gives DOPUS.1 width, height, depth (no aspect).
- DOpus remaps a toolbar brush by its CMAP with ObtainBestPen, precision IMAGE (Library/images.c remap, Program/toolbar.c:418);
  brush pen 0 is not remapped, so the ground already follows the theme (DOpus pen 0 = GR_SURFACE).
- The shipped CMAP is fixed Tokyo colours, so other themes get wrong glyph colours; levels map to pens 0,2,1,1, so no blend.
- The Workbench reserved pens 32..47 do not exist on DOPUS.1 (own screen, own colour map): pen numbers cannot be shared.

## Design (reuse, no new blend)

- Blend: brush levels map 1:1 to brush pens (ground, 33%, 66%, 100% of the glyph colour; accent the same), depth 3. At theme
  apply, write each brush file's CMAP with the theme's blended RGB from the same function the Workbench icons use
  (amarchy/iconpen_core.c ip_colours). DOpus's own ObtainBestPen then gets the blended pens. No accent loss: depth 3 has 8 pens.
- Size: generation makes size sets (16, 24, 32 px), each square and tall-pixel (gen_lucide_icons.py VARIANTS / coverage /
  fit_stems_y code), one bank file per set. Theme apply picks the set from the `dopus screen` reply and the screen aspect
  (menu_screen_aspect, li_pick_variant).

## Owner decisions (AskUserQuestion, 2026-11-08)

- Size: follow the DOpus screen font. Icon height about 2 x the DOPUS.1 screen font height (ta_YSize), snapped to the
  nearest of 16/24/32 (topaz 8 -> 16, 11-13 px font -> 24); tall-pixel variant (half height) on 2:1 pixel aspect.
- Accent kept: depth 3 brushes, 8 pens (ground, glyph 33/66/100%, accent 33/66/100%).

## Checklist

- [x] DT-1 gen_dopus_toolbar.py: depth 3 brushes, levels 1:1 to pens, size sets 16/24/32 x square/tall, one bank per set; test. DONE: test dopus_toolbar (tests/test_dopus_toolbar.py).
- [x] DT-2 brush CMAP writer in the theme apply path using ip_colours (no second blend); unit test: CMAP bytes equal ip_colours for two themes. DONE: dt_cmap/dt_cmap_patch in amarchy/dopustb_core.c, CMAP == ip_colours for two themes; test dopustb (tests/test_dopustb.c); written by dopustb_dos.c dtd_recolour on every theme apply.
- [x] DT-3 size picker: pure function (font height, aspect) -> set, reusing li_pick_variant; unit test with the owner's band rule. DONE: dt_pick_set (amarchy/dopustb_core.c) over li_pick_variant; test dopustb; screen read by dopustb_dos.c dtd_screen (LockPubScreen DOPUS.1, menu_screen_aspect).
- [x] DT-4 dopus_toolbar_flat switches to the picked bank; DOpus refresh so a patched brush is re-read (verify on rig: UNVERIFIED whether DOpus caches). DONE: dopus_theme_send(..., tb) picks the bank, re-sets listers whose brushes changed; test keymap dopus_theme_send_picks_the_set_and_recolours. RIG CHECK PENDING: unverified whether DOpus re-reads a patched brush on the re-set (DT-6).
- [x] DT-5 install.dos installs all sets; dist/README.txt. DONE: dist/addons/dopus/install.dos copies all Images/<set> and 6 banks, deletes the old single bank; uninstall-body.dos; dist/README.txt; Makefile dopus-toolbar target now Amarchy-toolbar-24. Open: tools/rig/amarchy_dopus_toolbar_rig.py still installs the old single bank (DT-6 must update it).
- [ ] DT-6 reachability: rig 3.2 + DOpus 5.104, theme apply on two screen modes (PAL hires non-laced, RTG 800x600), sentinel = bank path in DOpus lister + CMAP bytes read back from the rig disk.
- [ ] DT-7 owner eye check on a prepared rig (both modes).
