# s33 VSH-KILL-TERM-NATIVE-HANG hunt (rig 3, main be8228c1)

Date 2026-10-30. Rig 3 only. No source edits. Status: hang reproduced and located to "ixkill is in Wait while a
priority-1 busy-loop runs"; the exact signal ixkill waits on is NOT proven (rig 3 was stopped by its idle
watchdog before the control run, see "Open").

## 1. The new ixemul is loaded (confirmed)
- rig 3 LIBS:ixemul.library md5 a8a7bf4c4df7f53395fc320500fd25ed = ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library
  ("ixemul.library 80.1"); LIBS:ixnet.library md5 9247e6d4... = build295 ixnet.
- `UPTERM_RIG=3 python3 -I tools/rig/pfind_rig.py` -> `pfind_rig: OK` (needed `make build/amiga/ixpfind` first).
So the s32 pfind theory is closed: the hang is not pfind.

## 2. Repro and where it blocks
Repro: scratchpad kt3.py (same as kt.py plus ps samples and an unfiltered `snoop --entry`). Target script
`echo $$ >RAM:kt.pid; while [ $SECONDS -lt 150 ]; do [ -e RAM:kt.stop ] && exit 0; done`, started with
`Run >NIL: UP-Term:bin/vsh RAM:kt.sh` through wasabid; sender `UP-Term:bin/vsh -c "kill -s TERM 269629752"`.
Sender did not return within 90 s (U.run timeout).

`wasabi ps` while hung (decisive lines):
```
ADDR       KIND  PRI STATE    STACK    FREE  CLI  NAME
0x1089e808 proc    1 run      16384   14339    9  Background CLI (BOOTX:wasabid)
0x10123938 proc    1 ready   262144  256694   10  Background CLI (UP-Term:bin/vsh)    <- target, busy loop, PRI 1
0x1010d7d8 proc    1 wait    262144  259370    6  Background CLI (UP-Term:bin/vsh)    <- sender, inside SystemTags
0x10435ec0 proc    1 wait     16384   15568   12  Background CLI (c:ixkill)           <- ixkill: WAITING, not ready
SLOT STATE       TIME  OUTPUT   COMMAND
1    running      62s  -        UP-Term:bin/vsh -c "kill -s TERM 269629752"
```
Pri-0 tasks at the same time (from the pre-kill ps; same set): ramlib 0 wait, DO_LAUNCHER 0 ready,
dopus_clock 0 ready, Amarchy 0 ready, Workbench helpers 0.

- Snoop (60 s, no task filter, 13955 lines): 13948 lines are `UP-Term:bin/vsh` (the target's
  `Lock("RAM:kt.stop")` / `FindTask` / `OpenLibrary("dos.library")` loop every ~12 ms), 5 `Wait`, 0 lines from
  `c:ixkill`. ixkill made no DOS call in the window: it blocks before (startup) or outside DOS.
- When the target's loop ended by itself, the hang released: sender, ixkill and target all gone, and
  `C:ixkill -0 0x10123938` then returned at once (0 s).
- s32 pfind_rig ran `ixkill -0` against 34 idle tasks with no hang: none of them was a busy pri-1 task.

## 3. Cause (strong evidence, last link unverified)
The target is a native busy loop that never Waits and runs at PRIORITY 1: it inherits pri 1 from wasabid's
command CLI (wasabid and every CLI it starts run at pri 1). Exec never schedules a pri-0 task while a pri-1 task
is ready, so every pri-0 process (ramlib, DO_LAUNCHER, Workbench, dopus...) is starved for as long as the loop
runs. ixkill (pri 1) sits in Wait (not ready) and wakes only after the loop ends, so it waits on a reply from a
starved lower-priority task. Most likely ramlib (pri 0): ixkill's OpenLibrary chain (ixemul startup opening a
disk library / device) needs ramlib to LoadSeg it. NOT proven which task: no stack/wait-mask tool in wasabi,
and the control run did not happen (rig 3 stopped by its idle watchdog, watchdog.log "idle 60 min").

Why the sender ignores Ctrl-C: vsh shell/vsh.c:284 task_unix_signal runs ixkill with a synchronous
`SystemTags(cmd, SYS_Input, nil, SYS_Output, nil, SYS_UserShell, TRUE)` (vsh.c:308). A synchronous SystemTags
returns only when the child ends; the sender never looks at its break bits meanwhile, and the child's input is
NIL:, so Ctrl-C to the sender reaches nobody.

## 4. Violated invariants and layers
1. vsh (shell/vsh.c task_unix_signal / job_unix_signal): "a builtin's helper process never makes the shell
   uninterruptible" - kill blocks without bound on any stall of ixkill and ignores Ctrl-C. This is the vsh bug
   and the fix the ledger asks for.
2. Test environment (tools/rig + wasabid): the repro target runs at pri 1 because wasabid's command CLIs run
   at pri 1. A real user's busy script at pri 0 round-robins with ramlib and does not starve it. So the
   ">90 s" is a pri-1 busy-loop artifact of the repro; on a real Shell (pri 0) the same kill should finish,
   slowly. (Unverified, see Open.)
Not ixemul: pfind fixed and proven, _psignal (kern_sig.c:634-739) has no wait; ixkill made no DOS call.

## 5. Fix design (not implemented)
vsh, task_unix_signal (and job_unix_signal, which calls it):
- Do not add a new spawn mechanism (NP_ExitData is out: rc161 note). Reuse vsh's existing runner/job path,
  which already runs a SystemTags child in a runner process and handles breaks to it (job_child /
  job_signal / job_flush_sig, vsh.c:155-230). Run ixkill as a hidden job through that path and wait with
  `Wait(job_done | SIGBREAKF_CTRL_C)` in the shell.
- On Ctrl-C: send CTRL_C to the ixkill child, stop waiting, return -1 with `signal_why = "kill interrupted"`;
  os_signal then applies its existing HUP/TERM fallback (Ctrl-C to the target) so TERM still lands.
- Bound the wait (e.g. 5 s via timer.device, the clock vsh already uses for SECONDS): on timeout treat as
  "not sent" and fall back the same way. 
- Keep the result rule: rc 0 = sent, else fallback.
Test layer: kt-style rig targets must not busy-loop at pri 1: start them with `ChangeTaskPri 0` first or
loop on `sleep 1`.

## 6. Regression test (through the real entry)
New rig MUST in tools/rig/trapsig_rig.py (it already runs the kill TERM case last):
- A: target `vsh RAM:kt.sh` with a pri-1 busy loop (as today), sender `vsh -c "kill -s TERM <pid>"`, then
  `wasabi key ctrl-c`/break CTRL_C to the sender after 3 s: MUST return within 5 s (before fix: >90 s, ignores it).
- B: same with `ChangeTaskPri 0` in the target and no break: sender MUST return within 10 s and the target
  MUST end with st 143 (TERM -> its trap / CTRL_C fallback).
- Sentinel: sender's stderr/`signal_why` text for A proves the new wait path ran.
Fails before (A hangs), passes after; revert the vsh change and A hangs again.

## Open (next run, ~5 tool calls on a fresh rig)
- Control: kt4.py `pri0` (target with `ChangeTaskPri 0`): if kill returns at once, the starvation cause is proven.
- Which task ixkill waits on: during the pri-1 hang, `wasabi snoop --entry --task 'c:ixkill'` started BEFORE the
  sender, plus `ps` of ramlib (wait vs ready + its pending work). If the pri-0 run still hangs, the cause is
  elsewhere and this note's section 3 is wrong.

Scratch (small, kept): scratchpad kt3.py, kt4.py, kt3.log, kt-snoop.log (~1.5 MB), pfind3.log. Rig 3 stopped
by its watchdog (not by me) after the hang released; nothing was writing.

## Result s33

Step 1 verdict: priority-1 starvation is the trigger. Evidence: a first pri0 control was invalid (ChangeTaskPri inside the vsh script ran in a separate runner, ps showed the target still at pri 1, kill still hung). Valid control: target started through `Run >NIL: Execute RAM:kt.run` (ChangeTaskPri 0, then vsh script), ps showed it at pri 0, `vsh -c "kill -s TERM <pid>"` returned in 0.2 s rc 0. The exact task ixkill waits on was not snooped (probably ramlib).

Fix (vsh layer): task_unix_signal ran ixkill with a synchronous SystemTags. It now runs ixkill as a hidden job and waits with the pure helper sh_killwait (shell/sh_jobgone.h): reply, Ctrl-C or 5 s limit. On Ctrl-C or timeout it returns -1 (not sent), so os_signal's TERM/HUP fallback sends Ctrl-C to the target. Reused: job runner path (runner, job_dup, job_free, job_close_streams, command_stack), wait_or_tick, replied, os_now, orphan accounting (orphans, orphan_jobs).

Rig checks A and B: `UPTERM_RIG=n python3 tools/rig/trapsig_rig.py kill`.
- Old vsh (main build): A FAIL (sender 96 s, deaf to Ctrl-C); B ok on time (0.4 s), status 130.
- New vsh: A PASS (sender back at 3.0 s, right after the Ctrl-C); B PASS (0.2 s, status 130).
Host: `make test ONLY=sh_jobgone` 33 checks, 0 failed.

Open:
- B expects status 130, not the brief's 143: TERM reaches a native vsh as the Ctrl-C fallback.
- An abandoned ixkill is left running and frees itself when it ends; orphan_jobs keeps the program code loaded for it.
- The 5 s limit and the Ctrl-C re-raise (as os_wait does) are untested beyond check A.
- Other rigs with pri-1 busy-loop targets were not searched.
- The root starvation (wasabid at pri 1 starves pri-0 tasks ixkill needs) is untouched.
