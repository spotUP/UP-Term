---
title: s28 FLAT-PEN5-LAND, rig 14 (OS 3.2, aga256, depth 8), origin/main 41443536
---
Question: do the s28-flat-palette commits land, and does current main set pens 4-7 on flat ON?

Commit review: 0166e729, 16504079 (the "WIP" ordering change) and the FLATLIVE-BASELINE follow-up are already on main as aacd873b, 8b63426a, 2cfb772c
(cherry-pick of the three branch commits onto main conflicts in desk_core.c/.h, desk_dos.c, flat.c, test_desk.c because main holds the evolved versions).
6922b956 only adds the wbpens WATCH probe, already on main (diff 4/8 lines). The remote branch s28-flat-palette no longer exists on origin.
The part-3 claim "amarchy-flat pens_sync sets pens 4-7" is gone: main's pens_sync (flat.c) only holds the icon pens 32..47 (F14), it never calls desk_pen_colour.
So the PASS recorded on the branch came from a build whose flat.c set the pens; main has no such code. Nothing to land.

Run: build of origin/main 41443536 (make wasabi dist, rig-helpers), rig 14, --os 32, aga256 depth 8, palette.prefs not seeded (stock),
driver = amarchy_desk_rig put_flat_prefs + pick() (the drawn theme menu, desk_dos_apply), pens read with T:wbpens. Columns pens 0-7 | 252-255.
start          aaaaaa 000000 ffffff 6688bb 0000ff ff00ff 00ffff ffffff | ee4444 55dd55 0044dd ee9900
cycle1 ON      000000 222222 bebebe 8d8d8d 0000ff ff00ff 00ffff ffffff | ee4444 55dd55 0044dd ee9900
cycle1 OFF     aaaaaa 000000 ffffff 6688bb 0000ff ff00ff 00ffff ffffff | ee4444 55dd55 0044dd ee9900
cycle2 ON / OFF and cycle3 ON / OFF: identical to cycle 1.
Verdict: FAIL 3 of 3. Pens 0-3 follow the theme on ON and return on OFF; pens 4-7 stay stock (pen 5 ff00ff) on ON. OFF equals base.
Pens 252-255 untouched (not seeded, so this run proves nothing about the saved-palette restore of 4-7 beyond "stay stock").
Conclusion: dk_live_pens called last in desk_dos_apply (8b63426a) does not put the theme on pens 4-7 on current main. Not landed. Cause not yet found:
next measure is wbpens WATCH during one pick (who rewrites 4-7 after the apply: the Workbench screen reopen resets them to stock).
