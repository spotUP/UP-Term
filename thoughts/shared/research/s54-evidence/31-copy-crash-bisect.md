# 3.1 palette.prefs Copy crash: bisect (rig 67, AmigaOS 3.1)

Question: which program or file content makes `Copy RAM:palette.prefs ENV:Sys/palette.prefs` crash the 3.1 guest
(`IDE1 DATA write without request!? PC=0000007A`, FS-UAE exit).

Test file in every try: tests/data/desk/palette-gui-save.prefs (the owner's safe GUI Save bytes).
Script: scratchpad try.sh (reboot, wait alive, settle WAIT s, list amarchy-gui/DO_LAUNCHER, optional kill, Copy, 30 s, verdict).

| Try | What ran | Crash | Decisive line |
|-----|----------|-------|---------------|
| T1 | first Copy on fresh rig 67 boot | yes | guest ROM reset at PC f80dc8 after the two WB SetSwitch screen switches, then 140 CPU resets; watchdog log build/rig67/reset-loop-20261104-083924.fs-uae.log line 1297 |
| T2 | after watchdog restart, settle 20 s; no welcome amarchy-gui, no DO_LAUNCHER | no | rig alive, ide=0 |
| T3 | reboot, settle 2 s; welcome amarchy-gui (64K stack, CLI 11) + DO_LAUNCHER present | yes | ide=31 (the original symptom), rig did not come back |
| T4 | reboot, settle 2 s; Ctrl-C to the welcome amarchy-gui (gone before Copy) | yes | ide=31 -- killing the welcome does NOT clear the crash |
| T5 | reboot, settle 60 s; welcome + DO_LAUNCHER present, nothing killed | yes | wasabi "connection closed by the Amiga", ide=0 -- settle time does not clear it |

## Reading so far (no proven cause)

- The welcome window is not the culprit (T4).
- T2 is the only clean run: it was the boot after a watchdog restart, not a `rig.py reboot`. DO_LAUNCHER was absent there.
  DO_LAUNCHER is not in our source tree (grep amarchy/ src/ tools/ finds no hit): a Workbench/ROM process.
- Our code on the reset path: amarchy/wbreset_patch.c close_hook/open_hook (CloseWorkBench/OpenWorkBench),
  amarchy/borderblank_patch.c OpenScreen hooks (after_open calls MakeScreen + RethinkDisplay inside the caller's OpenScreen),
  amarchy/strut_patch.c OpenWindow hooks. Code reading did not show a NULL jump.
- Side finding: gui.c never calls wbr_client_drop; each exited amarchy-gui leaves a stale table entry
  (has_win 0, mode WBR_DISMISS). It is never signalled, so it cannot crash, but 8 runs fill WBR_MAX.

## Where it stands (budget reached, about 50 tool calls)

- 4 of 5 Copies crash; the only clean one (T2) is the boot after a watchdog restart.
- Ruled out: the welcome amarchy-gui (T4), boot timing / settle time (T5).
- Next candidates, one try each: (1) what the watchdog-restart boot lacks that a `rig.py reboot` boot has (DO_LAUNCHER is one visible difference);
  (2) Amarchy with the WBRESET patch off, then the borderblank patch off, then the strut patch off; (3) if all are ours-off clean, run --mmu + MuForce.
