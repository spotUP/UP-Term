# s33 VSH-BG-KILL-EARLY: `script & kill %1` with no delay

Branch fix/bg-kill-early (worktree /Users/spot/Code/UP-Term/vtcon-bgkill), commit 6c7cb8a1. Not pushed.

Cause: job_signal signals the runner and the SystemTags child (job_child). Right after `&` the child does not exist, the runner is blocked in SystemTags, the break is lost.

Fix: job.pend_sig keeps the undelivered break (job_signal, when has_child and no child found). job_flush_sig sends it to the child once job_child finds it, clears it, and clears it when the job is done. It is called from job_gone, which the wait loop (os_wait, 1 s wait_or_tick) and os_done (background check, wait builtin) already call under Forbid: no new timer or process. Decision helper sh_flush_pending(pend, child_found) in shell/sh_jobgone.h, host check in tests/test_sh_jobgone.c (27 checks, 0 failed).

Not covered: job_process (kill -s TERM through ixkill) returns 0 while the child does not exist yet; ixkill is a separate process sending a Unix signal to a task, nothing to keep on the job, so `script & kill -s TERM %1` with no delay still loses the signal. Left open (own row if wanted).

Rig: rc161_rig.py case script_kill_early (`script & kill %1; wait; echo ended`, 8 s). Rig 4. The rig's ps check needs WASABI=/Users/spot/Code/UP-Term/vtcon/build/wasabi/wasabi in a worktree (build/wasabi is not seeded; without it the "systemtags child present" line fails on an empty ps).
Old vsh (main, md5 7670f513...): `[FAIL] script_kill_early: 'hung (no end after 8 s)'`
New vsh (md5 5e63ffa8...): `[OK] script_kill_early: 'ended\n'`
