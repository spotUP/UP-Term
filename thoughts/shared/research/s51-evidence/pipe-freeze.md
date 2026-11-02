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

## Hunt 2 (rig 83, 2026-11-02) - not fixed, findings only

### PTY: freeze traced to Close of the last pipe handle
Trace build (TR in close_stream, subshell start/end, os_wait tick), PTY: pipes:
func_stage OK, printf_wc OK, then echo_wc froze the whole machine (daemon silent 150 s).
Sequence for echo_wc:
1. Subshell stage started, closed its write end (`cs in/close/done`), ended, was waited by os_wait.
2. wc runner started; main vsh ticked twice in os_wait.
3. `runcommand back` with rc=1: wc FAILED (abnormal; expected "6").
4. close_stream on the read end: "cs rd drain ... 0" (writer already gone), "cs close", never "cs done".
5. Close() of the read end (pty master) never returns; main's os_wait ticks stop: whole machine dead.
So the hang is inside Close of the last handle of a PTY pipe pair, after the write end closed.
That Close drives ACTION_END -> end_pair -> free_pair -> handler shutdown (dn_Task=0 under Forbid,
CloseDevice, DeleteMsgPort, CloseLibrary, return) in handler/pty_handler.c (packet() ~591, END ~660).
Prime suspects, unproven:
- handler shuts down / frees the pair while a reply or the closer's packet still references the pair's
  ports (mport/sport share the handler signal, init_port ~390);
- wc rc=1 means wc's (ixemul) reads on the master got an unexpected answer first; a pty-handler TR
  build showing the packet sequence for this pair is the next probe.

### PIPE: forced (probe `if (1 || !mounted)` in pty_pipe, since reverted)
Trace build: 52 runs (func_stage, echo_wc, printf_wc, three_stages; 3 + 10 rounds) all OK.
The earlier agent's PIPE: hang did not reproduce with the trace build: timing-sensitive.

### Normal (non-DEBUG) build, PTY:
Loop of slow_wc (`{ /C/Wait 1; echo hello; } | wc -c`) and true_wc (`true | wc -c`) froze the machine
on the first rounds again (which case not captured, no trace in that build).

### Orphan-path lead (job_free without clearing pipe_tab writer)
Real latent use-after-free: job_free (vsh.c ~797) leaves pipe_tab[].writer pointing at the freed job on the
subshell_proc orphan path (~2297), the runner orphan path (end of runner()) and task_unix_signal (~381, ~392).
Only os_wait clears it. Fix shape: clear pipe_tab writer==j inside job_free under the same Forbid.
NOT the cause of these cases: orphan paths run only when mn_ReplyPort == 0 (orphan_jobs at shell end);
`vsh script` waits for every stage.

### Next probes, in order
1. pty-handler TR build: packet trace for the pair in echo_wc (which actions wc sends, the END sequence,
   whether shutdown runs while the master Close is still outstanding).
2. Same run with a non-ixemul last stage (echo_cat) to split handler bug vs ixemul packet.
3. Stack 262144 probe (memory rule) not run yet.

## Hunt 3 (rig 83, 2026-11-02) - not fixed, handler cleared for this capture

Build: main fe7bdb7c merged (4323352f), pty-handler `make DEBUG=1` (PTY_DEBUG serial TR on every packet and
reply, plus new TR points `end` (pair|master, masters<<16|slaves), `free`, `freed`, `exit`), normal vsh.
Run: `vshpipe_rig.py`, first round. Raw handler trace: `pipe-freeze-hunt3-trace.log` (49 lines).

### Result: froze in printf_wc, with the handler idle
- echo_wc passed: FINDOUTPUT, FINDINPUT, WRITE 5 + WRITE 1 (write end), END write end (masters 1,
  slaves 0), then on the read end SEEK, EXAMINE_FH, SEEK, SEEK, PARENT_FH (all refused, ixemul fstat
  probing), READ 6, READ 0, END (masters 0, slaves 0), `free` (no waits, no queued writes), `freed`.
  No `exit`: the next case's Open was already queued on the handler port (correct).
- printf_wc: same sequence up to and including the PARENT_FH reply (WRITE 4, END write end, SEEK,
  EXAMINE_FH, SEEK, SEEK, PARENT_FH). Then nothing: no READ ever reached the handler, the machine froze
  (wasabid "did not answer for 15 s"), at 22:09.
- So at the freeze the handler had replied every packet it was sent, had no queued read, write or
  WAIT_CHAR for this pair, and had not run END, free_pair or the shutdown path for it. The
  ACTION_END / free_pair / handler-exit race suspected in Hunt 2 is not what froze this run.
- Code read of the same paths found no reply-after-free, double reply, unbalanced Forbid, or spin:
  service() only loops when a packet was replied (every reply is traced), END replies before
  free_pair, free_pair answers waits and stray port messages before FreeVec, and the exit runs
  only with the port list empty under Forbid.

### Where the freeze is now
Between wc's (ixemul, dist/gg coreutils) ParentOfFH on its stdin and its first Read: inside wc's
startup or in another task running at that moment (the printf subshell has just ended its write end
and is exiting; vsh main is in os_wait). Hunt 2 caught the same pipeline one step later (wc rc 1, then
vsh's Close hung): both runs point at the wc/subshell-exit window, not at a handler packet.

### Next probes, in order
1. Stack 262144 run of the same suite (memory rule, still not run).
2. wasabi `snoop --task` for wc and the vsh subshell across the freeze window (`--entry` so a call that
   never returns shows), plus vsh TR around subshell exit/job_free: which task makes the last call.
3. A non-ixemul last stage (no coreutils): e.g. a small vbcc reader in tools/rig, to split ixemul from vsh.
4. Fix the latent job_free/pipe_tab use-after-free (Hunt 2) regardless; then retest.
