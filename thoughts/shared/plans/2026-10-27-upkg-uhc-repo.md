---
date: 2026-10-27
topic: UHC as a second repository in upkg (list, search, browse, install through UHC)
status: decided, not started
---

# UHC as a second upkg repository

Owner request (2026-10-27): "i want to search browse and install uhc packages via uhc from upkg as if it was another repo connected to upkg".
Research: thoughts/shared/research/2026-10-27-upkg-uhc-repo.md.

## Facts the design rests on
- Catalogue = local drawer UP-Term:uhc/Packages, 12 entries (script `<n>` + `<n>.desc`). No remote index exists; it changes only when the up-uhc package changes.
- Install = `Execute UP-Term:uhc/Packages/<n>`; script ends `SetResult $UHCRC` (0 ok, 20 fail). `pkg install` hides the rc, so upkg calls the script directly. Only prompt: "already installed, proceed anyway (y/n)?" (Ask).
- Installed record = UP-Term:uhc/Packages/Installed/<n> holding the version.
- UHC has no uninstall.
- Scripts need ENV:UHC (UHCBIN, TEMPDIR, AMINETMIRROR); on the A1200 only ENVARC:UHC is set, so ENV:UHC is empty after boot. That is a bug in the up-uhc add-on install and blocks every UHC install.
- upkg rows (patch 0029 upkg_rows.c) carry no source; upkg_rows_run dispatches through opkg_cmd_exec. Patch 0040 adds a shell-out runner (xsystem after ui_suspend) to reuse.

## Steps (U1-U8)
- U1 up-uhc add-on: ENV:UHC populated at install and at boot (copy ENVARC:UHC), so UHC scripts run. Test: dist addon test + rig check `GetEnv UHC/UHCBIN`.
- U2 libupkg UHC source: read the catalogue drawer (name, PKGVERSION, desc) and Installed/ records into rows with src=uhc. Unit test on a fixture drawer.
- U3 rows: `src` field in upkg_row_t; upkg_rows_load appends UHC rows; rows_test covers merge and name clash.
- U4 install runner: UHC rows install via xsystem `Execute UP-Term:uhc/Packages/<n>` with the Ask answered, rc from UHCRC, output into the sink box; rows reload after.
- U5 remove (D2): parse Copy lines of the script, delete those files and Installed/<n>; refuse when the parse finds nothing.
- U6 CLI: `upkg list`, `upkg list-installed`, `upkg info`, `upkg install` and name search (`upkg list '*x*'`) include UHC rows (hooks in upkg_list_find and opkg_install_cmd).
- U7 browse UI: source column/tag, footer hints unchanged otherwise; test_chrome.
- U8 Reachability: rig 8 test drives `upkg browse`, finds a UHC row, installs it through UHC (sentinel: Installed/<n> written and the program runs), plus `upkg install` CLI once.

## Decisions (owner)
Owner answers 2026-10-27:
- D1 Native UHC source: upkg reads UP-Term:uhc/Packages and installs by running the UHC script. One install record (UHC's Installed/<n>). No ipk wrapping.
- D2 upkg removes UHC packages: delete the files named in the script's Copy lines plus Installed/<n>. A test parses all 12 shipped scripts.
- D3 Name clash: both rows shown with a source tag. CLI `upkg install <n>` picks the ipk; `upkg install uhc:<n>` picks UHC.
- D4 uhcupdate and uhccheck disabled (print "use upkg upgrade"). New catalogue arrives as a new up-uhc version in the feed. Goes with U1.
