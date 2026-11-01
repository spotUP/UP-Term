---
date: 2026-10-29
session: vtcon-05 s21
topic: KILL-BUS-ERROR, vsh $!, orphan jobs, rig refresh gate, kit re-pin with lynx
status: handoff
---

# vtcon-05 session 21: kill, vsh $!, orphan jobs, rig refresh gate, kit pin

## Done
- RIG-DRIFT-DUP-MD5SUM: program_pairs skips userland names already built (origin f69ae3eb).
- KILL-BUS-ERROR: UP-Term:bin/kill was GG coreutils 5.2.1 (traps on any pid). upterm-ports PR 26 merged (474dd78a)
  builds the 9.12 kill. Kit dist from 474dd78 (lynx held) and rig 10 PASS: `kill --version` 9.12, `kill -0` and
  `kill -TERM` on a `Run >NIL:` sleeper by its decimal Task address end it, no requester.
- VSH-BANG-PID: $! is the process Task address (origin 8f21e192).
- STALE-VTC-SH: refresh_programs removes RETIRED_VTC_FILES (vtc/bin/sh) (origin 04b827cc).
- VSH-ORPHAN-JOB (the "TERM hang"): not signal-related. `sh -c "cmd &"` exited before the runner started, the Shell
  unloaded vsh's code and deleted the reply port, the job task waited for ever. Fix: live_jobs + orphan_jobs() at exit,
  cli_Module=0, runner frees itself; job strings in AllocVec (vsh's pool is deleted at exit by _EXIT_1_vsh_mem).
  Subshell jobs too, with vsh_heap_hold/vsh_heap_release. Rig tests tools/rig/vsh_orphan_job_rig.py, proven by revert.
  origin 0e2997ab, 923d0795, e18967ba.
- RIG-ALWAYS-CURRENT (owner: "keeps all rigs up to date always"): rig.ensure_current() gate refreshes a drifted
  build/rigN/vtc before every gated rig.py command, every ami.req and wasabimcp launch; a changed LIBS:/devs/l file
  reboots a quiet own rig, else [WARN]. tests/test_rig_ensure_current.py. origin 2119aafe. Not yet seen live.
- lynx source: invisible-island tarball 403. upterm-ports PR 27 merged (e2bcbef2): recipe fetches the author's
  GitHub tag v2-9-3 (sha256 914e1dfb...), tree identical to Debian's orig.tar.bz2.
- Owner rule: Claude may merge PRs whenever needed, checks green (CLAUDE.md §4 Merging, memory feedback-merge-allowed).

## Kit
- Kit pinned to upterm-ports e2bcbef2 (spotUP/upterm 99fa5c9), make dist rc 0, lynx back as up-lynx .ipk;
  worktree upterm-ports-main removed.

## Open, in order (rows in thoughts/spot/todos.md)
1. Watch the first live `[REFRESH] rig N:` line from the gate (any rig command after a build).
2. VSH-WAIT-RC-SIGNAL: `wait $!` on a TERM'd job returns 0; bash returns 143.
3. VSH-PIPE-TAB-DANGLE: pipe_tab writer pointer can dangle after an orphan frees its job.
4. About 240 KB of vsh code leaks per shell that exits with live jobs.
5. 5.2.1-only coreutils still shipped (hostname, pinky, who) may trap like kill did.
6. Rebase diverged local main after vtcon-32 commits thoughts/shared/plans/2026-10-28-omarchy-ai.md (backup branch
   first). Until then push through a detached worktree on origin/main.
7. upterm-ports PR 25 (opkg patch 0060) still open.
