# RL20 SELECTA-AA-RICHUI (branch s57-rl20)

Built on RL19 (roundaa_core: raa_radius, raa_cover, raa_nearest) and ce_mix. New pure core amarchy/richaa_core.c/.h:
rich_corners(depth, box, fg, bg, put) blends the four corners of a filled box; rich_aa_calls is the sentinel.
Glue in amarchy/gui.c: fill_round() fills the box, then rich_corners with rich_put (true colour: mt_fill one pixel with the
mixed colour; pen screen: raa_nearest over the palette). Used for: the selected menu row (accent over MT_BG), the welcome
window Close / Open Help buttons (border in MT_TEXT over MT_BG, inner fill over the border). Depth under 4: square stock box.
Text and font sizes already go through aa_ink (menu_text.c, RL21 pattern); no change needed there.

## Tests
- `make test-one SUITE=richaa`: 13 checks, 0 failed. Reachability: rich_aa_calls rises by exactly 1 per call; untouched middle,
  corner pixel 0 back to bg, intermediate shades, mirrored corners; depth 2/3, tiny box, null put all write nothing.
- Revert check (ce_mix replaced by plain fg): 1 check fails (bg >= 4 && mid >= 4).
- `make amarchy-gui` and `make amarchy-flat` link clean (vbcc, warnings as errors). flat-budget unaffected (flat files untouched).

## Open
- No start logo exists in amarchy/ (grep logo: none): nothing to blend; needs an owner decision if a logo is wanted.
- Pixel proof on a rig not run: tools/rig/amarchy_richaa_rig.py is a stub (exit 2).
- Menu window and border outer corners are the window rectangle (cannot be rounded without a shaped window); not done.
