# Leg W implementation report

Branch: worktree-agent-a11020964f375b479
Worktree: /Users/spot/Code/UP-Term/vtcon/.claude/worktrees/agent-a11020964f375b479
Commit: 5b3df9ee (single commit, W1-W7). Not pushed.

## Per item
- W1 done: dist/instssblock.sh (replaces dist/iconlib/instremlib.sh); tests in tests/test_dist_iconlib.py on the real 3.1 and 3.2.2 sequences (tests/fixtures/Startup-Sequence.ks31, .ks322), idempotent, stripped by name and by plain call. Also removes a stale T: result at start.
- W2 done: install.dos has no Amarchy/index/Flat User-Startup writers (strip-only remains); Amarchy-Flat block goes into S:Startup-Sequence before IPrefs with the absolute path read from ENVARC:up-term/Dir.
- W3 done: install.dos copies wbstartup/*.info to SYS:WBStartup; parts.txt row `desktop Files/wbstartup`.
- W4 done: Uninstall deletes both icons and strips the flat block from the sequence (old User-Startup by-name strip kept).
- W5 done: reassign.sh with 2nd arg S:Startup-Sequence rewrites the flat block path; test in tests/test_dist_installer.py.
- W6 done: Makefile mkicon rows; tests/test_wbstartup_icons.py runs the Makefile rules and reads back do_Type 4, default tool, DONOTWAIT/STARTPRI/TOOLPRI.
- W7 done: tests/test_flat_startup_block.py rewritten; test_dist_installer.py and test_amarchy_docs.py updated; README.txt:732 and amarchy.c comment updated; rig.py / amarchy_gui_rig.py comments only.

## Tests (nice 19)
test_wbstartup_icons, test_flat_startup_block, test_dist_iconlib, test_dist_installer, test_amarchy_docs, test_dist_manifest: 147 passed, 6 skipped.

## Open / unverified
- tests/test_rig_a500_user_startup.py not run: needs build/amiga (absent in the worktree, collection error in tools/rig/kitset.py). Its fixture still carries old User-Startup Amarchy blocks, which is still valid for stripping.
- Rig logic (KIT_AMARCHY_BLOCK shadow pass in tools/rig/rig.py, FLAT_BOOT in amarchy_gui_rig.py) still keys on the old User-Startup blocks: needs update under W8 (rig run). Only comments changed.
- dist/common/update.installer:76, dist/Install.installer:215 and README.txt:63/999 not reviewed/changed; tools/kitsize.py not checked.
- install.dos itself cannot run on the host: the Installer steps are checked by text assertions only. WBStartup message reply (D6) unverified until W8.
