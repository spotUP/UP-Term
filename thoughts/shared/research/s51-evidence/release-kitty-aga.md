# Kitty T-E on AGA: NOT TESTED (rig.py aga hangs)
HEAD 71ce8edc. Verdict: no PASS/FAIL, kitty_z_rig.py never ran.
Rigs 77 and 78 (fresh each): start --max OK, ami.py ping answers (wasabid 0.11b2). `rig.py aga` reboots into AGA; its internal `rig.py start` exits 1 (CalledProcessError) after waiting; serial.log shows the AGA Workbench shell prompt (BOOTX:) is reached, but wasabid on the Amiga never answers ami.py ping (TCP connect TimeoutError errno 60). Same on both rigs, host load only fs-uae rigs plus other sessions' clang/drv.
Hypothesis: wasabi daemon/network (bsdsocket/net stack) does not come up on the AGA screenmode boot; rig.py aga bug, hunt separately.
Rigs 77, 78 stopped and cleared.
