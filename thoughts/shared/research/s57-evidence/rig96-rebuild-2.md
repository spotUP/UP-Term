# Rig 96 rebuild 2 (s57): owner eye-check rig, AmigaOS 3.2

- Build: worktree ../vtcon-s57-rig96b (branch s57-rig96b) at main 7cf4ac17. First build at e111afbe was refused by RIG-STALE-BASE (origin/main had moved 9 commits); fast-forwarded and rebuilt. make worktree-ready, wasabi, amiga, dist (UPTERM_PORTS=kit-main) rc 0.
- Old rig 96 stopped (build/rig96 removed), old logwatch pid 6761 killed.
- Install: `installer_rig.py ONLY fresh-all`, INSTALLER_KEEP=1: [PASS] fresh-all, 35 checks ok, Amarchy menu window after reboot True.
- Window title (vtcon-rig.fs-uae and start.args after the reboot): `vtcon rig 96 (060 max, OS 32) - vtcon-ee s57: owner eye check rig 96 (knobs, Locale, UPDemo 15)`. Purpose survived the installer reboot.
- Assign watch (`ami.py exec "Assign UP-Term: EXISTS" 10`, EXISTS only):
  - 2026-11-04 23:46:21 (right after install): rc 0, line `UP-Term  Empty-12MB-FFS:UP-Term`. ami.py requesters: empty.
  - 2026-11-04 23:51:29 (about 5 min later): rc 0, same line. ami.py requesters: empty.
  - Result: assign survived to 5 minutes; the loss from the earlier rig 96 was not reproduced.
- Rig state: up, idle watchdog 0, --keep; no other *_rig.py script run against it.
