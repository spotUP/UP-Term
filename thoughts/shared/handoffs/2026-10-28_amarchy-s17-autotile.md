---
date: 2026-10-28
topic: AMARCHY-AUTOTILE - phases 1 and 2 merged, phases 3 and 4 next
tags: [amarchy, amtile, autotile, handoff]
status: complete
session: Amarchy s17 (peers know it as vtcon-32)
---

# Handoff: AmTile autotiling

## Task(s)
- AMARCHY-AUTOTILE: AmTile, a standalone tiling commodity driven by Amarchy (owner decisions D1-D8 in the plan).
- 14 of 21 checklist items done: P1-1..P1-5, P2-1..P2-9. Open: P3-1..P3-4, P4-1, P4-2.

## Critical References
- Plan: thoughts/shared/plans/2026-10-28-amarchy-autotile.md (decisions, checklist, "Phase 2 as built").
- Research: thoughts/shared/research/2026-10-28_amarchy-autotile.md.
- Ledger: thoughts/spot/todos.md, row "Amarchy s17" (grep the id).
- Code: amarchy/tile.c (AmTile), amarchy/tile_core.c/.h (pure layout), amarchy/win_apply.c/.h,
  amarchy/rexx_host.c/.h, tests/test_tile.c.
- Rig template for P4-1: tools/rig/amarchy_window_rig.py.

## Recent Changes
- 5d184c6c tile_core + tests/test_tile.c (72 checks).
- 5f7e5433 rexx_host.c extracted; Amarchy, leaf and AmTile share it. 31f7f594 rexx_send restores RexxSysBase.
- c2c818de AmTile: broker, AMTILE ARexx port, 250 ms timer poll, state file ENV:/ENVARC:Amarchy/tile;
  win.c box_of/apply_box moved to win_apply.c as win_box_of/win_apply_box; Makefile target AmTile,
  dist-base, dist-check stack line, tests/test_amarchy_stack.py (AmTile 16384), dist/parts.txt,
  dist/install.dos (UP-Term:bin/AmTile), tools/rig/rig.py stage_upterm.
- a3d24cb4 review fixes: CheckIO before WaitIO on the timer (stale AbortIO signal blocked the loop up to
  250 ms); broker ActivateCxObj(g_on) so Exchange shows Active only while tiling.
- Checks run: cross build of AmTile, Amarchy, amarchy-menu, amarchy-gui rc=0; make test ONLY=tile and
  ONLY=win pass (72 and 68 checks); test_amarchy_stack.py on AmTile passes. No rig run yet.

## Learnings
- AmTile v1 finds only the screen named "UP-Term" (not "UP-Term.2".."UP-Term.9" or a PUBSCREEN override).
- ChangeWindowBox runs under LockIBase after screen and window membership checks, as win.c did.
- Exchange: Disable = OFF, Enable = ON, Kill/Unique = QUIT (state file unchanged).
- STATUS returns "on|off <tiled> <retiles>"; a bad argument returns rc 10. The retile count is the rig sentinel.
- Gap is TILE_GAP_DEFAULT until P3-3 wires tile_gap into g_gap.

## Artifacts
- Plan and research above. Main pushed at d495d2b4 (this handoff's first version); no PR (direct push).

## Next Steps
1. P3-1 menu row "Tile windows" under the window section, runs `UP-Term:bin/AmTile TOGGLE`.
2. P3-2 hotkey `lamiga t` if free (grep the binding table, record the result in the plan).
3. P3-3 config key tile_gap (amconf_core, default 4); AmTile reads it at start and on RETILE (g_gap in tile.c).
4. P3-4 Amarchy at boot runs `AmTile ON` when ENV:Amarchy/tile is "on".
5. P4-1 tools/rig/amarchy_tile_rig.py.
6. P4-2 rig 7 run, purpose "Amarchy s17: autotile reachability": plan steps 1-7, plus a second `AmTile ON`
   forwards and returns rc, Exchange Disable/Enable, ENV file content, a window closing mid-apply causes no
   crash, amarchy_window_rig.py regression (win_apply and rexx_host refactors). Stop rig 7 at the end.

## Other Notes
- Rig 7 is stopped (owner approved, heat). Rigs 2/3 musicplayer-55, 4/8 Selecta session, 9 unknown owner.
- The main checkout has another session's modified handoff (2026-10-27_amarchy-session10-main-merge-geom.md)
  and untracked vtcon-05 handoffs and stray files: not ours, never commit them.

## Update: Amarchy s18 (2026-10-28) - 21 of 21 done
- P3 merged 53ec8d71: menu row window.tile, hotkey super+alt+t (super+t is tab:new), hidden amconf key tile-gap
  (default 4, 0..64; GAP n wins until the next RETILE), Amarchy runs AmTile ON at boot when ENV:Amarchy/tile is on.
- P4-1 merged 93c93d22: tools/rig/amarchy_tile_rig.py + host box helper tools/rig/amarchy_tile_boxes.c (same tile_core code).
- Rig run 1 failed: AmTile looked only for the "UP-Term" screen. Owner revised D4: tile the FRONT screen (DOpus listers
  included, owner OK). Fix merged 50d4040a (lock_front_screen, tile_screen_switch drops the order on screen change).
- RUN <n> on AMARCHY counts installed rows only (terminal tab:/menuid:/slash: rows dropped); Tile windows row = 15.
- Rig run 2 (c48af51e): 11 of 11 steps, Exchange and hotkey pass. Evidence: thoughts/shared/research/2026-10-28_amarchy-tile-rig7.md.
- Open, not AmTile: amarchy_window_rig.py 11 of 17 on a DOpus rig. b2_* need a WORKBENCH port (DOpus has none);
  menu_move_left looks for MENU_TITLE "Amarchy" (amarchy_window_rig.py:29) but wasabi lists the borderless menu window
  with an empty title; move_left KeyError box0 is probably its knock-on. Both failed in run 1 too.
- Owner manual check: tiling looked right on rig 7.
