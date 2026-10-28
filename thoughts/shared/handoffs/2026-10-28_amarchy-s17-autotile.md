---
date: 2026-10-28
session: Amarchy s17 (peers know it as vtcon-32)
topic: AMARCHY-AUTOTILE - phases 1 and 2 merged, phases 3 and 4 next
status: in progress
---

# Handoff: AmTile autotiling

Plan: thoughts/shared/plans/2026-10-28-amarchy-autotile.md (owner decisions D1-D8, checklist, "Phase 2 as built").
Research: thoughts/shared/research/2026-10-28_amarchy-autotile.md.
Ledger: thoughts/spot/todos.md, row "Amarchy s17".

## Done (14 of 21): P1-1..P1-5, P2-1..P2-9
- main a3d24cb4 pushed. tile_core + tests/test_tile.c (72 checks), rexx_host.c (Amarchy, leaf, AmTile share it;
  rexx_send restores RexxSysBase), amarchy/tile.c (AmTile: broker, AMTILE port, 250 ms poll, state file
  ENV:/ENVARC:Amarchy/tile), win_apply.c shared with win.c, Makefile target AmTile, dist-base, dist-check stack
  line, parts.txt, install.dos (UP-Term:bin/AmTile), rig.py stage_upterm.
- Checks run: cross build of AmTile/Amarchy/amarchy-menu/amarchy-gui rc=0; make test ONLY=tile and ONLY=win pass;
  test_amarchy_stack.py on AmTile passes. No rig run yet.

## Open, in order
1. P3-1 menu row "Tile windows" under the window section, runs `UP-Term:bin/AmTile TOGGLE`.
2. P3-2 hotkey `lamiga t` if free (grep the binding table, record the result in the plan).
3. P3-3 config key tile_gap (amconf_core, default 4); AmTile reads it at start and on RETILE (g_gap in tile.c).
4. P3-4 Amarchy at boot runs `AmTile ON` when ENV:Amarchy/tile is "on".
5. P4-1 tools/rig/amarchy_tile_rig.py (template amarchy_window_rig.py), P4-2 rig 7 run, purpose
   "Amarchy s17: autotile reachability", steps 1-7 of the plan plus: second `AmTile ON` forwards and returns rc,
   Exchange Disable/Enable, ENV file content, close-during-apply no crash, amarchy_window_rig.py regression
   (win_apply + rexx_host refactors). Stop rig 7 at the end.

## Gotchas
- AmTile finds only the screen named "UP-Term" in v1.
- Rig 7 was stopped for heat (owner approved). Rigs 2/3 musicplayer-55, 4/8 Selecta session, 9 unknown owner.
- Main checkout has another session's modified handoff (2026-10-27_amarchy-session10-main-merge-geom.md) and
  untracked vtcon-05 handoffs: not ours, never commit them.
