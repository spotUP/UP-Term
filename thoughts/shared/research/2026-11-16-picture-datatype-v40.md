---
date: 2026-11-16
topic: Origin of picture.datatype v40.4 on rig 84; other rigs with picture < 43 under ilbm 43.x
status: partial (host-side evidence only; hardfile contents not mounted)
---
# picture.datatype v40.4 on rig 84

## Q1 who installed it
- PROVEN: picture 40.4 (26.5.93) is the STOCK AmigaOS 3.1 file. Host tree build/rig/os-31/Classes/DataTypes/picture.datatype (9768 bytes, host mtime 9 Nov 2026) carries "picture 40.4 (26.5.93)"; its ilbm is "ilbm 40.3 (29.4.93)".
- PROVEN: rig 84 has no host-side system dir; build/rig84/sys.hdf is an RDB hardfile (1572864000 bytes), an APFS clone of the master UPTERM_SYSTEM_HDF = /Users/spot/Code/UP-Term/images/nyhd2.hdf (paths.py SYSTEM_HDF; clone made per start by rig.py). Not mounted.
- PROVEN: no repo script/kit installs picture.datatype (git grep: only docs, README.txt:679 says the 3.1 one is V40 and a V43 must be installed by the user).
- PROVEN: no ledger/handoff row records an install on 01-Nov. Only the S85 handoff (2026-11-16_vtcon-s85-dopus-backdrop.md) says "installer unknown".
- INFERENCE (unverified): the 01-Nov-26 date is the install date of the master image's Workbench 3.1 base (stock files get the install date), not a later single-file install. Backup images/nyhd2-pre-patina-2026-11-13.hdf has file mtime 1 Nov 23:43, consistent with a master built ~1 Nov. Which person/tool built it: unknown.
- UNVERIFIED: a raw byte grep of nyhd2.hdf, nyhd2-pre-patina and rig84/sys.hdf finds the string "picture.datatype 43.41" in all three (master too) but no 40.4 string; that string may be inside a different file (e.g. an archive/Storage copy) or the master's DataTypes copy is not text-greppable. Cannot tell from the raw image which path holds which version without mounting. Note: master hdf mtime is 13 Nov, after 1 Nov, so S85's 40.4 was in the clone; the 43.41 hit in the master may be a Storage/backup copy.
- Where ilbm 43.10 came from: UNVERIFIED (not stock; stock 3.1 ilbm is 40.3).

## Q2 other rigs
- PROVEN: only build/rig (sys.hdf + host dirs os-31, os-32) and build/rig84 hold system disks right now; 5 build/rig*/ dirs exist, rig81 not among them (excluded anyway).
- build/rig/sys.hdf: hardfile, skipped (scan timed out, not mounted).
- build/rig/os-31 (host dir): picture 40.4 (26.5.93), ilbm 40.3: ilbm is NOT 43.x, so does not match. Date 9 Nov 09:33.
- build/rig/os-32: picture 47.5, ilbm 47.4: no match.
- Result: no other rig matches picture < 43 with ilbm 43.x on host-side files. Hardfile sys.hdf of build/rig and rig84 unmounted: UNVERIFIED.

## Master patch (2026-11-16, owner approved)

- Rig clone source: `UPTERM_SYSTEM_HDF`, default `images/nyhd2.hdf` (tools/rig/paths.py:340, rig.py fresh_sys_hdf `cp -c`). DH0: is the system partition.
- Before: master had picture.datatype 40.4 (9768 bytes, stock 3.1) under ilbm.datatype 43.10. Clone build/rig84/sys.hdf already had 43.41 + picture.datatype.v40.
- Patched with `xdftool` (host, no emulation, no fs-uae had the file open): old file written as picture.datatype.v40, picture.datatype replaced by 43.41 (34028 bytes, from Dropbox WB/Classes/DataTypes), both protection rwed.
- Proven after write: read back, picture.datatype is byte-identical to the Dropbox file and reports "picture.datatype 43.41 (20.12.98)"; .v40 is byte-identical to the old 40.4 file.
- Backup: `images/nyhd2.hdf.bak-2026-11-16-picdt` (APFS clone via cp -c, 1572864000 bytes logical, shares blocks until the master changes).
- New rig starts now clone the patched master.
