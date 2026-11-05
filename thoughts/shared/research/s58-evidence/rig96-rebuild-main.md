# Rig 96 rebuild from main (vtcon-ee, 2026-11-05)

Main built: cb85ce2f (pull ff'd from 606dafcd). make wasabi/amiga/dist: rc 0 (dist needs an ABSOLUTE UPTERM_PORTS; the
relative upterm-ports/.claude/worktrees/kit-main path fails: m68k-cc not found).
Old rig 96 stopped (plain stop, folder removed) with no installer/bench runner on it.

## BLOCKER: rig start --os 32 always exits 1 (boot-console check)
`installer_rig.py ONLY fresh-all` (flags --os 32 --max --idle-minutes 0) and a direct `rig.py start --os 32 --max ...`
(stop --keep first) both print `up`, then exit 1:
  rig: the boot console was not captured (no 'boot console open' line in boot.console; BOOTX:bootcap did not run)
boot.log is complete (wasabid started), build/rig96/boot/boot.console does not exist.
Cause (read, not fixed): rig.py STARTUP (line ~153-167) runs bootcap, but a clean --os image uses OS_HOOKS (line 653,
appended to S:User-Startup at ~743): `Assign VTC:` + `Run C:Execute BOOTX:go`, no bootcap. So commit 606dafcd's check
(rig.py ~2285) can never pass on --os 31/32. Fix belongs in rig.py: either run the clean image's Startup-Sequence via
bootcap too, or skip the capture check when the image is a clean --os one.
Result: fresh-all never ran, no install, steps 4-6 not run. Rig 96 left UP (stock 3.2, kit not installed).

## Retry 2026-11-05 (main d4910cfd, sonnet agent)
- make wasabi amiga rc 0; make dist (absolute UPTERM_PORTS kit-main) rc 0. Host load 25-45 (other sessions' vitest/make dist), builds slow.
- Gate: PASSES now. `rig.py start --os 32` on rig 96 prints `up`; boot.console = "boot console open"; no [ERROR] lines. (1272faf9 fix works.)
- Commands that work: `UPTERM_RIG=96 INSTALLER_KEEP=1 UPTERM_RIG_FLAGS="--os 32 --max --idle-minutes 0" python3 tools/rig/installer_rig.py ONLY fresh-all --purpose "<text>"`.
  installer_rig.py is not executable (use python3); it rejects --idle-minutes/--max on its own CLI except --os/--max: idle-minutes goes through UPTERM_RIG_FLAGS.
- Install: fresh-all ran 33 ok checks (kit, ixemul patched, PTY:, UPConsole STATUS, AmiSSL 5.x, certs) with no failing check.
- Then the reboot step FAILED: `rig.py reboot` -> `start --keep-sys` printed `up`, then `ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)`, exit 1.
- State afterwards (18:33): fs-uae pid 60752 up 3.5 min at 70-100% CPU; boot.log stops at "UP-Term shadows done" (last line, mtime 18:30); wasabi 127.0.0.1:1329 times out; `rig.py status` = down; no Guru (T:lastguru absent); serial.log shows the XCON shell prompt was reached. Hang on the FIRST boot of the installed kit; Assign UP-Term: EXISTS not obtainable; 5-boot series not started (stop at first hang).
- Rig 96 left UP (hung) for the owner/peers to inspect; not stopped.
- Side note: `rig.py stop --keep` on rig 96 took ~4 min and a start --keep-sys of rig 96 appeared during it (the stop output printed "stopped" twice).

## Retry 3 2026-11-05 (s59)
- Built from main 80014d07 (merge-base check ok; HEAD moved to 511b7c36 by a peer during install; make wasabi amiga + make dist rc 0, absolute kit-main UPTERM_PORTS). No make on this checkout at start.
- Old hung rig 96 stopped (plain stop, folder removed). fresh-all (INSTALLER_KEEP=1, --os 32 --max --idle-minutes 0): PASS, 35 ok checks, 0 failures, includes one reboot, Amarchy menu opens.
- 5 further `rig.py reboot` boots: all rc 0, wasabi answers after each. The s58 first-boot hang did not recur. Final: `Assign UP-Term: EXISTS` rc 0. No guru/T:lastguru checked beyond clean boots.
- Peers refreshed Amarchy files on rig 96 mid-run ([REFRESH] lines); boot warns only about missing dopus stamp and updemo earth.anim/fjord.png.
- Rig 96 left UP, `rig.py hold --hours 8` running.
