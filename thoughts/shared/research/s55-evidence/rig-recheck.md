# s55 rig recheck UD2 UD3 UD5 UD6 (rig 101, main 3f2f21aa) - NOT COMPLETED
- make wasabi + make amiga: ok. rig 101 started with purpose. make dist FAILED: upterm-ports build/kit/userland has no STAGED-FROM stamp (needs `make kit-stage` in upterm-ports, other repo, not run).
- Installer fresh-all (INSTALLER_KEEP=1, all boxes ticked, stale build/dist from 08:09) [PASS] 36 ok, reboot check ok.
- rig.py install pushed current build/amiga; C:UPDemo is 57840 bytes (10:18 build) after next boot.
- Reboot after install: reset loop (10+ CPU resets in 60 s) at 10:23:44, log build/rig101/reset-loop-20261104-102344.fs-uae.log; wasabid then unreachable ("frozen") ~15 min; stop+start recovered. No "87000004" requester seen in serial.log (possibly the known CON Lock boot bug; unconfirmed).
- Idle watchdog (15 min default) killed the rig during long waits once; use --idle-minutes.
- Checks UD3, UD6, UD2, UD5: NOT RUN (tool budget exhausted). No pixel measurements, no shots (ami.py was called with a bad command string in the last step).
- Scene numbers: splits 8, kitty animation 15, kitty placement 19, glyphs 21 (`UPDemo <n>` in an XCON window; recipe in s54 rig96-eyecheck-32.md third pass).
