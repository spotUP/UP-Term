# "Please insert volume SYSTEM:" on first boot of rig 94 (read-only hunt, 2026-11-08)

Status: NOT root-caused. Premise in the brief is contradicted by the boot log. No rig touched.

## Evidence
- The first-boot log order ("early wasabid start" ... "UP-Term shadows done" before "wait done") means the clone's
  S:User-Startup ran to the end of the shadows block before go printed "wait done". S:User-Startup runs only after
  DH0:S/Startup-Sequence line 18 (`Assign >NIL: ENV: RAM:ENV`). So ENV: WAS assigned. go's envwait loop
  (rig.py:476-480) is silent while ENV: is missing, but "wait done" being after User-Startup makes that unlikely.
  Stall point is therefore after the shadows block: the rest of User-Startup, Startup-Sequence tail (Resident REMOVE,
  C:LoadWB) or go's first lines after "wait done" (C:Assign ENV: EXISTS, Echo "env ok").
- The only literal naming System: in the clone is S:User-Startup:9-10 (`if exists "System:MUI"` / `assign MUI:`).
  It sits BEFORE the rig-wasabid block (line 42 of the pre-boot copy) and "early wasabid start" was logged, so that
  line completed. It is mixed case, the requester was uppercase SYSTEM:.
- No uppercase `SYSTEM:` exists in: build/rig94/boot (all scripts), dist/ (vshrc, tutorial, oldopts.installer only
  mixed case/prose), build/rig94/vtc, tools/rig/rig.py (only `system:` in a docstring at rig.py:741), clone S/ files.
  Checked: Startup-Sequence, User-Startup, Shell-Startup, DEVS:DOSDrivers (AUDIO CF0 IXPIPE PIPE PTY XCON),
  Prefs/Env-Archive listing. Not checked: file contents of Env-Archive/Sys, DOpus5* prefs, WBStartup (path missing in
  the listing), .info tooltypes, icon default tools (unverified).
- BOOTX:s/startup-sequence (build/rig94/boot/s/startup-sequence) names only DH0:, VTCX:, BOOTX:; never System:.
- rig.py:1504 `hard_drive_0_label` is set only for --os rigs; the clone uses the HDF's own label "System".
- A rig/clone state check at the time of this hunt shows the clone was rewritten by the dopus step (11:16-11:17), so the
  first-boot disk state is gone; fs-uae.log.txt and serial.log are from the retry boot (no requester text).
- Prior notes: no RIG-ENV-RACE note about a volume-name requester in thoughts/ (only the comment at rig.py:471-474).
  Closest precedents: thoughts/shared/research/s54-evidence/flat-boot-freeze-fix.md (a requester raised while a task
  holds LockPubScreenList cannot open, LoadWB + wasabid then wait on the list and the system stops; pens_theme read
  UP-Term: in Amarchy flat) and thoughts/spot/todos.md:2661 (UHC-Startup EXISTS on UP-Term: before the assign).

## Most plausible reading (UNVERIFIED)
A DOS lookup of a volume-name path raised by a boot task AFTER User-Startup (LoadWB/Workbench startup, Amarchy/flat
started at User-Startup line ~60, or the DH0 handler not yet validated when the first lookup after xdftool's write hit
it) cannot open its requester because the Workbench pubscreen list is locked, which stalls go too (it never reaches
"env ok"). The uppercase name points at a caller that upper-cases (a prefs/tooltype string, an icon default tool, or a
DOS-internal volume lookup), not at any rig text.

## Layer to fix
Unknown. Not rig.py wrapper text, not the clone's User-Startup line 9 (ran fine). Do not change anything on this
evidence.

## Next (needs a rig, not run)
On a freshly cloned boot that reproduces: `ami.py requesters` (task name owning the requester), `Assign LIST`,
fs-uae `-` snoop of Lock names, and keep the first-boot hdf copy before retry overwrites the clone.
