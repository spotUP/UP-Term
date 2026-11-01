---
date: 2026-11-01
topic: S28 pens not live after an Amarchy theme pick with flat on
status: fixed (rig helper)
---
# Pens not live after theme pick (flat on): rig helper read too early

## Verdict
Cause (a): the rig helper read the Workbench pens before the product finished the apply. Amarchy on main
(887a0459) sets the flat pens; `tools/rig/amarchy_desk_rig.py pick()` returned about 5 s too early.

## Path
gt_dos_select (amarchy/gthemes_dos.c) -> desk_dos_apply (desk_dos.c) -> dk_apply (desktop-theme mask; no-op when
desktop theme is off) -> dk_flat_pens (SYS:Prefs/Palette FROM T:amarchy-desk/palette.prefs USE with the flat map,
dk_wait_reset up to 6 s, dk_wait_iprefs) -> dk_wbborder -> dk_flat_sync. lf_gt_select (amarchy/leaf.c) then writes
AMARCHY_WBPENS / WBREFUSED / DESKS / DESKMASK / WBCLOSES.

## Rig 11 (3.2, --max) evidence
- Baseline: dri_Pens `0 1 1 2 1 3 1 0 2 1 2 1` (stock), flat STATUS `frame=hooked calls=0 flat=0`.
- Flat on + pick, read 3 s after pick(): pens still stock; ENV:Sys/Palette.prefs already written (same md5 as
  T:amarchy-desk/palette.prefs). At +15 s: pens `0 1 2 0 0 0 2 0 2 2 0 1` (flat map), no requesters, no editors.
- Timing, two runs each way: pick() returned 21-22 s after start; AMARCHY_WBCLOSES written 4.9-5.2 s after
  pick() returned; pens flipped 5.5-5.8 s after. pick() waited only for the recoloured list plus sleep(3).
- The rig-9 readings (stock pens, calls=0) and its "flat off" pens (`0 1 2 0 0 0 2 0 2 2 0 1`) were lagged
  readings of the previous pick.
- Colours AAAAAA/000000/FFFFFF/6688BB stay stock with desktop theme off: expected, flat only changes pens.

## Fix
pick() deletes ENV:AMARCHY_WBCLOSES before the pick and waits (60 s bound) until lf_gt_select writes it again.
Test: tests/test_amarchy_desk_rig_pick_waits.py (`make test ONLY=desk_rig_pick_waits`); fails on the old
sleep(3) code ("returned before the apply finished (0 of 6 polls)").

## After the fix (rig 11)
- flat off pick: pens right after return `0 1 2 2 1 3 0 0 2 2 0 1` (was flat map before).
- flat on pick: pens right after return `0 1 2 0 0 0 2 0 2 2 0 1`; amarchy-flat STATUS `frame=hooked calls=352 flat=72`.

## Open (not in scope)
- Flat off restores the desk_pens map `0 1 2 2 1 3 0 0 2 2 0 1`, not the stock pens (desk_pens_sync off path).
- Workbench on the 3.2 rig is depth 2 (4 colours): desk_backdrop_pen (pen 5, DK_MUTED) does not exist on screen.
