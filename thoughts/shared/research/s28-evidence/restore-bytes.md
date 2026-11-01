# s28 RESTORE byte-for-byte check (AmigaOS 3.2, rig 9, main 4c8b3f10 incl. 5a15f927)

Script: tools/rig/amarchy_icons_restore_rig.py (new): List every .info on SYS:, VTCX:, BOOTX:, Work:, md5sum on the Amiga, APPLY, md5, RESTORE, md5, compare.
Rig: fresh `rig.py start --max --os 32` + `rig.py install` (handler only, no kit). amarchy-icons from build/amiga (this tree), md5sum from the userland feed.

[WARN] List VTCX: rc 20
[INFO] 236 .info files on ['SYS:', 'VTCX:', 'BOOTX:', 'Work:']
[INFO] UP-Term:Amarchy/Icons/lucide.dat on the rig: missing
[WARN] no md5 for 1 files, first: ['SYS:Storage/DOSDrivers/AUX.info']
[INFO] APPLY rc 0: written=254 skipped-done=38 skipped-bad=0 skipped-nodat=0 skipped-nodefault=15 skipped-io=14 restored=0 restore-failed=0 skipped-readonly=0 skipped-locked=0 skipped-uaem=0 fallback=190 skipped-deep=0
[INFO] changed by APPLY: 234
[INFO] RESTORE rc 0: written=0 skipped-done=0 skipped-bad=0 skipped-nodat=0 skipped-nodefault=0 skipped-io=0 restored=254 restore-failed=0 skipped-readonly=0 skipped-locked=0 skipped-uaem=0 fallback=0 skipped-deep=0
[PASS] byte-identical 235 of 235, differing 0

Verdict: PASS. 235 of 235 hashed .info files byte-identical after APPLY then RESTORE; APPLY changed 234 of them (the 235th is not a walked target); restored=254 = written=254, restore-failed=0, rc 0.
Gaps: SYS:Storage/DOSDrivers/AUX.info could not be hashed (md5sum gave no line); `List VTCX:` returned rc 20 so the VTCX list may be partial. Neither is a difference.

## lucide.dat in the kit
Rig after `rig.py install` has no UP-Term:Amarchy/Icons/lucide.dat (rig.py install copies the handler only). The kit does ship it: Makefile dist-base depends on LUCIDE_DAT and copies dist/Amarchy/Icons/lucide.dat into the kit (Makefile ~1713); `make dist` built it here (121740 bytes). The script puts it on the rig when missing.
