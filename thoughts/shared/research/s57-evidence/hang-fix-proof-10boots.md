---
date: 2026-11-05
topic: Start hang fix proof, rig 141, main 92b29ff8
verdict: FAIL / INCONCLUSIVE (4 of 10 boots PASS, boot 5 rig stopped answering)
---

# Start hang fix proof on rig 141 (3.2, 060 max, --idle-minutes 0, 640x256x8)

Build: main 92b29ff8 (make amiga && make dist with absolute UPTERM_PORTS = kit-main worktree).
Recipe: INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all (every option ticked), then loop of rig.py reboot.

Install boot: PASS (step 34 AMARCHY port answers STATUS, step 35 menu window opens).

| Boot | rc | AMARCHY port | SYS:Prefs processes | Guru warn | CPU reset PC=f80edc delta |
|------|----|--------------|---------------------|-----------|---------------------------|
| 1 | 0 | yes | 0 | 0 | 0 |
| 2 | 0 | yes | 0 | 0 | 0 |
| 3 | 0 | yes | 0 | 0 | 0 |
| 4 | 0 | yes | 0 | 0 | 0 |
| 5 | HANG | wasabid stops answering during reboot's post-start Assign step | n/a | 0 | 0 |
| 6-10 | not run | | | | |

Boot 5 state: fs-uae alive at about 79% CPU for over 20 minutes, ami.py ping times out
(Errno 60), debug.log ends "nothing heard for 20 s", serial.log last write 11:23.
No Guru, no CPU reset line in fs-uae.log.txt (total count 0).
Cause UNVERIFIED: product hang (fix incomplete or another cause) or start-time race.
No debugger FIFO (rig not started with --debugger), so no task states captured.

Busy pointer: not measurable by ami.py; proxied by AMARCHY port presence plus zero Prefs processes. Proxy only.

Button pixels at 640x256 (2 px sides, 1 px top, rounded corners): NOT RUN, wasabid dead, no screenshot possible.

Rig 141 left running in the hung state for the owner.
