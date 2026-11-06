# Installer check 10 (other-disk drawer leaves SYS:UP-Term) on Amiberry OS 3.2, rig 57

Run: `UPTERM_RIG=57 UPTERM_RIG_EMU=amiberry UPTERM_RIG_FLAGS='--max --os 32' python3 -I tools/rig/installer_rig.py ONLY default FORCE`
(case `default` is the one with check 10; the script boots its own rig). Log: scratchpad check10.log (deleted copy; lines below are exact).

## Verdict: the CHECK is wrong (it predicts the Installer's drawer with a second helper run); the Installer is right.

Fails together (same cause): 5, 8, 10, 11 -- passed 34 of 38.
- FAIL 5 `ENVARC:up-term/Dir names Work:UP-Term: Empty-12MB-FFS:UP-Term`
- FAIL 8 `S:User-Startup assigns UP-Term: to Work:UP-Term`
- FAIL 10 before = `(20, 'LIST: Could not get information for "SYS:UP-Term"... object not found')`,
  now = `(0, 'Directory "SYS:UP-Term" ... MANIFEST 80906, fonts, Rexx, S, UP-Term.version, VERSIONS, emoji, unifont, licenses, share, Amarchy, bin, pkg, etc ... 213 blocks')` = the whole kit
- FAIL 11 `"Work:UP-Term/VERSIONS" is there`
Check 4 (assign names Work:UP-Term) and 12-30 passed: UP-Term: reaches the kit, so files landed in SYS:UP-Term.

## Differing lines
before: SYS:UP-Term does not exist (clean 3.2 image has no empty drawer; the comment "the master image has an empty one,
BASEDEST-EMPTY-SYSUPTERM" only holds for the 3.1 master). now: full kit tree. Not a date/block diff: absent vs installed.

## Cause (measured on the live rig after the run)
- `ami.py exec "VTC:distkit/Files/UPBigVol LIST"`: Work:, BOOTX:, VTCX:, Empty-12MB-FFS: (= SYS:) all `rw 20333840 KB free`;
  `Info` shows DH0..DH3 with identical sizes/used (all four are host directories on one disk). `ENV:UPTBIGVOL` after the
  Installer = `Empty-12MB-FFS:UP-Term`.
- install/bigvol.h:54 bv_choose: most free KB wins; a tie goes to SYS:. With four equal volumes the winner is SYS: (Empty-12MB-FFS).
- installer_rig.py:879 big_drawer() ran UPBigVol earlier in the same case and got `Work:UP-Term` (free counts of the shared host disk
  moved between the two calls, so Work: won by a block). The Installer's own later run tied and took SYS:UP-Term.
- So the Installer installed into SYS:UP-Term (correct per its rule), but the case expected Work:UP-Term, and at
  installer_rig.py:1172 `drawer.upper() != 'SYS:UP-TERM'` (expected drawer, not the real one) enabled check 10, which
  then compared a populated SYS:UP-Term to an absent one. Checks 5/8/11 fail from the same wrong expected drawer
  (check 5's text also shows volume name vs device name: Empty-12MB-FFS vs Work).
- Not a 3.2 or Amiberry effect as such: on 3.1 rigs the volumes differ in free space, so the prediction held. Dist/Install.installer
  writes SYS:UP-Term only when `#big` is empty or a tie picks SYS: (Install.installer:208); no other writer (grep 'SYS:UP-Term').

## Proposed root-cause fix (check side, installer_rig.py run_base_case)
1. Take the real drawer from the Installer, not from a second UPBigVol call: read `ENVARC:up-term/opts/UPTOPTDEST` /
   `ENV:UPTBIGVOL` left by the Installer (or the drawer-page string, as run_bigvol does) after the run, and use that for
   checks 4, 5, 8, 11 and for the `!= SYS:UP-Term` gate before check 10.
2. Make the choice deterministic on rigs whose volumes share a host disk: give the rig one volume with clearly more
   free room, or accept SYS: as the drawer and skip check 10 when the real drawer is on SYS:'s volume.
3. Compare volume names, not device names (Work vs Empty-12MB-FFS) in 5/8.
4. Fix the stale comment in install_rig.py:588 / installer_rig.py:1079 (no empty SYS:UP-Term on a clean 3.2 image).

## Other findings
- Boot of rig 57 (Amiberry, --os 32) took ~540 s ("go not started" for 480 s, then ok) in run 1, which my `timeout 580`
  killed (no verdict); run 2 booted in ~9 min as well. The case itself needs ~6 min after boot. Cause of slow boot not investigated.
- The case ends with `[FAIL] default ole >NIL: CON ON` -- the failure detail printed only a tail of S:User-Startup.

## Fix

Checks 4, 5, 8, 11 and the check-10 gate now use the drawer the Installer recorded in ENVARC:up-term/Dir (real_drawer), and check 10 runs only when that drawer is off the SYS: volume (on_sys_volume resolves the SYS: volume name). Commit: 4e01e26a. Test: tests/test_installer_rig_real_drawer.py (make ONLY=rig_real_drawer).

## Rig verify after 4e01e26a

Run: rig 57, Amiberry, --os 32, `installer_rig.py ONLY default FORCE`; log build scratchpad check10-verify.log. Boot ~420 s.
- Real drawer chosen by the Installer: Empty-12MB-FFS:UP-Term (the SYS: volume).
- ok 4 UP-Term: assigned to Empty-12MB-FFS:UP-Term; ok 5 ENVARC:up-term/Dir names Empty-12MB-FFS:UP-Term; ok 8 S:User-Startup assigns it; ok 10 (log numbering) VERSIONS there; ok 11 UP-Term:bin/sh there. Logged checks 1-29 all ok.
- Run ended `[ABORT] default: ERR: the Amiga stopped answering while Execute was running (nothing for 25 s, and a ping got no reply) (no verdict written)`, exit 2, after ok 29 (UPConsole STATUS). Rig answered ami.py exec afterwards, so likely a harness/emulator stall, not a product fail.
- Not reached: the other-disk check (check 10 gate, installer_rig.py:1226 `if not on_sys_volume(drawer)`), so skip-vs-fail is UNVERIFIED by this run; `[FAIL] default ole >NIL: CON ON` did not appear.
- `Assign LIST` line for SYS (exact): `SYS            Empty-12MB-FFS:`  -- no `SYS  <Volume>:` pair beyond whitespace; sys_volume_name() splits on whitespace into ["SYS","Empty-12MB-FFS:"] and returns EMPTY-12MB-FFS, which matches the drawer's volume, so on_sys_volume is True (matches the assumption).
- Correction: the gate sits between "UP-Term: block comes before..." (ok 9) and the file checks (ok 10 = VERSIONS) at installer_rig.py:1226. The log goes ok 9 -> ok 10 with no other-disk line, so check 10 was SKIPPED (not failed) on the SYS: volume. Verified from the log sequence.
