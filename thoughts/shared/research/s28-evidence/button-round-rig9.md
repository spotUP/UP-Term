---
title: s28 FLAT-BUTTON-ROUND rig 9 check (3.2) - NOT VERIFIED
status: rig-blocked
---

Code already on main: 67328573 (flat_button_corners, flat_frame_draw_round, c2_read in amarchy/flat.c). Host: flat_core 675/0,
desk 1828/0, flat_on_hang OK on origin/main 0fa468a9. Revert proof: dropping the cut-pixel write loop in flat_frame_draw_round
makes flat_core fail 2 of 675.

Rig 9, 3.2, amarchy-flat md5 476225b688bf791eded6a5620f79dc51 (built from 0fa468a9). Product route: amarchy_desk_rig put_flat_prefs(True) + pick()
(theme vantablack), then flatprobe (pen dump of GadTools windows).
- STATUS: flat=on, but `frame=hooked calls=0 flat=0`: the frameiclass hook never took the flat path, so the round code did not run.
- dri_Pens after pick: 0 1 1 2 1 3 1 0 2 1 2 1 (SHINE 2, SHADOW 1, FILL 3 vs BACKGROUND 0): flat pens still not live (same as flat-pens-not-live.md).
- Probe: buttons 80x14 read pen 0 over their whole rect, in flat on AND flat off (flat off pens 0 1 2 0 0 0 2 0 2 2 0 1): no fill (pen 4) and no
  bevel, so corners cannot be told from the background. Only the scroller knob shows pen 4.
Verdict: round corners NOT rig-verified; blocker is pens not live / frame hook flat=0 on the real route on this rig, not the round code.
