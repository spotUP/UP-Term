---
date: 2026-10-30
topic: VSH-EXT-RC161 fix design (vsh stops using NP_ExitData)
status: design, owner chose the vsh layer
---

# VSH-EXT-RC161: vsh stops setting NP_ExitData

## Cause (s32, thoughts/shared/research/s32/rc161-cause.md)
vsh starts the runner (shell/vsh.c:1272), the subshell (2062) and the SystemTags child (721) with
NP_ExitData = the job pointer, as a job identity tag. A loaded command runs inside that process
(RunCommand), so an ixemul program sees pr_ExitData != 0. ixnet's init_inet_daemon (ixnet/socket.c
~1079) reads pr_ExitData as an AmiTCP inetd DaemonMessage, TCP_ObtainSocket fails, exit(161). Every
AmiTCP netlib program does the same: pr_ExitData != 0 means "started by inetd". So vsh must leave
pr_ExitData at 0 in every process it makes.

## Invariant
A process vsh creates carries no value in a field another program interprets. Job identity lives
in a field programs only display (the task name) or in vsh's own data.

## New identity tags
1. Runner and subshell: a per-job unique task name, NP_Name = j->tname, built as
   "vsh job <n>" / "vsh subshell <n>" where <n> is a shell-wide counter that never repeats in the
   shell's lifetime (not the job pointer: a freed job's address is reused). CreateNewProc copies
   the name, so compare by string, not by pointer.
   - runner_alive(j): task_alive(j->task) && is a Process && strcmp(ln_Name, j->tname) == 0.
     Keeps the ABA guard: a new task at a dead runner's address has another name.
2. SystemTags child (Resident command or script): SystemTags ignores NP_Name, so the name cannot
   tag it. Identify it by its CLI standard input: a CLI process (pr_CLI != 0) whose
   cli_StandardInput == j->in, that is not j->task, not the shell's own process, and whose name
   does not start with "vsh " (other vsh runners/subshells that inherited the same handle).
   - j->in is unique per job for background jobs (NIL: opened per job, or an owned pipe) and for
     interactive foreground jobs (own_handle). A non-interactive foreground job shares the shell's
     input; the exclusions above cover the shell and its runners. A nested vsh runner inside the
     child has a "vsh " name and is excluded.
3. Drop NP_ExitData from all three CreateNewProc/SystemTags calls.

## Pure helpers (shell/sh_jobgone.h, host-tested in tests/test_sh_jobgone.c)
- sh_runner_is(is_process, const char *name, const char *tname): is_process && name && strcmp == 0.
- sh_child_is(is_process, has_cli, cli_in, job_in, is_runner, is_self, const char *name):
  is_process && has_cli && cli_in == job_in && job_in != 0 && !is_runner && !is_self &&
  !(name starts with "vsh ").
- Comments say why: pr_ExitData is AmiTCP's inetd DaemonMessage, not free for a shell to use.

## Regression test (reachability)
Rig, through the top-level entry (vsh -c on a rig, ixemul installed):
- `vsh -c "UP-Term:bin/true; echo st=$?"` prints st=0 (before: st=161).
- `vsh -c "UP-Term:bin/mkdir RAM:rc161; echo st=$?"` prints st=0 and RAM:rc161 exists.
- A background job and a script path (SystemTags child) still take Ctrl-C and still end
  (job_child finds the child): e.g. a script running `Wait 30` in the background, `kill %1`
  or Ctrl-C ends it.
- Sentinel: the test must fail with the fix reverted (st=161).
Host: tests/test_sh_jobgone.c covers the new helpers, including the ABA case (same address,
other name) and the shared-input case (shell's own process excluded).

## Result
Worktree /Users/spot/Code/UP-Term/vtcon-rc161, branch fix/vsh-exitdata (not pushed).
- Commits: 3f2331dd (vsh.c: NP_ExitData removed at the three sites, tname per job "vsh job <n>"/"vsh subshell <n>" via sh_job_tname, runner_alive by name, job_child by CLI input handle excluding runner/self/"vsh " names; helpers + 24 host checks in test_sh_jobgone), then the rc161_rig.py commit.
- Readers of "vsh job"/"vsh subshell" (vsh_orphan_job_rig.py, kill_force_session.py): `wasabi ps 'vsh job*'` globs and `'vsh job' in line` substring matches; both still match "vsh job 7". kill_force_session.py calls `wasabi kill --force 'vsh job'` (exact name): NOT checked, may need a glob 'vsh job*'.
- Rig 3, tools/rig/rc161_rig.py: new vsh: true st=0, mkdir st=0 + exists, bg_kill ended -> "rc161_rig: OK". Old vsh (main build): true st=161, mkdir st=161 -> FAIL. Sentinel proven.
- Unverified: bg_kill uses `Wait 30 &` which is likely a loaded command (runner path), so the SystemTags child path (job_child by input handle) is not exercised on the rig.
- Failing-first for host tests: the helper signatures changed with the fix, so old-code run is not possible; tests assert the new semantics (ABA by name, shell excluded, "vsh " name excluded).
- Mishap: one `make test` started in the main checkout by a failed cd chain; killed, only build/ touched there.

## Result 2
Worktree vtcon-rc161, branch fix/vsh-exitdata, rig 3 (not pushed).
- SystemTags branch reached and confirmed: `wasabi ps` while `vsh -c "RAM:rc161/w.script & wait"` runs shows `vsh job 1 (RAM:rc161/vsh)` (runner) and a new `Background CLI (Wait)` Shell process (the script's SystemTags child).
- a. Resident ixemul: `Resident rtrue UP-Term:bin/true PURE` (without PURE: "object is not of required type"; the name rtrue avoids vsh's builtin `true`), `vsh -c "rtrue; echo st=$?"` -> `st=0`; Resident removed after. OK.
- b. Background script `Wait 30`: `script & sleep 2; kill %1; wait; echo ended` -> `***Break` + `ended` within 8 s (job_signal reaches the child found by input handle). OK.
- OPEN ISSUE (race, not redesigned): `script & kill %1` with NO delay hangs (still running after 8 s; Wait ran its full 30 s). The child does not exist yet when kill runs, so job_signal finds no child and the signal is lost. Bare loaded commands (bg_kill, `Wait 30 & kill %1`) are fine (runner path). Not checked whether the old code had the same race.
- kill_force_session.py: wasabi kill matches the task name EXACTLY (case-insensitive) or the CLI command, or a 0x address (build/wasabi src/wasabid.c cmd_kill; "ambiguous" on several). Now kills the runner by the address from ps. NOT run (needs an XCON window session).
- Mishap: one ad-hoc probe ran without UPTERM_RIG=3 and hit rig 1: a failed `Resident` and two small files RAM:res.txt, RAM:res2.txt left there (ami also printed a [REFRESH] of 3 files on rig 1).
