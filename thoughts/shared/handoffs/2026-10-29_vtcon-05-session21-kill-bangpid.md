---
date: 2026-10-29
session: vtcon-05 s21
topic: KILL-BUS-ERROR, vsh $!, rig drift dup
status: partly blocked on owner
---

# vtcon-05 session 21: kill, vsh $!, drift dup

## Done
- RIG-DRIFT-DUP-MD5SUM: program_pairs skips userland names already built (origin f69ae3eb), test in tests/test_rig_vtc_drift_refresh.py.
- KILL-BUS-ERROR cause: UP-Term:bin/kill is GG coreutils 5.2.1 (traps on any pid). upterm-ports PR 26 (branch fix/coreutils-kill, commit 1f721fe) builds the 9.12 kill; rig 9: kill -0/-INT/-TERM on a real task pid work.
- The "No such process" seen first was a test artefact: vsh $! held a job pointer.
- VSH-BANG-PID (owner decision): $! is the process Task address. origin/main 8f21e192, test bang_is_the_process_pid, rig 9 PASS.

## Open, in order
1. Owner merges upterm-ports PR 26 (auto-mode refused gh pr merge). Then, with the owner's go-ahead (outside vtcon): re-pin through the upterm meta-repo `bin/upterm-bootstrap --update`, make dist, rig check `kill -0 $!` / `kill -INT $!` with UP-Term:bin/kill. Remove worktree /Users/spot/Code/UP-Term/upterm-ports-coreutils-kill after the merge.
2. VSH-BG-IGNORES-TERM (todos.md): target started by `sh -c "RAM:kp &"` survives INT and TERM from ixkill and kill-new. TERM must end it (bash ignores only INT/QUIT for async jobs in non-interactive shells). Evidence: scratchpad kill-esrch.md "s21 INT hang check" (copy into thoughts if needed).
3. 5.2.1-only coreutils still shipped (hostname, pinky, who) may trap like kill did.
4. Item 1 of s20: rebase diverged local main after vtcon-32 commits thoughts/shared/plans/2026-10-28-omarchy-ai.md (backup branch first).
5. Unexplained one-off: rig 9 lockup after kill-new -INT at host load 19 (not reproduced).
