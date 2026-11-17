# Kit names over 30 characters (s75, 2026-11-12)

## 1. Live patched archives: PASS
- `gh release download v1.1 -R spotUP/vtcon -p up-term.lha -p up-term-full.lha`: 34091020 and 72958627 bytes (the patched sizes).
- Host: `lha t` rc 0 for both. Host listing: up-term.lha 1439 files (1554 entries), up-term-full.lha 1469 files (1585 entries).
- Rig 99 (`UPTERM_RIG=99 rig.py start --max --os 32 --purpose "s75: kit filename length test of patched v1.1"`), Amiga LhA 1.51 from ~/Code/Reference/SanctuaryBBS/Node0/C/LhA, archives in Work: (host dir).
- `LhA -q x Work:up-term.lha` in Work:a: rc 0, 1439 files on disk = 1439. `LhA -q x Work:up-term-full.lha` in Work:b: RC_FULL=0, 1469 files on disk = 1469, no error output.
- `0017-secret-normalize.patch` present in both: Work:a/up-term/Files/net/bebbossh-1.46.1/patches/ and Work:b/up-term/...
- Base install (`CD Work:a/up-term`, `Execute Files/install.dos Files`, two y/N prompts answered n): BASERC=0, "UP-Term installed. Try: NewShell XCON:0/20/640/300/UP-Term/CLOSE". Install-Packages not run (coordinator scope change: ipks untouched).

## 2. Real limit
- Rig volumes are fs-uae HOST DIRECTORY mounts (vtcon-rig.fs-uae: hard_drive_0 = build/rig99/os-32 dir, hard_drive_3 = work dir "Work"), not FFS hdf images. Host-dir mount has no 30-character limit.
- Measured on Work: `Echo "x" >Work:<name>` with names of 30, 31, 32, 34, 40 characters: all five created, listed by `List`. 40 characters is fine.
- LhA 1.51 extracting a host-made archive holding d/ with 30, 31, 32, 40-character files and one 34-character file: all extracted (6 files OK), but the 34-character entry came out as directory `n34_.../n34_...` (name mangled) while the 40-character one was correct. So the 1468/1469 failure is not a volume limit; it is LhA 1.51 handling of that entry (pattern not understood; 31, 32 and 40 are fine). Cause in LhA NOT found.
- Classic FFS (DOS\0..DOS\3) limit is 30 characters: documented AmigaDOS fact, NOT measured here. No --os 31 rig test: the --os images are also host directories (paths.OS_IMAGES, own copy per rig), not FFS hdf, so a rig cannot show the FFS limit; skipped. Owner's real hardware on classic FFS would reject 31/32-character names: unverified.

## 3. Components over 30 characters (release v1.1, 38 lha/ipk assets scanned; up-term-src.lha 139 MB skipped)
Count from archive listings (lha l; ar + tar listing inside each ipk): 9 names in up-term.lha and up-term-full.lha, 3 ipk file names, 0 paths inside any ipk, 0 in the other lha archives.
Second count from source: dist/net/bebbossh-1.46.1/patches (2), themes/*.conf (7, Makefile `cp themes/*.conf` into Files/themes), tools/pkgs.py ipk_name over dist/packages.txt (3) = 12. Same 12.

| path | length | produced by |
|---|---|---|
| Files/net/bebbossh-1.46.1/patches/0015-bebbosshd-sftp-shell.patch | 31 | dist/net/.../patches |
| Files/net/bebbossh-1.46.1/patches/0022-utf8-in-vtcon-window.patch | 31 | dist/net/.../patches |
| Files/themes/github-light-high-contrast.conf | 31 | themes/ |
| Files/themes/gruvbox-dark-hard-contrast.conf | 31 | themes/ |
| Files/themes/gruvbox-dark-soft-contrast.conf | 31 | themes/ |
| Files/themes/gruvbox-light-hard-contrast.conf | 32 | themes/ |
| Files/themes/gruvbox-light-soft-contrast.conf | 32 | themes/ |
| Files/themes/shades-of-purple-super-dark.conf | 32 | themes/ |
| Files/themes/zenbones-forestbones-light.conf | 31 | themes/ |
| up-hexedit_1.0_m68k-amigaos.ipk (also Packages/ in up-term-full.lha) | 31 | pkgs.ipk_name |
| up-picocom_1.0_m68k-amigaos.ipk (also Packages/) | 31 | pkgs.ipk_name |
| up-sqlite3_1.0_m68k-amigaos.ipk (also Packages/) | 31 | pkgs.ipk_name |

Rename of 0017 (34) already shipped; the local build/dist kit still holds the old 0017 name (stale build).

## 4. Test
tests/test_dist_name_len.py (python, unittest; same style as tests/test_dist_*.py; not wired into the Makefile yet). Scans dist/ tree, themes/*.conf, feed names from pkgs.ipk_name/archive, plus build/ipk, build/dist/* and KIT=<dir> when present.
`nice -n 19 python3 -I tests/test_dist_name_len.py`: FAILED, 12 distinct names over 30 (the table above), plus the stale 0017 (34) from build/dist.

## Cleanup
Downloads (~210 MB archives, ipks) and scratch deleted. Rig 99 stopped and cleared (rig.py saved 140 files / 121 MB written on the rig to build/rig-saved/99/20261112-231951: delete when not needed).
