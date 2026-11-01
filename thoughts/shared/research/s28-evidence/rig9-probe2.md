# s28 rig 9 (AmigaOS 3.2): probe 2, flat-on with and without amarchy-flat

Question (rig8-crash-analysis.md probe 2): does flat-on hang when amarchy-flat is NOT running?

Answer: NO, and the probe does not split the suspects the way the analysis assumed. Neither run hung, and the
control (amarchy-flat running, same sequence as rig 8) did NOT reproduce the rig 8 hang on rig 9.

Setup (2026-11-01 ~02:25 CET, `UPTERM_RIG=9 rig.py start --max --os 32`): RAM:Amarchy, amarchy-menu, amarchy-gui,
amarchy-flat from the worktree build/amiga (host md5 Amarchy 8a350a9a..., amarchy-flat 18d06679..., same as rig 8).
UP-Term:bin has no amarchy-flat on this rig, so Amarchy's dk_flat_sync pl_start cannot launch one.
`Execute RAM:go` (Stack 262144 / Stack / Run >NIL: RAM:Amarchy), flat prefs off first; "Current stack size is 262144 bytes".

Run 1, amarchy-flat absent (`amarchy-flat STATUS` = `flat=absent` before and after):
- `put_flat_prefs(True)` returned after 1.0 s. 60 s later: `rig.py status` = `wasabid 0.11b2` (alive).
- wasabi ps: Amarchy (Background CLI, FREE 261914), amarchy-menu, amarchy-gui, Workbench, IPrefs all `wait`.
- Host CPU rig 9 fs-uae 81.9%. ENV:Amarchy/prefs reads flat-system-look = on, flat-window-gadgets = on.
- No T:amarchy-revert, no ENV:Sys/Palette.prefs or WBPattern.prefs written: the desk editors did not run.

Run 2, control with amarchy-flat running (STATUS before: `flat=off ... rectfill installed=yes ... text installed=yes ... frame=hooked`):
- `put_flat_prefs(False)`, `Run >NIL: RAM:amarchy-flat`, then `put_flat_prefs(True)`: returned after 6.4 s (rig 8: the Delete never returned).
- 60 s later: `rig.py status` = `wasabid 0.11b2` (alive). wasabi ps: Amarchy `ready` FREE 233502, amarchy-flat `wait` FREE 12314/16384.
- Host CPU rig 9 fs-uae 95.7%. amarchy-flat STATUS afterwards still `flat=off calls=0`.

Why the probe cannot split desk path vs patches:
- A prefs write never reaches the desk path. desk_dos_apply (dk_flat_pens, dk_wbborder, dk_flat_sync; desk_dos.c:632)
  is called only from gt_dos_select (gthemes_dos.c:224), i.e. a theme pick (KA_GTHEME, amarchy.c:310; leaf.c:370).
  So in rig 8's sequence the Palette/WBPattern/Workbench editors never ran either; suspect 2 (desk race) is not
  exercised by put_flat_prefs at all.
- The prefs write also never turned flat on in the running amarchy-flat (STATUS stayed flat=off), so rig 8's hang
  happened with flat OFF, during the Delete of ENV:Amarchy/prefs, while only the always-installed RectFill/Text/frame
  patches were live. That fits suspects 1/3 (patches active with flat off), not "flat-on".

Not reproduced on rig 9: the rig 8 hang is intermittent or rig-8-specific (rig 8 runs the fs-uae-debug binary).
Next useful probe: a theme pick (the real flat-on path, gt_dos_select) with amarchy-flat running, repeated a few
times, on a `--debugger` rig to read the PC if it wedges.
Rig 9 stopped and cleared after the runs.
