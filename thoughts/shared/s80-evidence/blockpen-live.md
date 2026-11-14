# BLOCKPEN live rig check (S81, branch s81-blockpen, a2191d04)

**Answer: UNKNOWN (branch shows no pen reset and theme picks apply; the control did not reproduce the failure, so the run does not prove the fix).**

Rig 8, `--os 32`, `rig.py aga256` (Workbench screen 640x256, depth 8, ami screens), AmigaOS 3.2. Purpose "up-term-e8: blockpen live check".
Patina branch build md5 f73e7415a92229d943c050a4de1ac6f3 (build/amiga/Patina, built after desk_core.c edit). Main build md5 bc0b605db70850c357eb9647317bcef0.
Patina started from VTCX:Apps/bin via patina_gui_rig (clean 3.2 image has no WBStartup start). Rig start needed --allow-stale-base (HEAD lacks 6 origin/main commits).

## Sentinels
Branch, boot 1 (fresh ENV:): PATINA_DESK_START_PLAN 31, PATINA_DESK_START_WRITES 5, PATINA_THEME tokyo-night.
Branch, boot 2 (rig rebooted to aga256, Patina restarted): START_PLAN 31, START_WRITES 5 (fresh ENV: always plans the palette, so not a reset verdict).
Branch, three theme picks via the menu (patina_desk_rig.pick; the Keep requester was not found by the helper, apply still counted):
- pick1 PATINA_DESKS=1 DESKMASK=15 WBCLOSES=0 DEFERRED absent, theme vantablack
- pick2 DESKS=2 DESKMASK=15 WBCLOSES=0 DEFERRED absent, theme white
- pick3 DESKS=3 DESKMASK=15 WBCLOSES=0 DEFERRED absent, theme vantablack
A second branch run (after reboot, ENV kept) repeated: DESKS 1,2,3, WBCLOSES 0, no PATINA_DEFERRED.
Control (main Patina, fresh reboot, START_PLAN 5 WRITES 2): picks DESKS 1,2,3, DESKMASK 15, WBCLOSES=0, no PATINA_DEFERRED. Same as the branch.

## Reading
- Theme pick applies on the branch (DESKS rises, DESKMASK 15, theme changes). Not checked: the pixel result on screen.
- No close/reset counted on the branch, but main also counted none, so this check does not separate fixed from unfixed. The failing loop of the earlier report was not reproduced on main here (cause unknown; main has moved since: desk_core.c differs, 34 changed lines in patina/).
- PATINA_WBCLOSES is published only after a menu pick; the start path increments it but does not publish it. There is no start-time sentinel for the reset verdict, so "boot 2/3 decide no reset" is not directly readable.
- Boot-by-boot test with a real boot 3 not done as separate reboots; reboots with Patina restart were done for boot 2, the control and the branch re-run.

## Needed to decide
Publish desk_dos_wb_closes after the start path (one am_sentinel line in desk_dos_start), or compare Workbench screen address before/after a pick (patina_desk_rig.watch_reset) on branch vs the main commit the S80 report was measured on.

## Cleaned
build/rig8 (141 MB) removed by `rig.py stop`; its saved copy build/rig-saved/8 (11 MB) deleted. Scratch in the session scratchpad (Patina.branch 411 KB, helper scripts) left. Worktree build/amiga/Patina was swapped to main's binary for the control and restored (md5 checked). No screenshots taken.
