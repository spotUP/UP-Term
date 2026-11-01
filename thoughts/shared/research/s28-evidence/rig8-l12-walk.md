# s28 rig 8: L12 all-volume icon walk (AmigaOS 3.2, rig 8, head 1cb2ea9c)

Binary build/amiga/amarchy-icons md5 25e75ee02f85ccec1a7ad975267c3623 (Amiga side md5sum RAM:amarchy-icons identical).
lucide.dat built with `make lucide-dat` (md5 de9875de3a319ebf77cec2c5ad0b81e9), put to VTCX:Apps/Amarchy/Icons/ (UP-Term: = VTCX:Apps) before locking.

## Setup (md5 before APPLY)
- SYS:L12/zzfallback.info (copy of Prefs.info, no table match, drawer) f506d8be426706993d7984a01ffda44c
- SYS:L12/zzlocked.info (Protect SUB wd, flags ----r---) f506d8be426706993d7984a01ffda44c
- SYS:L12/zzuaemdir.info (copy of Prefs.info, drawer) f506d8be426706993d7984a01ffda44c
- SYS:L12/zzuaem.info.uaem 401b30e3b8b5d629635a5c613cdb7919
- DH1: (VTCX) write-protected with `Lock DH1: ON` (Info: "Read Only"); `Lock RAM: ON` fails ("Attempt to lock drive RAM: failed"). DH1:zzro.info f506d8be..., DH1:KitTest.info d0fe86f9fdd70858b9401b7b761243bc, DH1:Apps.info dff21204ca9cfac7c73904845aa7cae0

## APPLY (output verbatim)
written=183 skipped-done=12 skipped-bad=0 skipped-nodat=0 skipped-nodefault=0 skipped-io=7 restored=0 restore-failed=0 skipped-readonly=74 skipped-locked=1 skipped-uaem=1 fallback=150 skipped-deep=0
(apply rc 0)

## STATUS (separate process, verbatim)
icons=applied
STATUS prints only this line; the counters (incl. skipped-deep=) are printed by APPLY/RESTORE, not STATUS (icons.c STATUS branch).

## After APPLY
- zzfallback.info and zzuaemdir.info both 74492404690ea592357029acdcd6c675 (rewritten); zzlocked, zzuaem, DH1 files unchanged.
- Backups: SYS:Storage/Icons/Amarchy/Empty-12MB-FFS/L12/{zzfallback,zzuaemdir}.info (md5 f506d8be... = original), also ENV, ENVARC dirs.
- Manifest lines: `93 Empty-12MB-FFS:L12/zzfallback.info ; c3bc8abb ; 2edb1754` (and zzuaemdir). The crc shown by Search is the manifest line, ok.

## RESTORE (verbatim)
written=0 skipped-done=0 skipped-bad=0 skipped-nodat=0 skipped-nodefault=0 skipped-io=0 restored=8 restore-failed=1 skipped-readonly=0 skipped-locked=0 skipped-uaem=0 fallback=0 skipped-deep=0
restore rc 5
STATUS afterwards: icons=applied (manifest and backups remain, 11K manifest).
md5 after RESTORE: zzfallback.info and zzuaemdir.info still 74492404... (NOT f506d8be...). Other files unchanged.

## Verdict
- skipped-readonly>0 PASS (74), skipped-locked>0 PASS (1), skipped-uaem>0 PASS (1), fallback>0 PASS (150), skipped-deep= printed PASS (in APPLY output, not STATUS), backups + manifest lines PASS.
- RESTORE byte for byte FAIL: restored=8, restore-failed=1, rc 5; the L12 files were not restored. No dialog, no crash, no hang. Cause not hunted (which entry failed is unknown).
