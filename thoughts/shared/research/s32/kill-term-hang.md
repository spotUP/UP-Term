# kill -s TERM hang (rig 4, 2026-10-30)
Block point: INSIDE ixkill (shell/ixkill.c:57 -> ixemul.library kill() -> pfind(), ixemul-vtcon/library/kern_sig.c:440/491), not vsh's SystemTags. vsh's os_signal/task_unix_signal wait synchronously for it.
Evidence:
- `VTC:ixkill -TERM <live native vsh addr>` direct from the rig: still running after 40 s, ignores Ctrl-C; wasabi ps: state "wait", no DOS calls in snoop (--task 'ixkill*').
- signal 0 hangs too (`ixkill -0 272630024` on a native `Wait`, `-0` on AUDIO process), so it is independent of signal number and of vsh: kill()/pfind on ANY live native task. Only _psignal is skipped for sig 0, so the wait is before it, in pfind (reads getuser(t)=tc_TrapData of a non-ixemul task, then ->u_ixbase) or the kill() prologue.
- Dead/nonexistent pids return rc 1 at once (ixkill -TERM 12345, 0x1, dead vsh addr); ixkill ? fine. INT on a dead addr rc 0 via the SIGINT fallback.
- ixemul.library 80.1 on the rig. No Guru, no requester (wasabi guru, windows).
Not found: the exact instruction (no PC dump tool); pfind deref of a native task's tc_TrapData is the prime suspect (garbage pointer -> trap -> ixemul trap handler waiting).
Proposed root fix: ixemul pfind() must not trust tc_TrapData of an arbitrary task (check it points at a user struct with the magic cookie / ixemul registered-process list, as the comment says "checks magic cookie"); then ixkill returns ESRCH and vsh's existing HUP/TERM fallback to Ctrl-C runs. Verify: rebuild ixemul-vtcon, ixkill -0 <Wait> returns 1 fast.
Interim vsh-side safety (secondary): task_unix_signal should not block on a non-ixemul target.
