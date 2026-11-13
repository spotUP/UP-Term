# flat-ensure rig proof (FLAT-ABSENT-AT-BOOT)

Rig 97 (--max --os 32). Script fix: none. Old binary = HEAD build in a temporary worktree, put at VTCX:Apps/bin/Patina (rig.py ensure_current gate bypassed via rig.GATE_STATE busy, else it restores the new build on every command).

## New binary (working tree), exit 0
    [INFO] UP-Term:bin/patina-flat (what Patina starts): UP-Term:bin/patina-flat 210220 Today 17:13:10
    [OK] step 1: no patina-flat resident before the restart
    [INFO] step 1 STATUS: flat=on calls=86 chained=0
    [OK] step 1: flat prefs on, the Patina start left patina-flat resident (STATUS flat=on|off)
    [OK] step 1: exactly one patina-flat process
    [OK] step 1: PATINA_FLAT_START >= 1 (desk_dos_flat_ensure started it)
    [OK] step 2: patina-flat already resident, the start gate started none (PATINA_FLAT_START 0)
    [OK] step 2: still exactly one patina-flat process
    [OK] step 3: patina-flat stopped before the restart
    [OK] step 3: flat prefs off, the Patina start left nothing resident
    [OK] step 3: PATINA_FLAT_START 0
    [OK] 0 failed

## Old binary (HEAD), exit 1
    [INFO] UP-Term:bin/patina-flat (what Patina starts): UP-Term:bin/patina-flat 210220 Today 17:13:10
    [OK] step 1: no patina-flat resident before the restart
    [INFO] step 1 STATUS: flat=absent
    [FAIL] step 1: flat prefs on, the Patina start left patina-flat resident (STATUS flat=on|off): flat=absent
    [FAIL] step 1: exactly one patina-flat process: []
    [FAIL] step 1: PATINA_FLAT_START >= 1 (desk_dos_flat_ensure started it): 0
    [OK] step 2: patina-flat already resident, the start gate started none (PATINA_FLAT_START 0)
    [FAIL] step 2: still exactly one patina-flat process: []
    [OK] step 3: patina-flat stopped before the restart
    [OK] step 3: flat prefs off, the Patina start left nothing resident
    [OK] step 3: PATINA_FLAT_START 0
    [FAIL] 4 failed

Result: step 1 FAILS before (STATUS flat=absent, PATINA_FLAT_START 0), all steps PASS after. Step 2's 'still exactly one process' fails on old only as a consequence of step 1 (none resident).
