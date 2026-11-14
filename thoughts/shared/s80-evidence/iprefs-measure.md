# s80 IPREFS theme switch: measurement and option A status

## Step 1, rig measurement: NOT DONE
The worktree had no build/amiga, so a rig needed `make amiga` first. All builds go through buildq (owner rule); the queue was 4-10 jobs
deep for the whole session (other sessions' selecta/tsc/test builds), so no rig was started. Nothing about `reset` /
desk_dos_wb_closes per apply, or a font reset on switch 2, is measured. Counters to read once a rig runs (published by lf_gt_select
after each pick): PATINA_WBCLOSES (+1 per apply that closed windows for a reset), PATINA_DESKMASK, plus the new PATINA_DEFERRED.
Existing driver: tools/rig/patina_desk_rig.py pick() (menu path, Keep requester clicked), needs rig `--os 32` per its header, DOpus via `rig.py dopus`.

## Option A: built ahead of the measurement (premise still unverified)
Code reading only (the premise risk from iprefs-theme-switch.md stands: s75 saw the requester on switch 2, which a pure pen-map
change on switch 1 would not explain; a font write is the other trigger, and option A also defers the font).
- patina/desk_core.c desk_defer_map(): pure. Foreign windows and a reset needed -> returns the pen map in force (0 stock, 1 flat) to write
  colours for, or DESK_DEFER_SKIP when the live pens match neither map. Otherwise DESK_DEFER_NONE.
- patina/desk_dos.c dk_apply: after closing our windows, counts non-backdrop windows on the Workbench screen (dk_foreign_windows);
  when deferring: no reset wait, font dropped from the mask, palette written for the live map, our windows reopened,
  desk_dos_deferred++ and sentinel PATINA_DEFERRED (the one status notice; no modal).
- tests/test_desk.c test_desk_defer_map (ONLY=desk).
Status of checks: host test and `make amiga` queued in buildq, see the report that carries this file for results.
Gap: desk_dos.c start path (line ~1790, dk_flat_pens ~907) has the same reset+close pattern and is NOT changed.
