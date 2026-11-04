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

## Run 4 (s54, rig 66) - INCOMPLETE, rig.py reboot never brings wasabid back

Date 2026-11-04 08:36-09:10. Verdict: NOT PROVEN (steps 1-5 not run; no pre-reset marker List taken). One incidental observation again points to CONFIRMED.
- Fresh `rig.py start --max --os 32` (host load 25-99): wasabid answered. User-Startup block appended on the host (build/rig66/os-32/S/User-Startup lines 15-17); `VTC:amarchy-flat` present.
- Reboot 1 (`rig.py reboot` 08:41:35, 5:49 total): rc=1, CalledProcessError from the `start --keep-sys` step; start printed `auth failed: the Amiga took the connection but its daemon did not answer for 150 s - frozen, or stuck on something`, then `stopped`. The guest stayed unreachable (ami.py exec timeout) although fs-uae kept running.
- Stop --keep and a fresh `start --max --os 32 --keep-sys` (08:51:45, up 08:54:30): wasabid answered. First reads 08:54:58-08:55:05: `flat=held calls=0 chained=0 why=marker`; `List ENVARC:Amarchy/flat-starting` -> object not found; ENVARC:Amarchy empty. The marker came from the boot killed by the reboot-1 failure (flat had run < 60 s, reset killed it) and was consumed by this boot (flat.c:1489). Second independent sighting of the held-by-marker read (Run 3 had one too), still without a pre-reset List.
- Reboot 2 (`rig.py reboot` 08:55:48, rc=1 at 09:02:01): same CalledProcessError and same `auth failed ... 150 s` text; no STATUS in the next 420 s (to 09:09:07). Host load average 99/80/78 at 09:05.
- First exception, full message: `subprocess.CalledProcessError: Command '[python3.14, tools/rig/rig.py, 'start', '--keep-sys', '--max', '--os', '32', '--purpose', '...']' returned non-zero exit status 1` raised at rig.py:1935 (`reboot`), after the start step's `auth failed: the Amiga took the connection but its daemon did not answer for 150 s`. Before it, a thread `_watch` in ami.py:237 (`_look` -> requesters -> wasabi read_ui `_fill`) died with `TimeoutError: timed out`. Pattern: every `rig.py reboot` (--keep-sys start on a dirty system disk) loses wasabid; every fresh `start` after `stop --keep` works. Suspect: reboot does not stop fs-uae cleanly before the start, or the dirty DH0 (reset mid-boot) delays wasabid past 150 s; not isolated.
- Driver script used: scratchpad drive.py (STATUS poll, T0+25 s List, reset, STATUS first read, control at T0+90 s); never got past the first boot.
- Rig 66 stopped and cleared.

## Run 5 (s54, rig 66) - NOT PROVEN, `start --keep-sys` after `stop --keep` loses wasabid

Date 2026-11-04 09:11-09:28. Verdict: NOT PROVEN (no STATUS line, no marker List taken; neither CONFIRMED nor REFUTED).
- 09:11-09:14 fresh `rig.py start --max --os 32 --purpose "Amarchy session: flat marker proof run 5"`: rc=0, wasabid up.
- 09:14:51 `stop --keep` (5 s quiet first), appended `;BEGIN Amarchy-Flat / Run >NIL: VTC:amarchy-flat BOOT / ;END Amarchy-Flat` to build/rig66/os-32/S/User-Startup, `start --keep-sys --max --os 32` (the planned way to avoid `rig.py reboot`). Output: `up`, `[WARN] S:Shell-Startup not read: auth failed: the Amiga took the connection but its daemon did not answer for 150 s - frozen, or stuck on something`, `[REFRESH] rig 66: 2 file(s): pty-handler, vtcon-handler`, `auth failed ... 150 s`; `rig.py status` = down at 09:21.
- Driver (scratchpad drive5.py) polled `VTC:amarchy-flat STATUS` for 420 s: `09:27:59 A boot1: no STATUS in 420 s`. Steps A and B not run.
- Finding: the loss of wasabid is not specific to `rig.py reboot`; a plain `stop --keep` then `start --keep-sys` on this image also failed here (Run 4's fresh `start --keep-sys` worked). Difference to Run 4: this boot ran the new Amarchy-Flat startup line on a user-startup edited after the first boot. Cause not isolated (flat BOOT freezing the guest daemon is one candidate; host load 41-83).
- Rig 66 stopped and cleared.

## Run 6 (s56, rig 67) - CONFIRMED

Date 2026-11-04 10:50-11:13. Build: `make amiga` from main d873ce13 (contains the FLAT-BOOT-FREEZE fix 5dee7eb5); wasabid 0.11b2 stayed up through every boot (no regression of the freeze). Rig: `UPTERM_RIG=67 rig.py start --max --os 32`, then `stop --keep`, Amarchy-Flat block appended to build/rig67/os-32/S/User-Startup (10:55), `start --keep-sys` for every later boot.
- Boot 1 (up 10:57:57): `flat=on calls=0 chained=0` at 10:58:05; ENVARC:Amarchy empty already (the start step takes ~2.5 min, so the 60 s window had passed before the first read). Not a usable fast-reset boot.
- Boot 2 (stop --keep 10:58:32, start 10:58:43; script polled STATUS from the start step in parallel):
  - 10:59:06 `flat=on calls=18 chained=0`
  - 10:59:07 `List ENVARC:Amarchy/flat-starting` -> `flat-starting                    16 ----rwed Today     10:59:05` / `1 file - 16 bytes - 2 blocks used` (marker on disk, written 10:59:05, read-only List)
  - 10:59:07 `rig.py stop --keep` (reset ~2 s after the List, ~25 s after flat started, inside the 60 s window).
- Boot 3 (start 11:04:45; output said `already running` because the boot-2 start process had brought the rig back up after the stop): first read 11:04:47 `flat=held calls=0 chained=0 why=marker`; `List ENVARC:Amarchy` -> `Directory "ENVARC:Amarchy" is empty` (marker consumed by the held start, flat.c:1490).
- Control (stop 11:05:12, start 11:05:23): 11:08:05 `flat=on calls=49 chained=0`; `List ENVARC:Amarchy` empty at 11:08:05 and 11:09:42 (marker deleted by the 60 s timer; boot ran > 90 s). Reset 11:09:48 (stop --keep), start 11:09:59: 11:12:43 `flat=on calls=58 chained=0` (no marker -> no hold).
- Verdict: CONFIRMED. A reset within 60 s leaves ENVARC:Amarchy/flat-starting on disk (listed pre-reset) and the next boot reads `flat=held why=marker`; after a >90 s boot the marker is gone and the next boot reads `flat=on`.
- Caveat: boot 3 and control were observed after `stop --keep` + `start --keep-sys` (a hard kill of fs-uae), the same kind of reset as a Guru/reboot as far as the disk is concerned.
- Rig 67 stopped and cleared.
