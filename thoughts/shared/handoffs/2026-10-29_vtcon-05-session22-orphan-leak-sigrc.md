---
date: 2026-10-29
session: vtcon-05 s22
status: finish line reached; follow-ups open
---

# vtcon-05 session 22: refresh gate GG:bin/sh, vsh orphan code leak, ixemul signal rc

## Done (vtcon origin/main a2105b8a, ixemul-vtcon 45c9311f)
- Rig refresh gate: GG:bin/sh is in program_pairs, so drift, refresh and gate signature use one list. Before this fix, the gate reported GG:bin/sh as stale but never copied it. Test: tests/test_rig_ensure_current.py test_stale_gg_sh_is_refreshed_with_vsh, which fails when the fix is reverted.
- vsh orphan code leak (about 250 KB per shell that orphaned a job): orphan_end gives the code to DOS through the last runner's pr_SegList[3] plus PRF_FREESEGLIST. segs[3] of an NP_Entry process already holds a DOS jump segment inside the array's allocation, so the guard checks the flag, not an empty entry. Test: tools/rig/vsh_orphan_job_rig.py test 4 (0 bytes lost; 1253696 bytes lost with the old guard).
- VSH-WAIT-RC-SIGNAL: ixemul PR 8 merged. ix_startup returns 128+sig. Probe: tools/rig/sigexit_rc_rig.py, tests/amiga/ixsigexit.c.

## Open, in order
1. IXEMUL-SIGRC-DEPLOY: the rigs and the kit load ixemul from ../ixemul-vtcon/build295. That checkout holds another session's feature/ixemul-80 (dirty, 38 behind main). Do not rebuild there. When that branch has main merged and build295 is rebuilt, run `UPTERM_RIG=<n> tools/rig/sigexit_rc_rig.py` with no IXEMUL=. It must show rc 143/130.
2. Local main diverged from origin (49 ahead, 186 behind). The rebase waits for vtcon-32's omarchy-ai plan commit. Make a backup branch first. Until then, push via a detached worktree on origin/main.
3. Pre-existing failures on origin/main: test_rig_kit_stamp GoScript::test_shadow_pass_sits_behind_the_stamp and test_rig_kit_shadow_keymap (amnotify). These are not from s22. Find the owner session before fixing.
4. upterm-ports PR 25 (opkg 0060) is open. PR 26 (coreutils kill) waits on the owner.

Ledger: thoughts/spot/todos.md section "## s22 (2026-10-29) vtcon-05".
