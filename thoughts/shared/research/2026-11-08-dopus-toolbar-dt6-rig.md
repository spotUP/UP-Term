---
date: 2026-11-08
topic: DOpus toolbar DT-6 rig reachability
status: blocked
---

# DT-6 rig run, PAL hires non-laced (rig 63, KS 3.2, DOpus 5.104)

Script: tools/rig/amarchy_dopus_toolbar_rig.py (installs all 6 sets + banks, 2 theme applies via d.pick).
Host expectation: tools/rig/dopustb_expect.c (dt_pick_set + dt_cmap from the product sources).
Example: `dopustb_expect dist/amarchy-themes/ethereal.theme 8 22 44` -> set 16t.

## Result, theme 1 (vantablack), run 2
- (a) FAIL: lister B toolbar = DOpus5:Buttons/Amarchy-toolbar-24, expected 16t. Lister A kept DOpus5:Buttons/Custom-bank (OK).
- (b) FAIL: CMAP of 16t/Copy.small = 1a1b26494d60787e9aa9b1d63a486b5974b07aa2f71a1b26, not dt_cmap(vantablack).
- (c) caching verdict: NOT answered (no recoloured brush reached the disk for the expected set).
- RTG 800x600: NOT run.

## Likely cause (unverified)
dtd_screen locks public screen "DOPUS.1". On this rig DOpus runs on the Workbench screen
(`dopus screen` -> "Workbench 640 256 4 ..."), so there is no DOPUS.1: font_h=0, no aspect,
dt_pick_set falls back to set 24 square. The recolour then also targets the wrong set.
Rig tooling already has `rig.py dopus --own-screen` (tools/rig/dopus_rig.py OWN_SCREEN, 1344x1080) which
gives a DOPUS.1 screen. Next run: use --own-screen, or decide whether the product must fall back
to the Workbench screen when DOPUS.1 is absent (product decision for the owner; plan assumes DOPUS.1).

## Open problems
- d.pick timeouts: run 1 "Down to Style: no stable state within 30s"; run 2 second pick "open menu: no stable state within 60s". Not reproduced by hand; cause unknown. The owner's "enter Amarchy theme" timeout did not recur.
- dist/addons/dopus/install.dos HEAD contains its whole body twice (grep "Lab dopus" = 2); the second copy is the old single-bank installer.
- Unused leftover strip_colours and inverted font check in the rig script (cosmetic, ends at font 8).

## Rerun after cc4641a7 (rig 63, KS 3.2, PAL hires 640x256 non-laced, DOpus on the Workbench screen)
Fixes: cc4641a7 (screen from the `dopus screen` reply, install.dos once, rig nits) and the next commit (rig script reads ENV:Sys/font.prefs: topaz 8 on this rig).
Binary: build/amiga/Amarchy md5 a8275fabe011fd6d5ce957ccb696a74f, built from cc4641a7 sources, copied to build/rig63/vtc.
- Theme 1 ("white"): (a) PASS lister B toolbar = DOpus5:Buttons/Amarchy-toolbar-16t (expected 16t); lister A kept Custom-bank. (b) PASS CMAP of 16t/Copy.small
  = ffffffababab575757000000cfcfcf9f9f9f6e6e6effffff == dt_cmap(white).
- Theme 2: NOT RUN. d.pick timed out again: "open menu: no stable state within 60s (last None)" (same as earlier run 2; the menu driver, not the toolbar).
- (c) caching verdict (recoloured brush shown after re-set without restart): NOT ANSWERED, needs the second apply.
- RTG 800x600: NOT RUN.
DT-6 stays OPEN: PAL theme 1 sentinels pass, theme 2 / caching / RTG outstanding.
