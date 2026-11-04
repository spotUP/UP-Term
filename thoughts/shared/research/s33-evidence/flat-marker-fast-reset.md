# FLAT-MARKER-FAST-RESET proof attempt (rig 62): INCONCLUSIVE, rig never answered

Date 2026-11-03 22:11-22:28. Question: does a reset within 60 s of amarchy-flat start leave ENVARC:Amarchy/flat-starting so the next boot reads flat=held why=marker?

Code read (amarchy/flat.c): marker_write at install_all time, deleted only by the 60 s timer (`marker_live && pt_fired(&tm)`) or by a start that held for the marker. A reset before 60 s leaves the file, and flat_start_decide(marker_exists()) returns FLAT_HELD_MARKER on next start. Static reading supports the hypothesis; not run.

Rig attempt:
- `UPTERM_RIG=62 rig.py start --max --os 32 --purpose ...` at 22:11: wasabid never answered (connect timeout, SYN_SENT on 127.0.0.1:1295; fs-uae listening, guest not accepting). Fresh os-32 image has no amarchy-flat autostart (only rig 35's full install has the ;BEGIN Amarchy-Flat block).
- Stopped --keep, appended `;BEGIN Amarchy-Flat / Run >NIL: VTC:amarchy-flat BOOT / ;END` to build/rig62/os-32/S/User-Startup (VTC: = VTCX: has amarchy-flat), restarted 22:15:53. boot.log reached "wasabid started"; rig.py logged "boot N s: wasabid started" up to 549 s; wasabid still unreachable. Host load average 32-50 (tsc, Chrome). First boot (before my edit) hung the same way, so the edit is not the cause.
- No STATUS lines, no marker checks, no reboots possible. Verdict: NOT RUN (neither CONFIRMED nor REFUTED).
- Cleanup: rig 62 stopped and cleared.

## Run 2 (s34, rig 66) - PAUSED

Date 2026-11-04. Stopped by coordinator (rig 96 R3 must run alone). Verdict: NOT RUN (neither CONFIRMED nor REFUTED).
- `make wasabi` OK (959c33bd). Rig 66 started `--max --os 32` with purpose; wasabid answered this time (0.11b2) despite host load (tsc 220% CPU), unlike rig 62.
- VTC:amarchy-flat present on the fresh image; no autostart. Appended `;BEGIN Amarchy-Flat / Run >NIL: VTC:amarchy-flat BOOT / ;END Amarchy-Flat` to build/rig66/os-32/S/User-Startup.
- `rig.py reboot` issued to load that startup; it had not returned (still booting) when the stop came. No STATUS reading, no marker check taken.
- Rig 66 stopped and cleared.
- Exact next step: start rig 66 (same command), append the User-Startup block as above, `rig.py reboot`, then at boot T+0 read `ami.py exec "Rx ..."` / amarchy-flat STATUS (flat=on) and `List ENVARC:Amarchy/flat-starting`; `rig.py reboot` at T+30 s after the command is quiet; read STATUS + marker (expect flat=held why=marker if hypothesis holds). Control: reboot after T+90 s (expect flat=on). Repeat the fast case once.

## Run 3 (s34, rig 66) - INCOMPLETE, host load blocked the proof

Date 2026-11-04. Verdict: NOT PROVEN by the protocol (fast case, control, repeat all not run). One incidental observation points to CONFIRMED.
- Host load 50-96 (rig 96 fs-uae 90%, DEViLBOX tsc/node, grep 79%, rig 66 itself 88%). `rig.py reboot` start step raised CalledProcessError (wasabid wait) on 3 of 3 reboots; boots took 2.5-5 min; `ami.py` timed out on the last boot (rig 66 `status` = down for 2+ min). Stopped per the 2-boot rule.
- Kickstart 47.115, Workbench 47.2 (os-32). User-Startup block appended (confirmed present on the rig).
- Reboot 1 (08:18:22, rig.py killed the first boot after start; flat ran < unknown s, certainly < 60 s of Amiga time in the reset): start step failed, rig came up late.
- Reboot 2 (08:23:08, return 08:25:54), read 08:25:59-08:26:21 (T+10..30 s): `flat=held calls=0 chained=0 why=marker`. ENVARC:Amarchy/flat-starting then ABSENT (`List`: object not found; ENVARC:Amarchy empty). Consistent with flat.c line 1489: a held-by-marker start deletes the marker ("one inert start per crash"), so the marker left by the reset in the earlier boot was consumed by this boot. The marker was not read before the deletion, so "present after reset" is inferred, not listed.
- Reboot 3 (08:26:51, return 08:32:34): wasabid never answered (ami.py exec timeout at 08:34), no STATUS.
- Not done: boot reading flat=on at T+0, the T+30 s fast reset with a marker List, control (T+90 s), repeat.
- Note for the next run: read STATUS and List the marker BEFORE the reset, and run when host load is low (the 3 reboots each cost 3-5 min under load 60).
- Rig 66 stopped and cleared.
