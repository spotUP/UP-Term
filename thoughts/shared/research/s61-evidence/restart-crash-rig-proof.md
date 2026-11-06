---
date: 2026-11-06
topic: rig proof of the _chkabort restart-crash fix (two legs)
build: parent of 9b3e6599 link state (AMARCHY_NOBREAK= empty) vs 9b3e6599
rig: 176, AmigaOS 3.2, 060 --max, purpose "vtcon-c1: Amarchy restart crash proof"
status: LEG U crash seen; LEG F FAIL (fix does not stop the restart crash)
---

# Rig proof, Break + restart of Amarchy

Loop script: scratchpad restart-proof/loop.py (Break every Process whose command is a SETUP_NAMES program, wait up to 10 s for no Amarchy process, GetEnv AMARCHY_STRUT_INSTALLED, count Exception/CPU reset lines in the rig's fs-uae.log, then Run >NIL: VTCX:Apps/bin/Amarchy and wait 20 s for port AMARCHY). Pause schedule 0.2 0.5 1 2 3 s after the port answers.

## Commands
- Leg U build: rm the three binaries; make build/amiga/Amarchy build/amiga/amarchy-gui build/amiga/amarchy-flat AMARCHY_NOBREAK= ; make build/amiga/amarchy-menu AMARCHY_NOBREAK=
  tests/test_nobreak.py: [FAIL] x3 (offsets 0x371C, 0x28E4, 0xF444), rc 1.
- Leg F build: rm the three; make chkabort-check: [OK] x3 (no library _chkabort), rc 0.
- Both: UPTERM_RIG=176 rig.py start --max --os 32 (once); amarchy_gui_rig.py --setup --no-build (its last check "flat prefs off" prints FAIL, unrelated flat-prefs state; Amarchy started and the port answered); rig.py reboot; loop.py.

## Leg U (unfixed): CAUSE SEEN as a crash
| cycle | pause | process gone | AMARCHY_STRUT_INSTALLED | new exc lines | next start |
|---|---|---|---|---|---|
| 1 | 0.2 | yes | 0 | 0 | port up |
| 2 | 0.5 | yes | 0 | 0 | port up |
| 3 | 1 | yes | 0 | 0 | port up |
| 4 | 2 | not read: the call hung | - | Exception 3 at 102880ca -> f80bc2!, CPU reset | guest reset, watchdog restarted it |
(Run 1 of the loop, partial log loopU-partial.log. Crash line from fs-uae.log: "Exception 3 (102880ca) at 102880ca -> f80bc2!" then "CPU reset PC=f80edc".) A second loop run on the watchdog-restarted rig got only "connection refused" (rig still booting); it measured nothing and is not counted. The skipped-cleanup reading (process gone, variable 1) was NOT seen: cycles 1-3 cleaned up (0). The crash itself is the positive result. Which task faulted, and whether the cleanup was skipped in cycle 4: UNVERIFIED.

## Leg F (fixed 9b3e6599): FAIL
Fixed binaries deployed (sizes on the rig equal build/amiga: 367256, 534836, 209560; md5 not compared, UNVERIFIED), rig rebooted after setup.
| cycle | pause | process gone | variable | new exc lines (cumulative) | next start |
|---|---|---|---|---|---|
| 1 | 0.2 | yes | 0 | 0 | port NOT up in 20 s |
| 2 | 0.5 | yes | unset (port was not up before Break) | 3 | up |
| 3 | 1 | yes | 0 | 3 | NOT up |
| 4 | 2 | yes | unset | 6 | up |
| 5 | 3 | yes | 0 | 6 | up |
| 6 | 0.2 | yes | 0 | 6 | up |
| 7 | 0.5 | yes | 0 | 6 | NOT up |
| 8 | 1 | yes | unset | 9 | up |
| 9 | 2 | yes | 0 | 9 | NOT up |
| 10 | 3 | yes | unset | 12 | up |
| 11 | 0.2 | yes | 0 | 12 | NOT up |
| 12 | 0.5 | yes | unset | 15 | up |
Passed 2 of 12 (cycles 5, 6). Five CPU address errors (Exception 3) in 14 cycles, PCs 1025eb46, 1025a23e, 102891be, 1025f266, 1025c37e (all in the Amarchy segment range of hunt 2). Pattern: a start whose port does not answer in 20 s is followed by a crash within the next cycle. The variable is always 0 or unset, never 1: the cleanup did run after the Break, so the crash here is NOT the skipped-cleanup path of the _chkabort cause.
Extra cycles 13-14 (Break right after Run): INVALID. The script ran Run while the previous Amarchy was still up (duplicate start exits), so the Break did not hit a starting instance; cycle 14 ended in the sixth crash (connection refused). Not a startup-Break measurement.

## Verdict
- Leg U: crash seen in cycle 4 of 4 run.
- Leg F: FAIL. The empty _chkabort does not remove the restart crash on this rig: 5 of 12 Break+restart cycles crashed, with the cleanup having run each time. Another cause is still open (the restart crashes without a skipped cleanup).
- UNVERIFIED: that Exception count 15/17 equals crashes only (3 log lines each: Exception, two CPU reset); fs-uae.log was capped/rotated between legs, so only leg F lines are counted.
