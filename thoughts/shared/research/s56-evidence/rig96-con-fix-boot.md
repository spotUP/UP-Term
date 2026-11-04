# Rig 96 boot check after the CON 87000004 fix (s56, vtcon-16)

Verdict: 3 of 3 clean boots (plus the install's own post-install reboot, also clean). No "CON Program failed #87000004", no reset loop.

Dist: not rebuilt by this session. build/dist (mtime 2026-11-04 10:54:52) came from vtcon-ee's `make dist` (pid 33776, built from d873ce13, which contains fix 4ea3135c); it is newer than d873ce13 (commit 10:50:12). main was 4705d533.
Install: `UPTERM_RIG=96 UPTERM_RIG_FLAGS="--max --os 32 --idle-minutes 0" INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all` (every option ticked, CON on): [PASS], 35 checks ok, L:vtcon-handler, DEVS:DOSDrivers/XCON, PTY: mounted, Amarchy menu opens after the reboot. Only warning: "the Workbench stayed PAL (W43)" plus updemo files missing in the rig tree (installer-only).

| Boot | wasabid | 87000004 in serial.log | reset/guru/alert lines | Version L:vtcon-handler | requesters |
|---|---|---|---|---|---|
| post-install | 0.11b2 | 0 | 0 (349 lines) | vtcon-handler 1.0 | none |
| 1 | 0.11b2 | 0 | 0 (378 lines) | vtcon-handler 1.0 | none |
| 2 | 0.11b2 | 0 | 0 (407 lines) | vtcon-handler 1.0 | none |
| 3 | 0.11b2 | 0 | 0 (436 lines) | vtcon-handler 1.0 | none |

Each boot: `rig.py stop --keep`, then `start --keep-sys --max --os 32 --idle-minutes 0 --purpose ...`; checks 40 s after wasabid answered via ami.py exec/requesters/screens (front screen: Workbench Screen). No screenshots taken.
Rig 96 left RUNNING for the owner eye check.
