---
date: 2026-10-30
session: vtcon-05 (s32)
status: partial
---

# Session 32 handoff: vsh TERM trap, ixemul programs die under vsh (rc 161)

## Done (on origin/main)
- TEST-PKGS-MKBADDR closed: c240dca0 (pkgs 91/0). IXEMUL-CRT0-CTORS ticked as stale.
- VSH-TRAP-TERM closed: 8ebfc681. Break D runs the TERM trap; untrapped QUIT/TERM end a script with 128+n.
  sh_exec 574/574, fail-before shown. Rig 4 trapsig_rig: `ok break-D -> TERM`.

## Open, in order
1. s32 VSH-EXT-RC161 (todos.md ~line 27). On the rigs, every ixemul program (mkdir, ls, true, echo)
   started by vsh dies inside ixemul startup before main. It returns rc 161 (128+33, via ixemul
   2525442), and before 2525442 it returned rc 0 while doing nothing. Native programs (C:Dir) work.
   The same ixemul programs work from the AmigaDOS shell. Old vsh builds (8ebfc681^, d234dbd9^) fail
   the same way, and the rig libs match the build.
   The only death path is kern_sig.c:1061 sendsig(p, sig_exit, sig). The value 33 is likely sig_exit
   reading the wrong argument slot.
   Suspect: a break bit already pending in vsh's runner process (CreateNewProc + RunCommand, vsh.c ~700).
   Probes, cheapest first:
   a. Read the runner's tc_SigRecvd before RunCommand, or clear the break bits as a probe build.
   b. Reproduce on a second rig. Check whether the tutorial ever passed L-R (session 2 rig 4 report).
   c. Build ixemul with DEBUG_VERSION in a scratch worktree and watch it with wasabi debug.
   Fix the root cause, plus sig_exit's argument so the rc names the real signal. Add a regression test.
   Evidence: thoughts/shared/research/s32/rc161.md, rc161-root.md.
2. s32 VSH-KILL-TERM-NATIVE-HANG (todos.md ~line 14). `ixkill -0 <live native task>` hangs in ixemul
   kill()/pfind() (kern_sig.c ~440). pfind never searches TaskReady: the loop ends on the tail node,
   not on NULL. It then dereferences tc_TrapData of tasks that are not ixemul tasks. kill(self) must
   keep working, and ThisTask is in neither list.
   The exact hang point is unproven, so get a PC or a KPRINTF trace first. Do this in the same ixemul
   rebuild as item 1. Evidence: research/s32/kill-term-hang.md.
3. SHELL-TUTORIAL L-C1 rig half + L-R. Rerun `UPTERM_RIG=n python3 tools/rig/tutorial_rig.py` after
   item 1 is fixed (4/27 passed, all failures follow from rc 161). Evidence: research/s32/tutorial-rig.md.
4. RERUN-AFTER-IPREFS stays blocked until ../upterm-ports/build/kit/userland/SOURCES.txt exists.

## Gotchas
- After `git apply`, make did not rebuild build/amiga/vsh. Run `touch` on the changed source first.
- Disk is about 11 GB free, under the 20 GB floor. Run df before any ixemul build.
