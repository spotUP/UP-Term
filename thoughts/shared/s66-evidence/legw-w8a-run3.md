# W8a run 3 (s66): legw on KS 3.2 after the vsh PATH fix
Verdict: PATH fix WORKS; install still aborts one step later (new failure below).

Fix: shell/sh_exec.c `sh_startup_path()` (sets and exports VSH_DEFAULT_PATH when PATH is unset or empty; the define is the single existing one, moved above path_has), declared in shell/sh_exec.h, called in shell/vsh.c after ENV:vsh/vshrc and ~/.vshrc and before sh_startup_env, so a parent's or a vshrc's PATH wins.
Test: tests/test_sh_exec.c `a_shell_with_no_PATH_after_startup_gets_the_default_and_a_set_PATH_stays` (unset, empty, set). `make test ONLY=sh_exec`: 712 checks, 0 failed. Revert check (condition disabled): 3 failures; restored, 0 failed.
Build: `make dist-base UPTERM_PORTS=.../kit-main` rc 0 (build/up-term.lha).

Rig 86 (`rig.py start --os 32 --max`, purpose "up-term-bb s66: W8a vsh PATH fix verify"), `UPTERM_RIG=86 install_rig.py --only legw`:
- Step 1 base Install: instssblock.sh no longer says "vsh: rm: not found" / "grep: not found" (rm resolved via PATH). NEW FAIL: [ABORT] `Execute VTC:longwrap is blocked by the requester "startup message" (task rm, Workbench Screen, button Abort)`; output reached the file copies (amarnotify ... dopus-back.rexx), no verdict written.
- Steps (a)-(e), Uninstall: not reached.

New failure, cause: rm and grep are ixemul programs. The "startup message" requester is ixemul.library failing to open (known, thoughts/shared/research/2026-10-17_a1200-kit-tests.md Run 2). On a stock KS 3.2 rig LIBS: has no ixemul.library, and dist/install.dos copies it only at :1120 (`Copy $upixlib LIBS:ixemul.library`), after the instssblock.sh call at :607.
Proposed fix: move the ixemul.library/ixnet.library copy block (install.dos:1086-1143) ahead of the Amarchy-Flat insertion at :595-607 (or move that insertion after :1143). Not applied (budget).
Cleanup: rig 86 stopped, build/rig86 removed; scratch logs under 10 MB.
