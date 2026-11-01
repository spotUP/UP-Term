---
date: 2026-10-31
session: vtcon-05 s34
status: unfinished
---
# vtcon-05 session 34 handoff

## Done (origin/main)
- 91e0e355 vsh kill TERM fallback: Ctrl-D to a vsh target (trap, exit 143), Ctrl-C to other programs. trapsig_rig kill checks B/C/C2/D pass on rig 5. Unproven: the non-vsh Ctrl-C fallback (check D's ixkill answered first). Notes: thoughts/shared/research/s34-kill-term-ctrld.md.
- 4daa83df kill_force_session.py 6/6 on rig 5. Notes: thoughts/shared/research/s34-kill-force-session.md.
- be085ec8 lesson 12, README, tutorial_rig, tabs_rig use Left Amiga T (super+shift+t /theme takes New tab's Right Amiga key; owner chose to keep it). Case verdict lists counter deltas first.
- 1cbcfdde, ff0b27ab: lesson12 step 5 findings in thoughts/shared/research/s34-lesson12-ramiga-t.md.
- Priority-1 busy-loop targets: closed, nothing to change (proc_rig sends no kill).
- Docker image ixemul-gcc295 deleted (Docker VM disk did not shrink; host still 14 GB free).

## Open, in order
1. lesson12 step 5 (drag/copy picks the wrong word). Read sections "## s34 step 5" and "## s34 step 5 fix" of thoughts/shared/research/s34-lesson12-ramiga-t.md. Lead: Screen in tools/rig/tutorial_rig.py assumes LINE = 16 (2x text); the step-5 dump shows 8-pixel pitch. Next probe: sc.rows() twice 1.5 s apart at step 5 vs step 3; then fix the finder (derive pitch from ink bands, or settle on 2x). Host test in tests/test_tutorial*.py must fail before the fix. Probe copy: scratchpad tutorial_rig.probe.py (previous session, may be gone).
2. Prove the non-vsh Ctrl-C fallback of vsh kill (a target where ixkill times out).
3. RERUN-AFTER-IPREFS: blocked until ../upterm-ports/build/ exists.
4. Disk under 20 GB floor: owner to compact Docker VM disk, or a read-only scan for other large files.

## Gotchas
- case lesson12 needs timeout ~400 s; rig boots ~5 min under load.
- From a worktree: ln -s /Users/spot/Code/UP-Term/vtcon/build build, remove before commit.
- Never run two tutorial_rig.py --only lesson12 at once on rig 4.
- Ledger rows: grep thoughts/spot/todos.md for "s34 TUTORIAL-LESSON12-RAMIGA-T" and "s34 KILL-".
