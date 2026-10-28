---
date: 2026-10-28
session: vtcon-32 (Amarchy s19)
topic: AMTILE-LAYOUT implementation, rig verification open
status: in progress (41 of 43)
---

# Handoff: AmTile layouts (Amarchy s19)

## Where things stand

Plan: `thoughts/shared/plans/2026-10-28-amtile-layout.md` (43 items, owner decisions L1-L7, deviations D-1..D-7).
Ledger rows: `thoughts/spot/todos.md`, grep `AMTILE-LAYOUT`.

Done and pushed (41 of 43):
- P1 tile_core layouts, promote, cycle (b0eb07c1)
- P2 amconf keys tile-layout / tile-ratio / tile-nmaster (55d431d3)
- P3 AmTile commands LAYOUT, RATIO, NMASTER, PROMOTE, CYCLE, STATUS fields, persistence (82d18965)
- P4 dist rows: 15 hotkeys, 8 menu rows, icons, keymap tests (2b4c4078); kit shadow test pin fixed (0d2183af)
- P5 rig code in tools/rig/amarchy_tile_rig.py (6f86d7a0)

Open:
- P6-1 rig 7 full run green. Run 1 (1202a1f0, evidence
  `thoughts/shared/research/2026-10-28_amtile-layout-rig7.md`): full run 17/29. Isolated `--only` reruns
  pass layout_hotkey, all 7 lay_*, ratio, nmaster with exact helper boxes.
  - (a) the rig script leaks windows between steps (stale boxes from layout_hotkey on);
  - (b) cycle and monocle_raise unproven: `topmost()` uses wasabi `covered()` over the read_ui window list,
    which does not reflect layer order;
  - (c) promote fails only when run isolated.
  A sonnet agent (started 2026-10-28, rig 7, purpose "Amarchy s20: AmTile rig script fix") is fixing (a) and
  (b) in the rig script only and appending "## Run 2" to the evidence doc. If no correct z-order probe exists
  without a wasabid change, it stops on (b) and writes what is needed. Read its Run 2 section first.
- P6-2 evidence doc complete and rig 7 stopped.

## Next steps

1. Read `## Run 2` in the evidence doc. Rig 7 must be stopped; check `ps` for build/rig7.
2. If cycle / monocle_raise fail under a correct probe: AmTile bug (CYCLE raises via win_raise under
   LockIBase, ActivateWindow after unlock; tile.c). Root-cause fix plus a test, then rerun those steps.
3. If (b) needs wasabid support for layer order: ask the owner (wasabi is the spotUP fork, pin
   tools/nas/wasabi.rev).
4. When 43 of 43: mark the plan done in the ledger, then ask the owner for the manual test below.

## Manual owner test (not done)

On rig 7 or the A1200: super+alt+1..7 pick layouts, super+alt+l cycles, super+alt+minus/plus ratio,
super+alt+j/k master count, super+alt+return promote, super+alt+period/comma cycle windows (monocle brings
the target to the front); restart Amarchy and check layout, ratio and master count come back.

## Other things found

- `make dist-check` fails test_the_drawer_holds_licence_and_source_note_but_no_binary: 03894058 (I657) moved
  tools/nas/wasabi.rev to 562672a8 but dist/wasabi/SOURCE.txt still names be4c0559. Not AmTile. Asked
  session vtcon-f6 to update it; no reply seen yet.
- D-2: win_fit lets MinWidth beat the room left on screen, so a window can extend past the screen edge.
  Existing behaviour, owner call, in the ledger.
- tools/rig/amarchy_window_rig.py passes 17/17 on `--os 32` (ca4e50e2); b2_* steps need a WORKBENCH port.

## Do not commit (other sessions' files in the tree)

thoughts/shared/handoffs/2026-10-27_amarchy-session10-main-merge-geom.md, tools/rig/installer_rig.py, `-`,
scratchpad_info, scratchpad_w.log, the two vtcon-05 handoffs. Rig 9 belongs to another session.
