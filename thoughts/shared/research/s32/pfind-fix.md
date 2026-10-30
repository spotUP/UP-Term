# pfind fix (ixemul-vtcon library/kern_sig.c), s33

Branches (not pushed, not merged): ixemul-vtcon `fix/pfind-tasklist` (worktree /Users/spot/Code/UP-Term/ixemul-pfind) commit f372476;
vtcon `fix/pfind-rig` (worktree /Users/spot/Code/UP-Term/vtcon-pfind) commits 4d4df9ff + follow-up (pfind_rig.py, tests/amiga/ixpfind.c, Makefile rule).

## Defect
Both exec list walks end ON the tail node (ln_Succ NULL), never execnode == NULL, so the NULL checks never fired: TaskReady
never searched, every address "found", kill() then read tc_TrapData of an arbitrary address. Fix: found flag; TaskWait, TaskReady,
then SysBase->ThisTask (in neither list; kill(self) stays 0). Not found -> 0 -> ESRCH, so kill()'s SIGINT fallback still runs for native tasks.

## Hang point: NOT proven
On rig 4 (060, old lib) `ixkill -0 <addr>` for all 34 live tasks (native handlers, devices, Workbench, a `Wait` process, ixemul programs)
returned rc 1 or 0 at once; no hang reproduced. The earlier hang (s32 kill-term-hang.md) did not recur with this state, so the
tc_TrapData-deref theory stays unproven. The proven defect is the list walk (A/B below).

## Other loops
Grepped ln_Succ/lh_Head over the library, ixnet, net, stdlib, glue, utils: killpg/group_in_tree walk the ixemul process tree (no exec lists).
__vtcon.c task_exists uses the same for-loop but returns 0 after the loops (no NULL test): correct. tracecntl.c and _main.c use ln_Succ as
the loop condition: correct. Only pfind had the bug. (.claude/worktrees copies are other sessions' trees, untouched.)

## Regression test
`UPTERM_RIG=4 IXEMUL=<lib> IXNET=<lib> python3 -I tools/rig/pfind_rig.py` (rig booted with that lib; `make build/amiga/ixkill build/amiga/ixpfind`).
Host test not possible (pfind reads SysBase). Checks: self kill(getpid(),0)=0; live ixemul process via ixkill -0 = 0; ixkill -0 on every
other exec task (non-"Background CLI" must be rc 1, within 5 s); `ixpfind fake`: kill() of a Task-shaped block in no list, carrying our
user struct, must be ESRCH.
- old lib: `fake task in no exec list: ... rc 1 (want 0) FAIL fake: kill rc 0 errno 0 (ESRCH 3)`, pfind_rig: FAIL
- new lib: all 36 checks ok, `pfind_rig: OK`
(No hang on the old lib: the test cannot show a timeout failure; the fake block shows the wrong "found".)
Logs: scratchpad pfind-old-final.log, pfind-new.log.

## Notes
Rig 4 was booted with explicit IXEMUL/IXNET (old = main's build295 copy); DH0:Libs never touched, so nothing to restore. Rig 4 stopped.
Built libs: ixemul-pfind/build295 (20 MB, kept for the merge session). vsh -c "ixsigexit sleep &" gives rc 161 (known rc161 issue), so the peer test uses Run.
