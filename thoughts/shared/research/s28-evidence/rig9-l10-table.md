# L10 table-path check, rig 9, AmigaOS 3.2 (main 83126599)

Build md5: amarchy-icons 5e8818b593dc75499918085b42f3937d, lucide.dat de9875de3a319ebf77cec2c5ad0b81e9 (both read back from the rig and equal; the rig has no md5 tool, so .info md5s were taken on the host after rig file reads).
Deploy: VTCX:bin/amarchy-icons, VTCX:Apps/Amarchy/Icons/lucide.dat (UP-Term: = VTCX:Apps).

Note: the first APPLY failed with "lucide.dat missing or bad" because I ran `Assign UP-Term:` (removes the assign); re-added with `Assign UP-Term: VTCX:Apps`. Not a product defect.

## Results
- STATUS before: `icons=original`
- APPLY (rc 0): `written=6 skipped-done=0 skipped-bad=0 skipped-nodat=0 skipped-nodefault=0 skipped-io=6 restored=0 restore-failed=0 skipped-readonly=0 skipped-locked=0 skipped-uaem=0 fallback=0`
- STATUS after: `icons=applied`. Manifest lists only the 12 ENVARC:Sys / ENV:Sys def_*.info paths (disk, drawer, tool, project, trashcan, RAM).
- .info changed per drawer (md5 before vs after APPLY): System 0 of 10, Utilities 0 of 2, Prefs 0 of 21, Tools 0 of 12. Main's APPLY does not touch drawer icons (table paths = def_*.info only; the walk is not on main), so L10 "Lucide in the four drawers" is NOT met on main.
- RESTORE (rc 5): `written=0 skipped-done=0 skipped-bad=0 skipped-nodat=0 skipped-nodefault=0 skipped-io=0 restored=0 restore-failed=1 skipped-readonly=0 skipped-locked=0 skipped-uaem=0 fallback=0`
  STATUS after RESTORE still `icons=applied`: the RESTORE defect reproduces on main (restored=0, restore-failed=1). Not investigated.
- Drawer md5 after RESTORE equals before for all 45 files (trivially, nothing changed). The def_*.info files were not md5'd before APPLY, so a byte-for-byte restore of those is unproven (and RESTORE reported failure).

Per-file md5 before = after = restored: 45 files (System 10, Utilities 2, Prefs 21, Tools 12); host list was in scratch only, identical across the three snapshots.
