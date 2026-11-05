---
date: 2026-11-05
topic: S57-START-POINTER-EDITOR-HANG
status: open (cause not proven on a rig)
---

# Amarchy start blocks on SYS:Prefs/Pointer after a fresh 3.2 install

## Symptom (rig 128, s57-rl15, test register row 106)

- After the Installer reboot, Workbench shows only the RAM Disk icon and a busy pointer.
- `wasabi ps`: process "Background CLI (SYS:Prefs/Pointer)" in wait for minutes.
- Installer step 34 fails: the AMARCHY ARexx port never appears.

## Static reading (worktree da62f6b3)

- amarchy.c opens the AMARCHY port (rexx_host_open, line ~582) only after the
  start tick has run desk_dos_flat_pens_refresh, marker_recover, wb_backdrop and
  desk_dos_start_apply. So any editor that blocks inside the start apply also
  keeps the port from opening.
- desk_dos_start_apply (desk_dos.c ~1456-1592) writes the palette (dk_put_palette).
  It then starts `SYS:Prefs/Pointer FROM ... USE` through dk_put/dk_editor with
  no IPrefs idle wait. dk_wait_reset runs only when desk_pens_reset() says the
  pens changed. desk_editor_waits_iprefs() (desk_core.c:204) names only Workbench
  and WBPattern.
- dk_apply carries an older note: "SYS:Prefs/Pointer raised a Program Error
  requester when started under the running reset". The start apply does not
  follow that rule.
- dk_editor runs SystemTags synchronously with NIL: input and output. A requester
  or an IPrefs wait inside the editor therefore blocks Amarchy with no timeout.

## Candidate violated invariants

1. A prefs editor starts only when IPrefs is idle, after the palette notify has
   finished. The start apply breaks this for Pointer after a palette write.
2. The AMARCHY ARexx port exists before any start work that can block. Without
   it, the Installer check and the BAR STATUS/OFF calls from child processes
   depend on an external editor finishing.

## Fresh-all baseline

- Test register rows 83 and 87: fresh-all PASSED on main 9fd26e8c and 7cf4ac17.
- Between 7cf4ac17 and the rig 128 build, the only amarchy changes are 4ea76673
  (RL20) and the RL15 work (8a704261, 1229912b).
- The hang is therefore either intermittent, or caused by the extra
  dk_windows_close/open that RL15 put around the palette write.

## Rig 130 run (2026-11-05, worktree da62f6b3, --os 32, fresh-all)

- Build: make amiga and make dist (kit-main) both returned rc 0.
- Installer reached "VTC:distkit/Install (100% done)". Log: "[WARN] the
  Workbench stayed PAL (W43)".
- REPRODUCED: `FAIL 34 after the reboot: the AMARCHY ARexx port answers STATUS:
  no port`, then `[ABORT] fresh-all: auth failed: the Amiga took the connection
  but its daemon did not answer for 15 s`. installer_rig.py rc=2.
- Worse than rig 128: after the reboot wasabid itself stops answering. `ps`
  and `windows` time out; one `guru` call got through ("No Guru recorded").
  fs-uae for rig 130 at ~110% CPU (running, not a crashed emulator).
  So the block holds something that wasabid's ps/windows also need (an
  Intuition or Workbench lock, or a Forbid), not only Amarchy's own process.
  This points at a lock taken around the editor call (dk_windows_close /
  desk_wb_close in the main process) rather than at the Pointer editor alone.
  Not proven: no snoop could be taken.

## Open

- Reproduce on a rig:
  - `ps` for "SYS:Prefs/Pointer"
  - `wasabi windows` and `ui list` for the requester text
  - `snoop --task 'Pointer*' --entry` for the blocking call
- Run the Stack 262144 probe on the same run.
- Check whether "amarchy-flat BOOT left nothing resident" (ledger rows 4151 and
  4154) is the same block: the setup reads the resident list while the start
  apply hangs.
- Fix only after the measurement names the blocking call.
