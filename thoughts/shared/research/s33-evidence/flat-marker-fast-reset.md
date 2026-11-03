# FLAT-MARKER-FAST-RESET proof attempt (rig 62): INCONCLUSIVE, rig never answered

Date 2026-11-03 22:11-22:28. Question: does a reset within 60 s of amarchy-flat start leave ENVARC:Amarchy/flat-starting so the next boot reads flat=held why=marker?

Code read (amarchy/flat.c): marker_write at install_all time, deleted only by the 60 s timer (`marker_live && pt_fired(&tm)`) or by a start that held for the marker. A reset before 60 s leaves the file, and flat_start_decide(marker_exists()) returns FLAT_HELD_MARKER on next start. Static reading supports the hypothesis; not run.

Rig attempt:
- `UPTERM_RIG=62 rig.py start --max --os 32 --purpose ...` at 22:11: wasabid never answered (connect timeout, SYN_SENT on 127.0.0.1:1295; fs-uae listening, guest not accepting). Fresh os-32 image has no amarchy-flat autostart (only rig 35's full install has the ;BEGIN Amarchy-Flat block).
- Stopped --keep, appended `;BEGIN Amarchy-Flat / Run >NIL: VTC:amarchy-flat BOOT / ;END` to build/rig62/os-32/S/User-Startup (VTC: = VTCX: has amarchy-flat), restarted 22:15:53. boot.log reached "wasabid started"; rig.py logged "boot N s: wasabid started" up to 549 s; wasabid still unreachable. Host load average 32-50 (tsc, Chrome). First boot (before my edit) hung the same way, so the edit is not the cause.
- No STATUS lines, no marker checks, no reboots possible. Verdict: NOT RUN (neither CONFIRMED nor REFUTED).
- Cleanup: rig 62 stopped and cleared.
