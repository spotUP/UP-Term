# Check 26 (SYS:UP-Term as it was before Install) on a fresh 3.2 rig

Verdict: check 26 PASSES. Main commit tested: a51c35bc (includes 4c0200d7 + 6c405484).

Rig: 47 (`UPTERM_RIG=47 rig.py start --max --os 32 --purpose "vtcon-16 Amarchy session: Uninstall keeps old drawer check 26"`).
Stopped and cleared: `rig.py stop` printed "stopped", `rig.py clear` printed "removed build/rig47"; afterwards build/rig47 does not exist and no process mentions rig47.

Kit: `make dist` fails on this host ("upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports"; sibling repo, not run). build/dist/up-term was the stale 11:53 kit without DirWasThere, so a private copy was made and Install, Files/install.dos and Uninstall were re-assembled from the committed templates with tools/mkinstaller.py + mkkit.templates('base') (grep: DirWasThere x3 in install.dos, x2 in Uninstall). Run with UPTERM_KIT pointing at it. The copy (66 MB, scratchpad) is deleted.

Command:
UPTERM_RIG=47 UPTERM_KIT=<scratchpad>/kit/up-term nice -n 19 python3 -I tools/rig/install_rig.py --only wasabi --force

Check 26 line verbatim:
ok 26 after base Uninstall: SYS:UP-Term as it was before Install

Summary: "install_rig wasabi: passed 29 of 30". Checks 1-22 (base Install, WASABI install, Uninstall run) ok; 24, 25, 27-30 ok.
The one failure, unrelated to the drawer fix:
FAIL 23 after base Uninstall: S:User-Startup as it was, byte for byte: first difference at line 15: now ';BEGIN AmiSSL', before ''
(the AmiSSL block appears in S:User-Startup after Uninstall but not before; not checked whether Install, Uninstall or the rig image writes it)
No separate reboot step exists in the wasabi case (verdict file: this run only).
