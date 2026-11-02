# AMARCHY-MENU-IXEMUL-BOOT-RACE fix (branch s30-ixrace, commit e07358c9)

Fix: tools/rig/rig.py SHADOWS_UP_SCRIPT (the early block before the kit's Amarchy block) now also runs the RIG_LIBS
assigns under `If EXISTS VTC:ixp6/ixemul.library` and writes RAM:rig-libs-done; GO skips the ncurses ADD and the RIG_LIBS pass when
that marker exists (no duplicate ADD entries).

Host test: tests/test_rig_kit_shadow_keymap.py::test_early_block_assigns_libs_ixp6_before_amarchy_and_go_skips_then.
Fixed: passes. rig.py change reverted: fails (5 failed vs 4). The 4 other failures in that file are pre-existing in a
worktree (they need built build/amiga/vsh etc.); not related.

Rig proof: NOT DONE. Cold boot of rig 57 (amiberry, OS 3.2) worked, but a fresh rig has no kit Amarchy block in
S:User-Startup, so the early block is not inserted and go does the assigns (boot.log: no early lines; LIBS: had ixp6,
ixemul 80.1). The race needs the kit installed first (installer_rig.py ONLY default); that run failed at rig_start
(rig.py start exit 1 while rig 57 already ran) and the budget ended. Open: install kit on a rig, cold boot twice,
check boot.log "early LIBS: assigns done (before Amarchy)", no ixemul requester, amarchy-menu in ps.
Note: go writes boot.log with `>` ("go started"), which truncates the early block's lines if the early block runs first;
the early log lines will not survive. Check RAM:rig-libs-done instead.
