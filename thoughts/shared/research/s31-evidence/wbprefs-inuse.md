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

## Part 2 (s31 opus agent, rig 97, OS 3.2, 2026-11-02 20:56-20:59): snoop run, NOT reproduced, no fix

Setup deviations: the fresh-all install could not run. main's build/dist lacked Files/install-amissl.dos, and `make dist-base`
stops at "upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports" (another repo, not touched).
So the run used the plain `--os 32` rig (VTC: Amarchy, main build 5a6eaf58), Amarchy started by amarchy_rig.start, flat on by
put_flat_prefs(True), three picks by amarchy_desk_rig.pick (Keep clicked). Driver: scratchpad drive.py (not committed).

Result: no Program Error, no in-use failure (no `Error 202` anywhere in 8.6k snoop lines). Tool limit: wasabi snoop logs Open/Lock
but NOT Close/UnLock (0 Close lines), so how long a handle is held cannot be read from snoop; only open order and gaps.

Workbench.prefs order on the first flat-on pick (the only pick that writes it; picks 2 and 3 run Palette + WBPattern only,
dk_wbborder finds n = g = 0):

    20:57:19.110 amarchy-gui  SystemTagList SYS:Prefs/Palette ... USE      (IPrefs reads palette.prefs at 19.135, at once)
    ... 22 s in dk_wait_reset / dk_wait_iprefs ...
    20:57:41.390 amarchy-gui  SystemTagList SYS:Prefs/WBPattern ... USE    <- dk_flat_pens own-backdrop write, NO wait after it
    20:57:41.608 SYS:Prefs/Workbench Open("ENV:Sys/Workbench.prefs", create) = ok   (230 ms after WBPattern: dk_wbborder)
    20:57:45.643 Amarchy (main) Open("ENV:Sys/Workbench.prefs", read) = ok + Open("ENVARC:Sys/Workbench.prefs", create)  (Keep: persist)
    20:57:48.325 IPrefs       Open("ENV:Sys/workbench.prefs", read) = ok   <- 6.7 s after the USE, then Locks to 48.637
    20:57:48.441 IPrefs       Open("ENV:Sys/wbpattern.prefs", read) = ok   (IPrefs takes both notifies in one Workbench reset)

What this settles:
- The apply runs in the amarchy-gui process (VTCX:Apps/bin/amarchy-gui), Keep's persist in the Amarchy main process.
- IPrefs opens ENV:Sys/Workbench.prefs 6.7 s after our editor USE and holds it through its Workbench reset (Locks 48.33-48.64).
  Any MODE_NEWFILE on that file inside that window fails "in use". In this run no Workbench write fell in the window: our only
  Workbench USE came BEFORE IPrefs' open. A failure therefore needs a second Workbench-editor write within ~7 s of the first
  (or within a stalled reset): pick N+1 changing the border/gauge (flat toggle, dk_orig_off), the Revert (dk_wb_restore at the
  15 s timeout if IPrefs' reset is stalled, hypothesis 1), or the user's own SYS:Prefs/Workbench Save. Hypothesis 3 is the
  mechanism; hypothesis 2 is one way to land in it.
- dk_flat_pens leaves the WBPattern USE (desk_dos.c ~708/723) without dk_wait_iprefs, so dk_wbborder's Workbench USE lands
  while IPrefs has a pending wbpattern notify; both are then read in one late reset. Same gap as before every Workbench editor run.

Fix direction (not made: holder not caught, budget spent): invariant "no Amarchy write of an ENV:Sys prefs file while IPrefs has
an unprocessed notify or a running reset". Writer side (desk_dos.c): dk_wait_iprefs(idle) after the WBPattern dk_put in
dk_flat_pens, and after every Workbench editor run (dk_wbborder l.~853, dk_wb_restore l.~823, dk_wb_absent l.~794, dk_orig_off
l.~1308), the Font/Pointer pattern (l.426/448/1237). Host test: a desk_core decision "which editor runs need the IPrefs wait"
(none exists yet; desk_iprefs_idle is only the idle predicate, tests/test_desk.c:513).

Deciding run still needed: snoop with a Close-capable trace (wasabi snoop lacks Close; extend it, or poll `wasabi ps` IPrefs
state) and two flat toggles less than 7 s apart (flat on pick, then flat off pick at once), on the installed kit once
upterm-ports `make kit-stage` + `make dist-base` succeed.
