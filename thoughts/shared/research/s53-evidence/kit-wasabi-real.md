# KIT-WASABI-DOUBLE with the real kit (rig 91, 2026-11-03) -- INCOMPLETE, no verdict

Verdict per point: NOT MEASURED. The kit install never ran.

## What ran
- `make kit-stage` in ../upterm-ports: rc 0; `git status --short` there identical before and after (nothing committed).
- Rig 91 start 1 (--max --purpose "vtcon-ee s53: wasabi double-daemon kit proof"): rig.py exit 1
  "ERR: the Amiga stopped answering while Assign was running (nothing for 25 s, and a ping got no reply)";
  boot.log ended "assigns done / wasabid started"; ami.py exec then died with TimeoutError on connect (daemon
  silent) with fs-uae at ~65% CPU: S31-RIG96-FREEZE signature. Stopped rig 91 (build/rig91 removed).
- Rig 91 start 2 (retry once): very slow (more than 40 min wall), boot.log again "assigns done / wasabid started",
  serial.log reached the BOOTX: shell prompt. Later `ami.py ping` -> "wasabid 0.11b2", and
  `Status FULL` showed Process 2 (16384, the exec call) and Process 3 (stk 262144) both "BOOTX:wasabid",
  no C:wasabid: one daemon, as expected from the rig-wasabid block. Rig answered.
- `install_rig.py --only wasabi` then failed at its first step (owners.remove -> installed_addons -> run) with
  ConnectionRefusedError: fs-uae pid 53576 for rig 91 had exited cleanly at 02:39:08
  (fs-uae.log.txt: "fs-uae shutting down, fs_emu_run returned"), nobody on my side issued stop. Cause unknown
  (another session stopping 91, or the rig watchdog); not investigated further (budget).

## Not answered
1. exactly one wasabid after kit install: not measured
2. nothing on host port 1234: not measured (only 1324, rig 91's own, was seen LISTEN on its fs-uae)
3. second `C:wasabid` rc 5 "already running": not measured
Post Amiga-side reboot: not measured.

## Needed next
A rig 91 that stays up for the install; start 2 took far longer than normal and the rig vanished mid-use.
Check host load and who else touches rig 91 before retrying.
