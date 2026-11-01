# Flat persist evidence

## FP-1 (plan R1.8): 15 s confirm after a desktop theme apply

Branch s29-fp1-confirm. Pure state machine desk_confirm_* in amarchy/desk_core.c (tests/test_desk.c test_desk_confirm: `make test ONLY=desk`, 1926 checks, 0 failed).
Revert proof: with the timeout comparison in desk_confirm_tick broken (`now < deadline + 1000000L`), `make test ONLY=desk` gives `[FAIL] desk: 1926 checks, 3 failed` (test_desk.c:1091, 1092, 1105); restored, 0 failed.
FP-2 hook: `desk_dos_confirmed()` in amarchy/desk_dos.c (Keep calls it; today it clears the pending marker and the Workbench.prefs snapshot).

Rig 12, AmigaOS 3.2 (tools/rig/amarchy_confirm_rig.py, sentinels AMARCHY_CONFIRM_SHOWN / _KEEPS / _REVERTS_USER / _REVERTS_TIMEOUT):
- Check 1 PASS: flat ON pick shows the requester "Amarchy desktop theme" (wasabi windows list), Keep clicked, KEEPS +1, ENV:Sys/Workbench.prefs byte 73 = 1.
- Check 2 PASS: flat OFF pick, no click, SHOWN +1, REVERTS_TIMEOUT +1, md5 before e30c49d39da3d7b2223aeccbdfc27d46, after 15 s e30c49d39da3d7b2223aeccbdfc27d46 (equal).
- A first run FAILED check 2 (md5 cb431079... after revert): Workbench.prefs, written by dk_wbborder, was outside the snapshot mask. Fixed by dk_wb_snapshot / dk_wb_restore; the rerun passes.
- Caveat: the pick helper returns after the 15 s window, so the "applied" md5 read in the script was already the reverted file; the proof is the failed-then-passed pair around the fix.
- Not verified by eye: the requester look. Revert click path (user Revert) is not rig-tested; REVERTS_USER sentinel exists.
