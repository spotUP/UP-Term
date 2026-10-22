---
date: 2026-10-22
topic: Amarchy Phase F, dist chain, rig checks; RELEASE-1.0 browse and nofpu state
branch: amarchy-a (worktree .claude/worktrees/agent-a7edb2573dbabf68d), main
---

# Handoff: Amarchy Phase F and rig checks

## State

- amarchy-a: Phase F docs and release wiring are in (c40350b9), Uninstall stops a running
  Amarchy (0f4dae6c), CxFilter fix (8adbb48e: plain `rawkey lcommand` filter + SetFilterIX,
  rows=9 on the rig), dist chain fix (2561433d: `make dist` builds the ipk feed and the
  index), sibling paths in tests through tools/rig/paths.py (9650fcd6).
- Amiga builds in the worktree need `UPTERM_ROOT=/Users/spot/Code/UP-Term`.
- The ledger (thoughts/spot/todos.md, Amarchy entries near lines 1123-1135) holds the
  per-commit detail and the open list. Trust it over this file.

## Open, in order

1. Owner A1200 test of the hotkeys (steps in the ledger). Decides whether the hotkey bug is in
   the rig's wasabi Left Amiga key injection or in the Amarchy filter. On the rig, STATUS and
   RELOAD pass, but no chord fires (amarchy_rig.py 5 of 7, keymap_rig B5 1 of 12).
2. Exact heap measure on the rig: estimate is 32.0-32.1 KB of the 32768 budget.
3. Rig checks not yet run: 4 (bad edit), 6 (amarchy_menu_rig.py), 7 (window close frees the
   keymap), RUN <n>; scripts still to write: A6, B6, D1 timing, E1 clash.
4. rig.py install does not install Amarchy or amarchy-menu. dist/keymap header should say that
   user rows need a `[bind]` header.
5. F1 fresh-kit install; tick the plan checklist
   (thoughts/shared/plans/2026-10-22-amarchy-commodity.md, near line 313).
6. Owner decision still open: no dist/packages.txt row for Amarchy (base kit). Recommended: leave
   it out.

## RELEASE-1.0 side (main)

- Browse (13292c0a): upkg stays alive after Return at the OK box. B5 fails with empty output,
  undiagnosed; both colour checks read 0 -> 0; patch 0026 revert proof not run.
- `rig.py --nofpu` failed to boot three times ("no wasabid answer"), possibly host load.
  Retry when the host is quiet, then run the five no-FPU checks.
- Then: re-pin upterm, dist and dist-check, owner retest. The release upload asks first.

## Rig findings

- thoughts/shared/research/2026-10-22_amarchy-rig-checks.md: Amarchy checks 1-8 on rig 4.
- thoughts/shared/research/2026-10-22_browse-nofpu-rig4.md: browse and --nofpu run on rig 4.
