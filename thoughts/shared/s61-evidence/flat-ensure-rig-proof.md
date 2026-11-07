# flat-ensure rig proof (FLAT-ABSENT-AT-BOOT)

Rig 97 (--max --os 32). Script fix: none. Old binary = HEAD build in a temporary worktree, put at VTCX:Apps/bin/Amarchy (rig.py ensure_current gate bypassed via rig.GATE_STATE busy, else it restores the new build on every command).

## New binary (working tree), exit 0
    [INFO] UP-Term:bin/amarchy-flat (what Amarchy starts): UP-Term:bin/amarchy-flat 210220 Today 17:13:10
    [OK] step 1: no amarchy-flat resident before the restart
    [INFO] step 1 STATUS: flat=on calls=86 chained=0
    [OK] step 1: flat prefs on, the Amarchy start left amarchy-flat resident (STATUS flat=on|off)
    [OK] step 1: exactly one amarchy-flat process
    [OK] step 1: AMARCHY_FLAT_START >= 1 (desk_dos_flat_ensure started it)
    [OK] step 2: amarchy-flat already resident, the start gate started none (AMARCHY_FLAT_START 0)
    [OK] step 2: still exactly one amarchy-flat process
    [OK] step 3: amarchy-flat stopped before the restart
    [OK] step 3: flat prefs off, the Amarchy start left nothing resident
    [OK] step 3: AMARCHY_FLAT_START 0
    [OK] 0 failed

## Old binary (HEAD), exit 1
    [INFO] UP-Term:bin/amarchy-flat (what Amarchy starts): UP-Term:bin/amarchy-flat 210220 Today 17:13:10
    [OK] step 1: no amarchy-flat resident before the restart
    [INFO] step 1 STATUS: flat=absent
    [FAIL] step 1: flat prefs on, the Amarchy start left amarchy-flat resident (STATUS flat=on|off): flat=absent
    [FAIL] step 1: exactly one amarchy-flat process: []
    [FAIL] step 1: AMARCHY_FLAT_START >= 1 (desk_dos_flat_ensure started it): 0
    [OK] step 2: amarchy-flat already resident, the start gate started none (AMARCHY_FLAT_START 0)
    [FAIL] step 2: still exactly one amarchy-flat process: []
    [OK] step 3: amarchy-flat stopped before the restart
    [OK] step 3: flat prefs off, the Amarchy start left nothing resident
    [OK] step 3: AMARCHY_FLAT_START 0
    [FAIL] 4 failed

Result: step 1 FAILS before (STATUS flat=absent, AMARCHY_FLAT_START 0), all steps PASS after. Step 2's 'still exactly one process' fails on old only as a consequence of step 1 (none resident).
