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

## Round 2 (rig 67, AmigaOS 3.1, main 528fa672)

Switches (guest only, gone with the rig clone): S:User-Startup got `If NOT EXISTS BOOTX:no-amarchy` around the
Amarchy and amarchy-index Run lines; SolidWin off through a `; C:SolidWin` comment line in S:User-Startup (BOOTX:solidwin-up
skips when S:User-Startup spells `C:SolidWin`; `rig.py reboot` rewrites solidwin-up, so a flag inside it does not hold);
DOpus5 off by C/LoadWB -> loadwb_old and Libs/workbench.library renamed (stock "Workbench" process, no DO_LAUNCHER).
Same try.sh as round 1 (rig.py reboot, settle 20 s, Copy, 30 s), IDE count now diffed per try.
Gotcha: `rig.py start` after a dead rig re-clones sys.hdf and drops every guest switch; they were re-applied each time.

| Split | What ran at Copy time (ps) | Crash | Decisive line |
|-------|----------------------------|-------|---------------|
| S1-pre a | SolidWin only (switch leaked: no Amarchy, no DOpus) | yes | reset-loop-20261104-085903.fs-uae.log: SetSwitch amiga x2, BEAMCON0 0000->0020 PC=10031f1e, `CPU reset PC=f80dc8`, 86 resets |
| S1-pre b | SolidWin only | yes | rig gone (connection refused), ide 0->0 |
| S1a | nothing of ours but wasabid/UPConsole/upgetty/shadows: stock Workbench only | no | alive, ide 0->0 |
| S1b | same | no | alive, ide 0->0 |
| S2a | Amarchy (bar, gui x2, menu, bar idle task), no DOpus, no SolidWin | yes | `IDE1 DATA write without request` 0->26, rig gone |
| S2b | same | no | alive, ide 0->0 |

Reading:
- The control boot (no Amarchy, no SolidWin, no DOpus) is clean 2 of 2. Third-party SolidWin 1.12 (Thomas Rapp, dist/solidwin)
  ALONE crashes 2 of 2; Amarchy alone crashes 1 of 2. So Amarchy is sufficient but not necessary: the trigger is shared by
  an unrelated Intuition patcher, which points away from one Amarchy patch and toward the palette-prefs Workbench reset
  (IPrefs) racing any program that holds Intuition patches/windows -- or a system/emulator fault that a bare WB misses by luck.
- Caveat: with ~75% crash rate elsewhere, 0 of 2 on the control has ~6% odds by chance; the control needs ~4 more clean
  runs before "a bare boot never crashes" is a fact.
- S3 (Amarchy patches off one at a time) not run: budget, and the SolidWin-only crash makes a single Amarchy patch an
  unlikely sole cause. Next: (1) control x4 more; (2) SolidWin alone x2 more on clean switches; (3) only if (1) stays clean,
  S3 on Amarchy, plus --mmu + MuForce on the SolidWin-only boot to catch the faulting PC (BEAMCON0 write at 10031f1e is
  in RAM: resolve which module owns 10031f1e on that boot).
- Side note: the "Amarchy bar idle" task runs with a 4096-byte stack at pri -128 (RULES 6: size from what it calls) -- unchecked.
