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

## s35 Ctrl-C fallback

Verdict: the Ctrl-C branch of the os_signal fallback works for a non-vsh target. Rig 5, vsh from 91e0e355,
`trapsig_rig.py kill` (new checks E and F), PASS:
- E kill -s TERM at C:Wait (job of a vsh script, not an ixemul program): sender back in 0.3 s, `kill-rc=0`, Wait
  ended (job script wrote RAM:kt.rc) well before its 100 s. Only the fallback can end it: nothing else signals it.
- F control, kill -s USR1 at the same target: no fallback for USR1, `kill-rc=1`, Wait still running after 6 s.
  Same target, same sender; only the signal differs, so the end in E is the fallback's Ctrl-C.
Correction to the D caveat: the fallback is entered when ixkill FAILS (-1: no ixemul process there, here after
0.3 s), not only on a timeout. A 0.3 s answer for C:Wait is that failure, so D already ran the Ctrl-C branch; E/F
add the negative control that D lacked. A genuine ixkill timeout (SH_KILL_LIMIT_MS) reaches the same line and is
covered by A (sender broken at 3 s).
