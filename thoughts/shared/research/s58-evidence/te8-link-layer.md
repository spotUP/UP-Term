# TE-8-LINK-LAYER evidence

- Loop comparison (ip_fold vs gt_pen_budget fold_pen): identical. Same squared RGB distance, pens 0..last, last = depth <= 2 ? DK_ACCENT : DK_DARKER,
  strict < so a tie keeps the lower pen. Only the input type differed (uint8 triple vs packed 0xRRGGBB).
- Moved down to gthemes_core: DK_* role-pen enum, desk_role. New: gt_nearest_role_pen(t, rgb, depth), called by ip_fold and gt_pen_budget.
  rgb_dist, fold_pen and ip_fold's private dist + loop deleted. gthemes_core.c no longer includes desk_core.h.
- 7-target m68k build: rc=0 (amarchy-index links).
- Host: gthemes 6125 checks 0 failed; iconpen 2154 / 0; desk 2311 / 0.
- New test: test_gthemes_nearest_role_pen_ties_low_and_caps_at_depth (exact-role colour, depth-2 cap, brute-force first-minimum reference, ip_fold agreement).
