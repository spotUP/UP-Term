# Rig 96 rebuild (s59, 09:53): owner eye-check rig, AmigaOS 3.2, main 26b4c17f

- Worktree ../vtcon-s59-rig96 (branch s59-rig96) at 26b4c17f, 658 MB, left in place. make worktree-ready, wasabi, amiga rc 0; make dist rc 0 (absolute kit-main UPTERM_PORTS).
- fresh-all run 2 failed before install: RIG-STALE-BASE (origin/main moved to 3a27f3ce, 5 commits). Fixed by adding --allow-stale-base "owner asked for main 26b4c17f eye-check build" to UPTERM_RIG_FLAGS (the brief pins 26b4c17f). Run 1 log was lost (installer ended without verdict, same gate likely).
- Run 3: [PASS] fresh-all, 35 ok checks, "after the reboot: Left Amiga + Space opens the Amarchy menu window" ok. Kit kept (INSTALLER_KEEP=1).
- start.args: --max --os 32 --idle-minutes 0 --keep --allow-stale-base ... --purpose "vtcon-ee s59: owner eye check rig 96 (main 26b4c17f)".
- rig.py status: up (wasabid 0.11b2). hold --hours 4 set, ends about 13:53. Rig left UP.
- Warn seen: Directory Opus not installed in rig VTC: drawer; Workbench stayed PAL (W43).
