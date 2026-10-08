---
date: 2026-10-08
topic: Installer update -- copy only what changed
tags: [installer, dist, kit, amiga]
status: draft
---

# Installer update: copy only what changed

Owner request 2026-10-08 (direction decided by the owner): every install takes very long; the
Installer detects an installed UP-Term and offers an Update that copies only what changed.

## What is there (research)

- The kit is built by `make dist` (Makefile `dist:`), plain copies into build/dist/UP-Term;
  Files/VERSIONS from tools/mkversions.sh (first line `UP-Term <commit> <date> (upterm <commit>)`).
- dist/install.dos runs in parts (`Lab <part>`, `Execute install.dos <Files> STEP=<part>`); the
  Installer (dist/Install.installer) runs each part with P_STEP and copies the big drawers itself
  with copyfiles (P_COPY: copy-coreutils, copy-unifont, copy-emoji, copy-python, copy-nvim).
  Options travel in ENV:UPTOPT<keyword> files written by P_OPTIONS, deleted by P_OPTIONS_END.
- Nothing records the options after an install (markers in ENVARC:up-term name what Install
  made: gg, tmp, cryptossh, font-*, ixpipe, wasabid, wasabikey).
- Native install tools: install/upicon.c (vbcc, `$(VC)`), core in install/iconspec.c tested on
  the host in vttest_host (tests/test_iconspec.c). The new tool follows that split.
- Installer 43.3 has no file reading beyond (getenv) (an ENV: file whole) and (exists); a tool
  hands it values through ENV: files (the ENV:UPTermRemote pattern, Install.installer ~177).
- Rig: installer_rig.py drives the real Installer by gadget id (3.1: Installer 43.3, 3.2: 47);
  build/dist is shared with other agents' `make dist`: rig cases run from a private kit copy.

## Decisions

- D1 MANIFEST lines: `<part> <size> <crc32 hex> <path>`, path kit-relative (`Files/vsh`,
  `Install`), sorted by path bytes, path last (names have spaces: `Files/UP-Term Prefs`).
  `<part>` is the install.dos part that installs the file, `-` for none (README, icons of the
  kit's top drawer). One line per install.dos part section too: `<part> 0 <crc of the section's
  text> install.dos#<part>`, so a part whose script changed runs again.
- D2 The part of each kit path comes from dist/parts.txt (`<part> <kit prefix> [<UP-Term: drawer>]`,
  the third column for copy parts). Host test: every kit file is claimed by exactly one line,
  every copy part of install.dos/Install.installer has its line with the same drawers.
- D3 tools/mkmanifest.py (host, zlib.crc32) writes Files/MANIFEST at the end of `make dist`.
- D4 Install copies Files/MANIFEST to UP-Term:MANIFEST and the option files to
  ENVARC:up-term/opts/ (UPTOPT<keyword>, as P_OPTIONS wrote them) as its last part.
- D5 Start: UP-Term: assigned (or ENVARC:up-term/Dir) and UP-Term:MANIFEST and
  ENVARC:up-term/opts there: the first page names the installed version (first line of
  UP-Term:VERSIONS) and the kit's (Files/VERSIONS), and asks Update / Full install (Abort is the
  page's button). Without a manifest or a record (an install by an older kit) there is no Update:
  the full install, which records both.
- D6 Native tool `upupdate` (install/upupdate.c main, install/updiff.c pure core, vbcc,
  68020, no ixemul): `upupdate OLD NEW KIT` merges the two manifests by path; writes
  ENV:UPTUPD<part> (KB to copy for that part, at least 1 when it runs), ENV:UPTUPDKB (total),
  ENV:UPTVEROLD / ENV:UPTVERNEW (first lines of the two VERSIONS), and for each copy part
  T:UPTUPD-<part> (an AmigaDOS script: MakeDir for new drawers, `Copy "<kit>/<path>"
  "UP-Term:<drawer>/<rest>" CLONE` for new and changed files, `Delete` for files gone from it).
  No hashing on the Amiga: the CRCs come from the host.
- D7 Update run: the options come back from ENVARC:up-term/opts into ENV: (the UPTOPT files the
  parts read) and into the Installer's variables; no question page; each part in the install
  order runs only when ENV:UPTUPD<part> is set: step parts with P_STEP, copy parts by executing
  their T:UPTUPD script under a working page. The bar counts only the update's KB. Then D4.
- D8 Files gone from a step part: the part runs again (its input set changed); what an older kit
  put outside UP-Term: and this one no longer installs stays (Uninstall removes by name).

## Checklist

- [ ] U1 dist/parts.txt + host test (every kit file claimed once; copy parts match the scripts)
- [ ] U2 tools/mkmanifest.py + `make dist` writes Files/MANIFEST (D1, D3); host test of the format
- [ ] U3 install/updiff.c core + tests/test_updiff.c in vttest_host (`make test`): new, changed,
      gone files, changed part sections, KB per part, scripts per copy part
- [ ] U4 install/upupdate.c (vbcc) + Makefile + kit (Files/upupdate)
- [ ] U5 install.dos: the record (MANIFEST, opts) at the end of a full install (D4)
- [ ] U6 Install.installer: detection page (D5), the update run (D7), Installer 43.3 language
      (test_dist_installer), page text <= 56 characters
- [ ] U7 Uninstall leaves nothing of it (UP-Term:MANIFEST goes with the drawer, opts with
      ENVARC:up-term): install_rig/installer_rig leftover checks
- [ ] U8 installer_rig case "update": previous kit installed, new kit with one changed file:
      only that file's date changes in UP-Term: (List ALL dates before/after), the page flow is
      detection -> update -> done, time recorded, S:User-Startup and ENVARC:up-term/opts
      unchanged, then Uninstall removes everything. On 3.1 (43.3) and 3.2 (47).
- [ ] U9 the reachability test: the update case drives the real Installer from the kit's
      Install icon and proves upupdate ran (its ENV:UPTUPDKB) -- that is U8
- [ ] U10 README.txt: Update

## Verification

Automated: `make test ONLY=installer`, vttest_host (updiff), installer_rig update on both rigs.
Manual (owner): an Update on the Replay (3.2) over the kit installed today.
