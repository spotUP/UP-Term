# Release blocker "install_rig base check 89 Uninstall removed the files" - re-check

Date 2026-11-02, session vtcon-ee, rig 64 (stopped and cleared), kit build/up-term.lha (2 Nov 12:54), wasabi 0c43ce3.

Verdict: STALE. The check passes on this kit.

Decisive line (install_rig --only base, rig 64): `ok 90 Uninstall removed the files`
(the case's numbering has moved: the old 89 is now 90; 89 is "after Uninstall: no TMP: (Install made it)", also ok).
All 15 "after Uninstall" checks 76-92 are ok, incl. `ok 91 ENV:vsh holds no files`.

The base case still FAILS overall (passed 80 of 92), for another reason: 12 checks
`FAIL 51..63 PORTS: <tool> through vsh's $PATH` (sed, awk, less, find, xargs, diff, cmp, patch,
gzip, bzip2, xz, tar - output shows only the tool name, tool not found), and
`[FAIL] base ks sed(1) entry, run makewhatis /System/UP-Term/share/man` (path:UP-Term:bin/sed). Not investigated (outside the question).
Likely: the kit lacks the ports binaries on this build (sed missing in UP-Term:bin) - unverified.

Notes: first `make wasabi` failed once with tls.h missing (race with another session's build), second run OK.
First rig start failed with RIG-WARM-SRV-RACE (build/amiga/Amarchy rebuilt by another session mid-start); retry OK.
Log: scratchpad r3/base.log (small, kept).
