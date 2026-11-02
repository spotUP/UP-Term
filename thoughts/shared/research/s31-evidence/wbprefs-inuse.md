---
date: 2026-11-02
topic: AMARCHY-WBPREFS-INUSE ("env:sys/workbench.prefs is in use and cant be saved to", Program Error of SYS:Prefs/Workbench)
status: open, no live evidence, no fix
base: main ffb1ff16
---

# AMARCHY-WBPREFS-INUSE hunt (s31, opus agent)

## Live state (rig 91, read first, nothing pressed)

The failing state was gone by the time this agent read it (2026-11-02, ~21:xx):

- `ami.py requesters` (with AMI_HOST=127.0.0.1 so the VTC: gate did not touch the rig): no requester open.
- `uitree`: one screen (Workbench, depth 8), one window "Workbench". No Program Error window.
- `wasabi ps`: only Workbench, Workbench Clipboard Server, IPrefs. No Amarchy, no amarchy-flat, no SYS:Prefs/Workbench.
- `Status FULL`: wasabid x2, Workbench, Status.

So Amarchy was not running and the requester had been answered or the rig restarted before this read. The holder of
ENV:Sys/Workbench.prefs could not be read live. Next repro must snoop BEFORE the pick (see below).

Tool note: from an agent worktree without build/amiga, `ami.py` with only UPTERM_RIG crashes in gate()
(kitset.derive needs build/amiga). `AMI_HOST=127.0.0.1 UPTERM_RIG=<n>` skips the gate (read-only use, does not refresh VTC:).
wasabi directly: `python3 -I build/wasabi/wasabi --host 127.0.0.1 --port 1233+N --key rigwasabi ps|snoop`.

## Code facts (desk_dos.c, main ffb1ff16)

- dk_editor (l.90) is synchronous (SystemTags, no SYS_Asynch). No two Workbench editors overlap inside ONE process.
- Amarchy's own file helpers close every handle on every path (dk_read l.31, dk_write l.42, dk_stream_copy l.877,
  dk_files_equal l.902). No leaked handle in Amarchy holds the file.
- The codebase already records the mechanism twice: after an editor USE, IPrefs opens the ENV: file on its notify
  (1.5-7 s later on 3.2, rig 56 snoop, comment l.784), and a write opened meanwhile fails "object in use"
  (dk_restore_targets l.1236 comment DESK-PTR-REVERT, dk_stream_copy l.887 error 202). The Font/Pointer editor runs
  get dk_wait_iprefs after them; NO Workbench editor run does (l.794 dk_wb_absent, l.823 dk_wb_restore, l.862 dk_wbborder,
  l.1308 dk_orig_off). dk_orig_off follows its editor with dk_env_fix_file at once (retried, so it survives).
- Workbench editor writers by process:
  - menu process (gthemes_dos.c:225 gt_dos_select -> desk_dos_apply): dk_orig_off (flat off only) then dk_wbborder.
  - Amarchy process: desk_dos_start_apply (start), desk_dos_revert via the confirm (desk_confirm_dos.c:63, Keep timeout
    or Revert), desk_dos_confirmed's dk_stream_copy reads ENV:Sys/Workbench.prefs (DK_WBPEND set) on Keep.
  So two processes can touch the file at once: pick N+1 in the menu while pick N's Keep/Revert runs in Amarchy.
- Owner-demo finding 2 (rig 96, owner-demo-rig.md): flat turned on, then the FIRST pick. In that apply the only Workbench
  write is dk_wbborder (flat on: dk_orig_off has wb=0), right after dk_apply's palette write and dk_flat_pens' palette
  write. With no earlier Workbench.prefs write of ours, the "IPrefs still reading our previous USE" explanation does not
  cover this case.

## Hypotheses still open (ranked)

1. IPrefs holds ENV:Sys/Workbench.prefs during a Workbench screen reset caused by the palette write just before
   dk_wbborder (3.2 IPrefs re-applies Workbench prefs after reopening the screen), and that reset is stalled because
   dk_flat_pens writes palette.prefs with our windows open (RL15 suspect 1, l.698-704). Links RL15 and this bug, both
   only on 3.2, both after a flat-on pick. Unverified.
2. Cross-process overlap: menu-process dk_wbborder/dk_orig_off editor vs Amarchy-process revert (dk_wb_restore editor) or
   Keep (dk_stream_copy reading the ENV: file) of the previous pick. Needs two picks within the 15 s confirm window.
3. IPrefs reading the previous Workbench USE (no dk_wait_iprefs after any Workbench editor run).

## Next step (one run decides between them)

Own rig (`UPTERM_RIG=97 rig.py start --max --emu fsuae --os 32`), full install, flat off. Start
`wasabi snoop --ignore-wasabi --output normal --log <scratch>/snoop.log --for 120` with NO task filter, then
put_flat_prefs(True) + one pick (amarchy_desk_rig.pick). grep the log for `Workbench.prefs`: the task whose Open/Lock is
unmatched by a Close/UnLock when SYS:Prefs/Workbench's Open(MODE_NEWFILE) fails (`= fail (Error 202`) is the holder.
- IPrefs -> hypothesis 1: fix in dk_flat_pens (windows closed + dk_wait_iprefs around its palette write, the dk_apply
  pattern) and dk_wait_iprefs before every Workbench editor run; host test on the desk_core decision.
- Amarchy/menu task -> hypothesis 2: serialise desk writers across processes (one owner of the confirm/revert).

No code changed by this hunt: the holder was not observed and the code alone does not single out one cause.
