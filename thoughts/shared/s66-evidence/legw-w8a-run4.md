# W8a run 4 (s66): legw on KS 3.2 after moving part "libs" ahead of "tools"
Verdict: ORDER FIX WORKS. Base Install no longer aborts (ixemul requester gone); legw passes 17 of 23. Remaining 6 FAIL are a different cause (below).

Fix (commit "Install ixemul.library before any part runs an ixemul program"): part libs moved from after boot to right after system in dist/install.dos and dist/Install.installer. Checked first: libs needs only $optnofpu (set at the top of install.dos), ENVARC/LIBS and, in the ICONLIB os30 branch, vsh (installed by system, which is before libs). Parts drawer and system run no ixemul program (drawer's vsh reassign.sh is shell builtins only). Uninstall is a single script; it restores ixemul.path/ixemul.none at :428-456, after its vsh unstartup calls, and stays consistent.
Test: tests/test_dist_installer.py `test_libs_part_comes_before_any_part_that_runs_an_ixemul_program` (both files). Fails on the old order (stash revert check), passes on the new one. tests/test_install_dos_amissl.py sliced libs..mount; now libs..copy-coreutils. 107 passed, 1 skipped.
Build: `make dist-base UPTERM_PORTS=.../kit-main` rc 0.

Rig (`rig.py start --os 32 --max --purpose "up-term-bb s66: W8a part order verify"`, came up as build/rig with no number; `install_rig.py --only legw`):
- ok 1 base Install runs; ok 2 UP-Term.version written
- FAIL 3-6 (a): no ";BEGIN UP-Term Amarchy-Flat" block. NEW FAIL cause: in the rig S:Startup-Sequence is the wrapper `BOOTX:bootcap / BOOTX:boot.console / C:Execute S:Startup-Sequence.os / EndCLI`; dist/instssblock.sh:30-38 inserts before the first IPrefs line and finds none, so it removes its output (found=0). Install itself ran instssblock.sh fine (rm/grep worked).
- ok 7-11 (b) WBStartup icons, (c) flat block listed in Status (block ran from elsewhere)
- FAIL 12 (c): AMARCHY port: WasabiError 'too many ARexx messages are still waiting for an answer'
- FAIL 14 (d): amarchy-flat process 7 not below 5 (User-Startup order)
- ok 15-23: resident patch, Uninstall, S:Startup-Sequence byte for byte
Proposed fix for 3-6, 14: the rig's boot wrapper hides the real sequence; the test (or rig boot) must target S:Startup-Sequence.os, or the rig's boot should keep IPrefs in S:Startup-Sequence. Not applied (outside budget; 12 may follow from the missing block).
Cleanup: rig stopped, build/rig removed, scratch logs under 10 MB.
