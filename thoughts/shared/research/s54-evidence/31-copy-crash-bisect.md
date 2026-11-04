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

## Round 3 (rig 67, AmigaOS 3.1, main 117c7add)

Same switches and try.sh as round 2 (rig re-cloned and switches re-applied after every dead rig). ps at Copy time
listed for each try; "control" = only `Workbench` (stock, DOpus5 off) of the watched names, no Amarchy, no SolidWin.

| Try | What ran at Copy time | Crash | Decisive line |
|-----|-----------------------|-------|---------------|
| C1 | control | no | alive, ide 0->0 |
| C2 | control | no | alive, ide 0->0 |
| C3 | control | yes | `BEAMCON0 0000 -> 0020 PC=10031f06`, `SetSwitch() - Picasso96`, `Illegal instruction: 0b7a at 000000A6 -> 00000000`, then 24542 x `filesys_lput/wput called PC=0000007a`; FS-UAE exited |
| SW1 | SolidWin only | yes | `BEAMCON0 0000 -> 0020 PC=10031f1e`, `Illegal instruction: 0b7a at 000000A6`, `filesys_*put called PC=0000007a`; FS-UAE exited |
| SW2 | SolidWin only | no | alive, ide 0->0 |
| C4 | control | yes | `BEAMCON0 0000 -> 0020 PC=1003211e`, `Illegal instruction: 0b7a at 000000A6`, `IDE1 DATA write without request!? 00 PC=0000007A` (ide 0->28, the original symptom) |

Totals with round 2: control 2 of 6 crash, SolidWin only 3 of 4, Amarchy only 1 of 2.

Owner of 10031f1e (SolidWin-only boot, ARexx IMPORT over exec/library jump tables and process seglists, scratchpad own.rexx/ex.rexx):
- No process seglist holds it (SolidWin's segments are 101AF51C-101B0CFC, 100FC58C-100FC774, 13F4-1560).
- exec.library -60 (Dispatch) -> 10031EF0 and 14 more exec vectors (-30 Supervisor, -54 Switch, -36, -162, -168, ...) point into
  100316DE-100323B6, the code of 68060.library (base 10029B58, its own vectors 100316DE-10031CAC): 68060.library's exec
  Dispatch replacement.
- Bytes at 10031F1A: `08EE 0007 0124` (BSET #7,$124(A6)) then at 10031F1E `4E72 2000` = STOP #$2000: the idle-loop STOP
  inside that Dispatch. So PC 10031f1e is where the CPU sat idle when BEAMCON0 changed -- the write itself came from the
  copper (the native view's copper list), not from CPU code at that PC. 10031f06 / 1003211e on other boots are the same
  68060.library Dispatch region. The BEAMCON0 line is not a culprit, only a timestamp of the RTG -> native switch.

Reading:
- The crash happens with NO Intuition patcher running (C3, C4): the bug is not Amarchy's and not SolidWin's. Patchers only
  change the odds (control ~1 in 3, patchers ~3 in 4). Every crash has the same chain: palette.prefs Copy -> IPrefs resets
  WB -> SetSwitch amiga x2 (RTG off) -> native view -> SetSwitch Picasso96 (RTG back on) -> CPU executes at $A6/$7A (jump
  through a NULL/zeroed vector) -> writes land in the filesys/IDE space.
- Suspects now: Picasso96 (rtg.library / the RTG mode switch during the WB reopen) or the 68060.library/JIT interaction on
  the emulated 060. Next: same control Copy with --mmu + MuForce (catches the jump to low memory with a PC and task),
  and one control on a non-RTG (AGA native) WB screen mode to see if the RTG switch is required.
- S3 (Amarchy patches off one at a time) not run: control crashed, so no Amarchy patch is the cause.
