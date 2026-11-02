---
date: 2026-11-02
topic: PIPE-FREEZE - vsh pipeline (builtin/subshell stage | external last stage) hangs, then the Amiga freezes
status: root cause NOT found; budget spent; two facts narrowed
branch: s51-pipefreeze (from origin/main b0f5c9da)
rig: 83 (060 max), build from this worktree
---

# PIPE-FREEZE findings (partial)

## Result
Root cause not found inside the 45-call budget. No fix, no regression test.
Two facts narrow the hunt.

## Fact 1: the pipe is PTY:, not PIPE:
On the rig PTY: is mounted (rig.py startup-sequence mounts VTC:ptymount), so
os_pipe() (shell/vsh.c:625) uses pty_pipe() (shell/vsh.c:588), and the pipe is
our own handler/pty_handler.c in pipe mode ("PTY:v<task><n>/w" and "/r").
Snoop (build/rig83/wasabi/snoop.log, printf_wc, first run after boot):
vsh opens PTY:v103a1c681/w (create) and /r, LoadSegs VTC:userland/bin/wc,
the runner sets local vars, wc (ixemul) opens bsdsocket, ixnet, timer.device,
ixemul.library v48, FindTask - last line 21:38:41, then
"snoop: nothing heard for 20 s - the machine is frozen". Amarchy idle calls stop too:
the whole machine froze, no Guru in serial.log.
The freeze falls while wc's ixemul start-up runs concurrently with the printf
subshell writing to and closing the write end.

## Fact 2: forcing PIPE: still hangs vsh
Probe build: pty_pipe() made to return 0 (`if (1 || !mounted)`, not committed),
so every pipe is PIPE:vsh.<task>.<n>. vshpipe_rig cases, 5 runs each, loop stops
at first FAIL:
- printf_wc: OK, OK, FAIL "hung (no end after 20 s)" at run 2
- echo_wc: OK, FAIL hung at run 1
- func_stage: FAIL hung at run 0
- three_stages: not reached (stopped)
After each hang the next case still ran and passed run 0, so with PIPE: the
machine likely kept answering (unverified: snoop was not restarted after the
reboot, so there is no snoop trace for the probe run).
Conclusion: the vsh hang is NOT PTY-handler specific; it lives in vsh
(subshell stage / runner / job wait). The whole-machine freeze may be a
second effect that only the PTY: path (or timing) produces.

## Code read, no defect proven
- handler/pty_handler.c service()/slave_writes()/master_reads(): every loop
  either shrinks a queue or breaks; no spin found. ACTION_END on a pipe:
  replies outstanding reads with 0, end_pair().
- shell/vsh.c job_free (797) unlinks live_jobs but does not clear
  pipe_tab[].writer; os_wait (1603) clears writer==j before job_free, but the
  subshell orphan path in subshell_proc (2260) calls job_free directly.
  A stale pipe_tab writer then reaches job_break -> Signal on freed memory
  from close_stream (436), which runs under Forbid. Candidate for the
  machine freeze; not proven.
- job_child (164) walks TaskReady/TaskWait under Forbid; a corrupt task list
  would spin forever there with no Guru. Fits the symptom; not proven.

## Next hunt (suggested order)
1. Rule-6 probe: same case under `Stack 262144` (not run).
2. Trace build: kprintf at subshell_proc end, close_stream, os_wait loop,
   runner ReplyMsg; run func_stage (fails at run 0 with PIPE:, quickest).
3. With PIPE: (no freeze), ps/taskdump during the hang: which task waits on
   what (vsh in os_wait? runner in Read? subshell not replied?).
4. Check subshell_proc orphan path vs pipe_tab writer clearing.

## Tools
Scratch loop script (not committed): imports tools/rig/vshpipe_rig as v,
stages vsh + coreutils like main(), runs v.run_case(case) N times, stops at
first FAIL.
