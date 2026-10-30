# s34: kill -s TERM fallback sends Ctrl-D to a vsh (branch fix/kill-term-ctrld)

Invariant: TERM must reach a vsh as TERM (trap runs, untrapped exit 143). The ixkill-timeout fallback in
os_signal (shell/vsh.c) sent Ctrl-C to every target, so a vsh saw INT (status 130, no TERM trap).
Level: os_signal fallback (where the bit is chosen); vsh's os_interrupted already maps Ctrl-D to TERM (8ebfc681).

Detection (searched: "vsh job|vsh subshell|SIGBREAKF_CTRL_D|ln_Name|cli_CommandName|SetProgramName|tc_UserData|
FindSemaphore" in shell/*.c,*.h): no marker vsh sets on itself exists; "vsh job <n>" is also the runner of a
NATIVE command, so the name cannot decide. Used: task_is_vsh(t) = process with a CLI whose cli_CommandName last
path component equals this process's own (fallback "vsh"), case-insensitive. job_is_vsh(j) = job_child(j) (the
SystemTags Shell process of a script) is a vsh, or the job task is named "vsh subshell <n>". A job aimed at a vsh
gets Ctrl-D on the child (or subshell task) only. Everything else keeps Ctrl-C via job_signal. Limit: a vsh
copy renamed differently from the running one is not recognised (gets Ctrl-C as before).
HUP (sig 1) stays Ctrl-C: vsh polls no break bit as HUP (D is TERM), so D would run the wrong trap.

Rig 5, `trapsig_rig.py kill` (new checks C, C2, D; B now wants 143):
- old vsh (main, 42f33499): B FAIL status 130; C FAIL trap not run, status 130; C2 FAIL 130 (want 143); A ok; D ok
- new vsh: A ok; B ok status 143; C ok trap log [TERM]; C2 ok status 143; D ok (C:Wait stops); PASS
D caveat: ixkill answered in 0.3 s there, so D does not prove the Ctrl-C fallback for a non-vsh; the code path for
non-vsh is unchanged Ctrl-C. Host tests: none cover os_signal (Amiga-only layer), none run.
