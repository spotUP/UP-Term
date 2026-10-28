---
date: 2026-10-28
author: spot (Amarchy s20)
status: done (07f71354, rig 7 PASS: thoughts/shared/research/2026-10-28-uhc-extbin-live-path-rig7.md)
ledger: thoughts/spot/todos.md row UHC-EXTBIN-LIVE-PATH
---

# UHC:ExtBin on the live command path

## Problem

`up-uhc` install makes `UHC:ExtBin` and runs `Path UHC:ExtBin ADD`, but that only
changes the installer's own Shell. Every Shell started afterwards from Workbench
(UP-Term windows: `shell/vsh.c` `wb_path()` copies the Workbench process's
`cli_CommandDir`) and every Amarchy launch (`amarchy/proclaunch.c`
`pl_dup_path()` copies the commodity's own boot path) lacks `UHC:ExtBin` until a
reboot. So `Pipe` and other UHC package programs are "unknown command" right
after install.

## Design (owner decisions, s20)

1. New kit tool `WBPath ADD|REMOVE <drawer>` edits the Workbench process's
   `cli_CommandDir` live (nodes are AllocVec'd `{next, lock}` BPTR pairs, see
   `struct wb_path` in `amarchy/wblaunch.c`; walk and edit under `Forbid()`).
   - ADD: Lock the drawer; if a node already holds a lock on the same object
     (`SameLock() == LOCK_SAME`), UnLock and do nothing; else append at the end.
   - REMOVE: unlink and free every node whose lock is `LOCK_SAME` as the drawer.
   - No Workbench process: print a `[WARN]` line, rc 5 (install still succeeds).
   - `__stack` + `$STACK` cookie like every Amiga program (memory rule).
2. `dist/addons/uhc/install.dos`: after `Path UHC:ExtBin ADD` (line ~201) also run
   `WBPath ADD UHC:ExtBin`, and echo the hint: already-open Shells keep their own
   path; run `Path UHC:ExtBin ADD` there once.
3. `dist/addons/uhc/uninstall-body.dos`: `WBPath REMOVE UHC:ExtBin` BEFORE anything
   under UHC: is deleted (the Workbench node holds a lock that blocks the delete).
4. Single source of truth for the path copy: one shared helper (config/ header,
   same pattern as `config/segstack.h`) that copies a `cli_CommandDir` chain.
   Used by `shell/vsh.c wb_path()`, `amarchy/proclaunch.c pl_dup_path()` and the
   free side (`pl_free_path`). WBPath reuses the same node layout.
5. Amarchy: `pl_dup_path()` copies the WORKBENCH process's path (via the shared
   helper), falling back to its own CLI path when there is no Workbench process,
   then appends `PL_BIN_DIR` last as now.
6. No vsh fallback lookup in ExtBin (it would bypass the user's own
   `Path REMOVE` and change lookup on every miss). Already-open shells: hint only.

## Checklist

- [x] P1 shared path-copy helper; vsh `wb_path()` and `pl_dup_path()` use it (no second copy loop left: grep `cli_CommandDir` after)
- [x] P2 `pl_dup_path()` copies the Workbench path, own CLI path as fallback, PL_BIN_DIR last
- [x] P3 `WBPath` tool: ADD (dedupe by SameLock), REMOVE (all matches), no-Workbench warn rc 5, stack cookie
- [x] P4 Makefile builds WBPath; kit ships it (wherever the other kit C: tools go); rig install stages it
- [x] P5 install.dos calls `WBPath ADD UHC:ExtBin` + open-shell hint
- [x] P6 uninstall-body.dos calls `WBPath REMOVE UHC:ExtBin` before deletes
- [x] P7 host tests: string/structure tests for install/uninstall order (existing dist/addons tests pattern), Makefile/kit listing
- [x] P8 reachability on rig 7 (purpose "Amarchy s20: UHC-EXTBIN-LIVE-PATH check"): fresh boot, install up-uhc, then WITHOUT reboot (a) new UP-Term window: `taskpath` shows UHC:ExtBin and `Pipe` (or another ExtBin program) runs by name; (b) an Amarchy launch runs the same program by name; (c) uninstall removes the node (taskpath on a new window lacks it) and UHC: deletes without "object in use"
- [x] P9 regression: each host test fails with its change reverted
- [x] P10 ledger row ticked with commits; handoff line

## Upstream note (optional, not ours)

vtcon-05 patch 0046 wrapper could call `Path UHC:C UHC:S UHC:ExtBin ADD`;
suggest only.
