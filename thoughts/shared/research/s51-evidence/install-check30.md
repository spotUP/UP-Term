# Install check 30 (AmiSSL on LIBS: before the reboot), OS 3.2, rig 85

Verdict: the PRODUCT was wrong (dist/install-amissl.dos), not the harness check.

- a4928c9d (non-interactive install-amissl.dos) IS an ancestor of fe7bdb7c, so the failure was not the old nested installer.
- Reproduced on current main with `UPTERM_RIG=85 UPTERM_RIG_FLAGS="--max --os 32" python3 -I tools/rig/installer_rig.py ONLY fresh-all FORCE`:
  `FAIL 30 ... LIBS:amisslmaster.library is version 5 or newer: object not found`; checks 31-33 pass (AmiSSL:Libs holds the files).
- Cause: install-amissl.dos ends with `Version >NIL: LIBS:amisslmaster.library 5` then `If WARN` -> `Assign LIBS: AmiSSL:Libs ADD`.
  On a machine with no AmiSSL on LIBS: (clean 3.2 image) Version answers rc 20 (object not found), above Execute's default
  FailAt 10, so the script ABORTS at that line and never reaches the Assign (Execute itself still returns rc 0 to the caller).
  install.dos has `FailAt 21` (line 30), which is why the Shell install worked since f5746979.
  Hand run on the rig before the fix: script rc 0, AmiSSL: assigned, `Version LIBS:amisslmaster.library` -> object not found.
- Fix: `FailAt 21` before the first command of the script; test test_script_survives_the_missing_library_check
  (tests/test_install_dos_amissl.py) fails without it, passes with it.
- After the fix: same fresh-all run -> `ok 30`. Hand run of the fixed script: `amisslmaster.library 5.27` on LIBS:.
- Side note: the kit's AmiSSL step is not idempotent without FailAt (MakeDir on an existing Work:AmiSSL gave rc 10); FailAt 21 also covers that.
- Still failing in the same run, unrelated to check 30: check 39 (S:User-Startup after Uninstall differs: leftover
  ';BEGIN rig-upterm-shadows' block, the rig's own shadow block, not the kit's).
- Used a stale build/dist/up-term (12:54, before a4928c9d): refreshed with `python3 tools/mkkit.py base` + copying
  the fixed Files/install-amissl.dos, because `make dist` stops on "kit-stage in upterm-ports" (ports kit has no STAGED-FROM).
