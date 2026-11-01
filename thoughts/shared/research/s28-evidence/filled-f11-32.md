# F11 on OS 3.2: filled Phosphor icons APPLY / RESTORE (rig 11, 2026-11-01, origin/main b9a7f933)

Measure only. Tool: tools/rig/amarchy_icons_restore_rig.py --vols SYS:,VTCX:,BOOTX:,Work:,RAM: (no tool change needed).

## A. APPLY then RESTORE
- 237 .info files listed, 236 md5-able (SYS:Storage/DOSDrivers/AUX.info gave no md5 from the md5sum helper; List shows it 481 bytes, so a helper quirk, not a missing file).
- APPLY rc 0: written=254 skipped-done=38 skipped-bad=0 skipped-nodat=0 skipped-nodefault=15 skipped-io=14 skipped-readonly/locked/uaem/deep=0 fallback=190. 235 of 236 listed files changed.
- RESTORE rc 0: restored=254 restore-failed=0. PASS: byte-identical 236 of 236, differing 0.
- A second APPLY gave identical counters (deterministic); a second RESTORE restored=254.
- Screenshot after the second APPLY: ~/Desktop/upterm-shots/s28-f11-32.png (640x256, not viewed; visual result UNVERIFIED).

## B. Per volume (this rig has no host/network/uaem/CD volume; DF0 empty)
| Volume | Device | Disk.info present | md5 before / apply / after | Verdict |
|---|---|---|---|---|
| Empty-12MB-FFS (SYS:) | DH0 | yes | 1988deac.. / a96e556a.. / 1988deac.. | written by APPLY, restored identical |
| VTCX: | DH1 | NO | none | no file to walk; the walk never sees the volume, no counter. Workbench shows def_disk |
| BOOTX: | DH2 | NO | none | same |
| Work: | DH3 | NO | none | same |
| RAM Disk: | RAM | yes (RAM:Disk.info, listed as link "> ENVARC:Sys/def_RAM.info") | 374909df.. / bd703647.. / 374909df.. | written (via def_RAM), restored identical |

- Volumes without Disk.info take ENV:Sys/def_disk.info. APPLY wrote ENV:Sys/def_disk.info and ENVARC:Sys/def_disk.info (both md5 f969a8a9b9faf9cbd296b0981cbd0e13 after APPLY). So 3 of the 5 drive icons depend only on def_disk. Whether Workbench 3.2 redraws them live, or uses another default (a def_diskarchive.info exists in ENVARC:Sys), is not measured; needs the screenshot looked at / a Workbench restart.
- No volume was skipped for read-only: skipped-readonly=0, skipped-locked=0, skipped-uaem=0.
- skipped-nodefault=15: icons.c/iconapply_core.c apply_one: dat table paths (names with ':' e.g. UP-Term:Amarchy/AmTile, SYS:Prefs/..) whose .info file does not exist on this rig and are not def_* (no system default object). Inferred from source, individual files not logged by the tool (it only counts).
- skipped-io=14: the tool does not name them. All 236 listed files have non-zero size. Candidate cause, UNVERIFIED: `List VTCX:` returns rc 20 on the dangling entry "VTCX:Songs/deflemask/PotaJoe (ShortFieldGames)" (object not found), so walk_dir Lock failures count as skipped-io; plus read failures. Per-file identification needs a log line in the tool (not done: measure only).
- Tool gap found: skipped-* are counters only; no per-file log, so which files hit skipped-io/nodefault cannot be listed without changing amarchy-icons.

## C. make dist / dist-check
- make wasabi dist, make rig-helpers: OK. Kit lucide.dat: dist/Amarchy/Icons/lucide.dat and build/dist/up-term/Files/lucide/lucide.dat both md5 27455828d6a7f1a568fd13ca102904da (119436 bytes expected): MATCH.
- make dist-check: FAIL, 1 of 15 tests: test_the_total_of_an_add_on_is_what_its_archive_installs: "uhc: 1440 KB for 4 KB" (embedded 1440 KB vs archive 4 KB). Add-on uhc, not icons; not investigated.
