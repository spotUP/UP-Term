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

State at 4f93e6a1: chords fire on rig 4 and the A1200; the viewer and menu open and stay up (TERM default
in amarchy-menu, 256 KB stack); a junk keymap is refused (63ebcad0); rig scripts start Amarchy from
VTC:Apps/bin (86ba6799). Ledger entries after "2026-10-22 dist chain" hold the detail.

1. Heap: 43288 B resident against 32768 on rig 4. Rework planned (ledger "heap breakdown").
2. Stack regression test for amarchy-menu; screen_text reader for amarchy_menu_rig.py (2 of 6, script defect).
3. Scripts A6, B6, D1, E1; F1 fresh-kit install; tick the plan checklist (near line 313).
4. Owner decision still open: no dist/packages.txt row for Amarchy. Recommended: leave it out.
5. The A1200 rebooted once on a manual `amarchy-menu viewer` run from a 4 KB shell stack; not seen since the
   256 KB stack. Unproven.

## RELEASE-1.0 side (main)

- Browse (13292c0a): upkg stays alive after Return at the OK box. B5 fails with empty output,
  undiagnosed; both colour checks read 0 -> 0; patch 0026 revert proof not run.
- `rig.py --nofpu` failed to boot three times ("no wasabid answer"), possibly host load.
  Retry when the host is quiet, then run the five no-FPU checks.
- Then: re-pin upterm, dist and dist-check, owner retest. The release upload asks first.

## Rig findings

- thoughts/shared/research/2026-10-22_amarchy-rig-checks.md: Amarchy checks 1-8 on rig 4.
- thoughts/shared/research/2026-10-22_browse-nofpu-rig4.md: browse and --nofpu run on rig 4.
