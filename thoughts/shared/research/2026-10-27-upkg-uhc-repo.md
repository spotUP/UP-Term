---
date: 2026-10-27
topic: UHC as a second repository in upkg (search, browse, install through UHC)
status: research
---

# UHC catalogue as a second upkg source: facts

Sources: A1200 `UP-Term:uhc/...` (read-only), vtcon origin/main `dist/addons/uhc/*` (92af1088), upterm-ports origin/main patches
(0029 upkg_rows, 0040 on branch feat/browse-launch commit 2e8e6b5) and the patched opkg source `upterm-ports/build/src/opkg/libopkg`
(checkout is at patch 0027; line numbers below are from that tree, the 0029/0040 code from the patch files).

## 1. How UHC installs a catalogue package
- Front end: `UP-Term:uhc/S/pkg install <name>` (`.KEY OPTION,WORD2`, FailAt 21). It checks `${UHCBIN}Packages/<name>` exists
  ("Package not found." then Done, rc 0!), asks `Ask "<name> already installed, proceed anyway (y/n)?"` if
  `${UHCBIN}Packages/Installed/<name>` exists (the only interactive prompt; piping/no console would hang), then
  `Execute "${UHCBIN}Packages/<name>"`; `IF WARN` prints "Installation failed." (pkg itself still exits 0 after SKIP Done).
- The catalogue entry IS the installer: `Packages/<name>` is an AmigaDOS script (unzip example): `Set PKGNAME/PKGVERSION/PKGURLPATH/
  PKGFILENAME`; `CPU CHECK 68020` (rc 20 on fail, `Set UHCRC 20`); `pkg-envcheck` (creates ENV:UHC/EXTBIN, EXTDOC, AMINETMIRROR defaults,
  `MakeDir UP-Term:uhc/ExtBin` and `ExtDoc`, copies to ENVARC:UHC); `MakeDir ${UHC/TEMPDIR}${PKGNAME}`; `pushd` there;
  `aget "${PKGURLPATH}${PKGFILENAME}" QUIET` (PKGURLPATH is a host path without scheme: `${UHC/AMINETMIRROR}util/arc/` =
  `de.aminet.net/aminet/util/arc/`, or `driar.se/amigafiles/` for curl); `lha e` the needed members; `Copy` binaries to `${UHC/EXTBIN}`
  (UP-Term:uhc/ExtBin/) and docs to `${UHC/EXTDOC}`; delete temp dir; `Echo "${PKGVERSION}" >UHCBIN:Packages/Installed/<name>`.
- Return code: each script sets `UHCRC` (0 ok, 20 failure) and ends `LAB CleanUp ... SetResult $UHCRC` (S/pkg/Packages/unzip tail).
  `Execute` of the script gives WARN to pkg on failure; `pkg` does NOT propagate it (exits 0). Running the script directly
  (`Execute UP-Term:uhc/Packages/<name>`) gives the real result: this is the one to call from upkg, plus the Ask is avoided.
- The generic downloader used by the UHC searchers is `S/uhcget <number> <searcher> <baseurl> [dest]` (RC 10 on failure) - not used by
  catalogue packages. `C/aget` returns >=20 local error, >=10 network error (S/uhcmirrorget).
- Interaction: none except the "already installed" Ask. No requesters seen in the unzip/curl/lzx scripts. Needs ENV:UHC/TEMPDIR,
  EXTBIN, AMINETMIRROR set. On the A1200 `ls ENV:UHC` was EMPTY and ENVARC:UHC holds AMINETMIRROR READER SEARCHURL SHOWARCHIVER SHOWPROGRESS
  TEMPDIR (no EXTBIN/EXTDOC yet): ENV needs the ENVARC copy (boot does it; a live rig may not) and `UHCBIN` env must be set (our
  Install.dos/install.dos set `SetEnv UHCBIN UP-Term:uhc`, Assign UHC:, Path UHC:C UHC:S).
- Needs `lha` and `aget` in the path; packages use their own tools (ExtBin) so the result lands in UP-Term:uhc/ExtBin (not under upkg's
  MANIFEST).

## 2. Installed record and removal
- Record: file `UP-Term:uhc/Packages/Installed/<name>` containing the version string (one line). The drawer does not exist on the
  A1200 yet (`ls` rc 2): nothing installed. `pkg installed` lists those files (`Type` of each = version).
- Version compare: the version text in `Packages/<name>` (`Set PKGVERSION`) vs the Installed file: same file format, so "upgradable" = strings differ.
- Removal: UHC has NO uninstall (`pkg` options are only install, installed, list, search, update; S/pkg). Upkg would need its own:
  delete `Packages/Installed/<name>` and the files the script copied (unzip: UnZip, README, README.AMI, unzip.doc in ExtBin/ExtDoc). The
  script has no file list; the member list sits in each script's `lha e` / `Copy` lines. Open design decision.

## 3. Catalogue source and update
- Catalogue = the local drawer `UP-Term:uhc/Packages` of the uhctools_os3.lha archive (vtcon add-on archives.txt: sha256 b4570477...,
  https://uhc.megaburken.net/uhctools_os3.lha). 11 packages (the one extra is lzx): amigae curl diskwiz lzx pipe split system ttcp
  unrar unzip xdms, each `<name>` + `<name>.desc`. `S/pkg update` merely rebuilds the local `UP-Term:uhc/packages.cache`
  (`name` padded to 20 + .desc) from that drawer: `pkg list`/`pkg search` read the cache. No remote index.
- Remote: nothing machine-readable; the site root lists only aget, uhcinstall, UHCTools.lha, uhctools_log (curl of
  uhc.megaburken.net/ ; /Packages/ is 404). New packages only arrive with a new whole archive. `S/uhccheck` fetches
  `<mirror>/uhctools_revision` (git sha; local `UP-Term:uhc/revision` = 43cc59c0...) and `S/uhcupdate` downloads
  `uhctools_<os>.lha` and unpacks over UHC: (rewrites files, so it fights upkg's MANIFEST). In our add-on the scripts are shipped
  unmodified (Install.installer text: "unmodified from their author"); the earlier decision (todos 2050) to disable them is not in
  dist/addons/uhc yet (nothing there touches uhcupdate; grep of origin/main dist/tests found no reference). So there is a remote
  freshness signal (uhctools_revision) but no remote catalogue index; the catalogue changes only with the up-uhc ipk.
- Mirrors: UHCMirrorList: uhc.megaburken.net, uhc.driar.se, uhc.a1k.org, uhc.no0b.de, uhc.hex9d.com.

## 4. opkg/upkg: feeds, list/search, install, browse rows
- Feeds: `UP-Term:etc/upkg.conf`, vtcon dist/upkg.conf:6 `src/gz upterm https://github.com/spotUP/vtcon/releases/latest/download`.
  Parsed in libopkg/opkg_conf.c:466-476 (`src`, `src/gz` -> pkg_src_list_append). Fetched per src by `opkg_update_cmd`
  (opkg_cmd.c:114) into lists_dir; loaded by pkg_hash_load_feeds (all of this is ipk-only: a Packages file per src).
- list / find: `upkg_list_find` (opkg_cmd.c:634, used by opkg_list_cmd:671, find:682, list-new:677). `search` (opkg_cmd.c:1333) is
  opkg's FILE search (which package owns a file), not a name search; name search = `upkg list '*glob*'` / `find`.
- Browse rows: `upkg_browse_list(which)` (opkg_cmd.c:895) prints list / list-installed / list-upgradable / list-new as text lines
  `NAME - VERSION - DESC` into memory; patch 0029 (libopkg/upkg_rows.c/.h) parses them in `upkg_rows_parse`/`upkg_rows_load` into
  `upkg_row_t {name, kind (AVAILABLE|NEW|INSTALLED|UPGRADE), installed, available, desc}` (+ `launch` from patch 0040).
  Browse (upkg_browse.c) renders `upkg_rows_status`. Install/remove/upgrade run via `upkg_rows_run(rows, cmd, name, sink)` which
  calls `opkg_cmd_exec(opkg_cmd_find(cmd), 1, {name})`, then `hash_reload()` and `upkg_rows_load` (0029).
  The Workbench window (0031-0039) uses the same rows + upkg_rows_run.
- Source/repo per row: NO. The row has no source field; internally pkg_t has `pkg->src` (pkg_src_t name/value) but the list text and
  upkg_row_t drop it. A repo/source column would be new.
- Cleanest hook (one place for browse, WB window and `upkg list`): add a second row provider in upkg_rows.c:
  1. `upkg_rows_load()` (0029): after `upkg_rows_parse` of the ipk lists, append UHC rows (kind from Packages/Installed + version
     compare), tag them with a new `src` field (`"uhc"`), sort/merge by name.
  2. `upkg_rows_run()` (0029): if the row is a UHC row dispatch to a UHC runner (below) instead of `opkg_cmd_exec`; skip `hash_reload`.
  3. For CLI parity, `upkg list/find/install/remove` need the same provider: either a small `upkg_uhc.c` called from
     `upkg_list_find` (opkg_cmd.c:634) and `opkg_install_cmd` (opkg_cmd.c:429, before opkg_solver_install; a name not found in the hash
     and present in the UHC catalogue goes to the UHC runner), or a pseudo-feed. Rows alone (no CLI) would leave `upkg install amigae` unknown.
  Alternative: ship UHC as a real ipk feed (one ipk per catalogue entry whose postinst runs `Execute UHC Packages/<n>`); then every opkg
  path works unchanged, row shows installed state via status file; downside: a generated feed to maintain and a second install record.

## 5. Reusable shell-out path (browse Launch, patch 0040, branch feat/browse-launch commit 2e8e6b5)
- `upkg_rows_launch_run(rows, name, line)` runs `sh -c LINE` through `xsystem(argv)` (vfork + exec; ixemul execve gives the program its
  $STACK cookie; patch 0014 also gives vfork children a 262144 byte stack), in the console the caller left curses for
  (`ui_suspend/ui_resume` as in run_action in upkg_browse.c:342-373), then reloads rows. `upkg_rows_launch_status(name, rc)` makes the
  "NAME exited with code N" status line; `upkg_rows_launch` reads `Launch:` from `<info_dir>/NAME.control`.
  An install-via-UHC runner can call `xsystem` with `Execute UP-Term:uhc/Packages/<name>` (needs UHCBIN/ENV:UHC set in the child env;
  check ENV:UHC/TEMPDIR first) and capture the rc, using the box sink (`upkg_set_sink`, upkg_browse.c:295-314) if the script's Echo output
  should show in the program box (xsystem children's stdout goes through the sink via upkg_wrap per patches 0009/0010/0024).
- Note: main of upterm-ports does not have 0040 yet (only branch feat/browse-launch); rows_test.c on that branch covers it.

## Gaps and risks for design
- pkg install returns 0 on failure and has the Ask: call `Execute Packages/<n>` directly and check `UHCRC` via rc.
- Dependencies on `lha`, `aget`, network (aminet via HTTP), 68020 check; Aminet/driar.se hosts third-party archives (licensing is UHC's
  own catalogue, we only invoke it).
- UHC removal does not exist; version "upgrade" has no source of truth beyond the bundled script (changes only with a new up-uhc ipk).
- `Installed/` state lives inside UP-Term:uhc, which up-uhc's Uninstall deletes whole (uninstall-body.dos), so records vanish with it.
