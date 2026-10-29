---
date: 2026-10-29
session: vtcon-32 Amarchy s22
status: handoff
---

# Amarchy session 22: DOpus theme colours, Workbench pens, AmTile stuck window

## Done (pushed)
- ad410dcd D14: `dopus_theme_commands` (menu_theme.c), owner pen table.
- 8c7dab2a: bar re-raises itself over foreign windows; shared `amarchy/poll_timer.c` (AmTile + Amarchy). Rig 3 check: AmTile poll path still tiles new windows (third window tiled in 0.1 s).
- cc3053e4: theme switch sends DOpus commands from `gt_dos_select`.
- aadea0a4 + 444e127e: theme switch sets Workbench pens 0-3 (bg/fg/dim/accent) behind a WCAG 4.5 contrast guard (`wb_contrast_x100`, refuses 0 of 23 themes); DOpus on Workbench gets four one-key `set pens` lines + `refresh lister`, `set palette` only when DOpus has its own screen. Sentinels AMARCHY_WBPENS / AMARCHY_WBREFUSED. Rig 9 (DOpus 5.104): 3 of 3 selects, pens read back.

## Why `set palette` did nothing
DOpus5 source (github BlitterStudio/dopus5): palette applied only by load_screen_palette (palette_routines.c:58-107) on its own screen (display.c:198). Read-back verbs: `dopus query palette`, `dopus query pens <key>`. Unknown commands reply rc 0.

## Open, in order
1. AMTILE-RIG-LAYOUT-STATE (ledger, owner chose a root-cause probe in a fresh session): after super+alt+l the active UP-Term window gets the new width but keeps old x; a click applies the move. AmTile calls ChangeWindowBox under LockIBase (win_apply.c:26, tile.c tick()). Test: ChangeWindowBox outside LockIBase; look for a second resize racing it (UP-Term NEWSIZE handling, XCON). The input-event nudge idea is a workaround, rejected. Evidence: scratchpad amtile-dying-window.md, amtile-layout-fix.md.
2. Owner look check: rig 9, Style > Theme, DOpus lister + Workbench title bar readable after switching.
3. Rig kit stages no Amarchy themes (agents copy by hand).
4. VTC: unassigned on DOpus-as-Workbench rigs (1f27f75d, vtcon-05); ownscreen_rig.py uses VTC:dripens.
5. Pre-existing: AmTile missing from `amiga:` target (test_make_amiga_builds_every_program_dist_base_copies_into_the_kit).
6. Rest of roadmap R1 (palette.prefs, auto-revert, backdrop, fonts) stays planned.
