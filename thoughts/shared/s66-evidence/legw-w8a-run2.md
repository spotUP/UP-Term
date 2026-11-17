# W8a run 2 (s66): legw on KS 3.2, real ports kit
Verdict: FAIL (install aborts; (a)-(e) not reached). Rig 80 (`UPTERM_RIG=80 rig.py start --os 32 --max`, purpose "up-term-bb s66: W8a KS 3.2 legw install"), `UPTERM_RIG=80 install_rig.py --only legw`. Kit: 173 programs.

Steps:
- 1 base Install runs: FAIL. Output: "vsh: rm: not found (it lives in UP-Term:bin: put that drawer on PATH, or run the UP-Term Install)", "vsh: grep: not found", "vsh: rm: not found", "vsh failed returncode 127".
- 2 UP-Term.version written: ok (Install got to the end of the file copies).
- 3-6 (a) Startup-Sequence block: FAIL (block absent; S:Startup-Sequence is the untouched 2-line boot one).
- 7-10 (b) WBStartup icons: FAIL (absent).
- Then reboot check: [ABORT] "Amiga did not come back within 240 s", no verdict written (a consequence of the missing block/icons).
- (c)-(e), Uninstall: not reached.

Key evidence (live rig 80 after the failure):
- `C:vsh -c 'echo PATH=[$PATH]'` prints `PATH=[]`.
- UP-Term:bin/grep (224492) and UP-Term:bin/rm (140036) EXIST. So the real-kit grep fix worked; the failure is no longer a missing grep.

Cause: dist/instssblock.sh:19 `rm -f ...` and :20 `grep -q ...` run in a bare `vsh instssblock.sh ...` started from the Install shell (dist/install.dos:607), where vsh's $PATH is empty (the default PATH comes from dist/vshrc `path reset`, which a script-mode vsh does not source; shell/sh_exec.c:6182 VSH_DEFAULT_PATH only applies via `path reset`). The reported "~569" call (`vsh unstartup.sh`, install.dos:573/579/588) needs no rm/grep (dist/unstartup.sh uses only builtins), so the failing script is instssblock.sh at install.dos:607, not unstartup.sh. Unverified: why KS 3.1 passes (maybe Install shell/ENV there sets PATH); not checked.

Proposed fix (root cause, in the script that needs the tools): make instssblock.sh independent of PATH, or give script-mode vsh its default PATH when $PATH is empty (shell/sh_exec.c startup: sh_get(PATH)=="" -> VSH_DEFAULT_PATH, which is also what bash does for an unset PATH). The vsh default is the better layer: every `vsh script.sh` from install.dos (reassign.sh, takecon.sh, unstartup.sh, instssblock.sh) gets it. Also check the same for the 3.1 difference.
Logs: scratchpad legw80.log, start80.log.
