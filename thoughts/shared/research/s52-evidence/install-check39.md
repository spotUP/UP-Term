# INSTALL-CHECK39-SHADOW-LEFTOVER (2026-11-03)

Writer: tools/rig/rig.py (SHADOWS_UP_BLOCK_TEXT, user_startup_with_shadows_block, called from sys_hdf_shadows_block /
os_tree_shadows_block on every rig start). Not the kit: no match for rig-upterm-shadows in dist/ or the kit scripts.
Cause: a reboot inside fresh-all re-inserts the rig block (kit Amarchy block present) after Install; Uninstall removes the
kit's blocks only, so check_startup_after_uninstall (tools/rig/install_rig.py, fresh-all check 40) saw it as a leftover.

Fix: user_startup.RIG_SHADOWS_BLOCK / without_rig_blocks; check_startup_after_uninstall compares both texts without that one named block
(stray lines and kit blocks still fail). install_rig.py main-case compare uses the same filter.
Test: tests/test_install_rig_check39_rig_block.py (check_startup_after_uninstall test fails with the filter neutered, passes with it).

Rig 91, OS 3.2, UPTERM_RIG=91 installer_rig.py ONLY fresh-all FORCE:
- before fix: "installer_rig fresh-all: passed 43 of 44", FAIL 40 after Uninstall: S:User-Startup as it was, byte for byte: first difference at line 39: now ';BEGIN rig-upterm-shadows', before ''
- after fix: "installer_rig fresh-all: passed 44 of 44", EXIT 0
