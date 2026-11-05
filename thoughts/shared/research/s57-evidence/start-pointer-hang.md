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

## Bisect step 1 (RL15 reverted): answered from the register, no rig run (2026-11-05, vtcon-c1)

- Verdict: RL15 (8a704261, dk_flat_pens window bracket) is NOT the cause.
- `git diff fdb5e99f 69f34ee9` outside thoughts/: the only shipping-code change is 8a704261's 7 lines in
  amarchy/desk_dos.c. The rest is the palette_closes test target, tests/amiga/copcheck.c and rawdrag.c (CONPROBES, not in
  the kit), tests/test_flat_core.c and tools/rig/amarchy_roundaa_rig.py. So "main with RL15 reverted" builds the same kit
  as fdb5e99f.
- fdb5e99f does not contain 8a704261 (`git merge-base --is-ancestor`: no). Rig 131 ran `installer_rig.py ONLY fresh-all`
  on it (register row 108, rig96-drag-freeze.md "Hunt attempt 3"): checks 1-33 ok, then FAIL 34 "AMARCHY port: no port", then
  [ABORT] wasabid silent for 15 s after the post-install reboot. That is the same failure as rigs 128 and 130.
- Rig 125 (df0c1923, also without RL15, register row 99): the same silent wasabid after the post-install reboot.
- Register rule 9: the same build, config and recipe means no rerun, so rig 132 was not started.
- Between the PASS baseline 7cf4ac17 (rows 83 and 87) and fdb5e99f, the only shipping change is RL20 (4ea76673:
  amarchy/gui.c fill_round/rich_put, richaa_core.c; amarchy-gui links it). RL20 is the remaining code suspect.
- Confound, not ruled out: every failing run was on a loaded host. Rig 125: host about 610% CPU. Rig 131: rigs
  96/127/129 plus a node pre-push hook, each at 85-100%. A vitest at 210-280% was also running at this check. The PASS
  runs' host load was not recorded. A wasabid that stops answering while fs-uae runs at about 110% also fits guest
  starvation (memory "Rig hang? check host load").
- Next (needs a new brief): fresh-all on 7cf4ac17 again on a quiet host, and on fdb5e99f with RL20's gui.c reverted,
  both with host load logged. That separates "RL20" from "host load / intermittent".

## A/B fresh-all (rig 133, 2026-11-05, vtcon-c1)

Recipe both runs: `rig.py start --max --os 32 --idle-minutes 0 --allow-stale-base ...`, then
`INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all`; build = make worktree-ready/wasabi/amiga, `make dist` via kit-main.
Host load logged every 60 s (`ps -Ao pcpu,command | sort -rn | head -5`).

| Build | Verdict | Failing step text | Max non-fs-uae host CPU |
|---|---|---|---|
| A 7cf4ac17 | FAIL | 34 "after the reboot: the AMARCHY ARexx port answers STATUS: no port"; then [ABORT] wasabid silent 15 s | 94% (Defender epsext, clang); rig 96 fs-uae about 110% |
| B fdb5e99f minus RL20 (git revert -n 4ea76673, clean) | FAIL | same text, same abort | 114% (Spotlight mdworker); clang about 96% before start |

- Conclusion: RL20 is NOT the cause. The build recorded as PASS (7cf4ac17, rows 83/87) now fails identically, so the
  failure is not a code change between 7cf4ac17 and fdb5e99f. Remaining suspects: host load/state (the PASS runs were
  on a rig 96 with a different history) or an environment difference (the earlier PASS runs used --keep rigs that were
  not fresh-started the same way). Host load here was moderate (under 120% non-fs-uae), so heavy load alone is not
  needed to fail.
- No ps/windows (wasabid silent) and no screenshot (rig already down after the abort). Not dug further, per brief.
