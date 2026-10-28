# S11 UHC text-style package cases (rig 4, pg8-text2)

Start drift [WARN]: 17 files in VTC: differ from the build (Apps/bin/Amarchy, Claude, UPConsole, amarchy-gui, amarchy-index, amarchy-menu, hl, mdv, pty-handler, rz, sz, upgetty, uptelnet, vsh, vtcon-handler, gg/bin/sh); `rig.py install` fixed it. The menu rig's stale guard then refused a row twice (5 files: hl, mdv, rz, sz, vsh differ after restaging) until `rig.py install` was run again right before the row.

"Real UHC" on the rig = the staged SYS:UHC/Packages catalogue (gui rig uhc_catalogue, one stub pg8uhc); the rig has no UHC add-on (no UHC: assign).

| case | verdict | evidence | rc |
|---|---|---|---|
| pkg_text (tags, Refresh, Remove No/Yes, Upgrades, no-console; feed packages) | PASS | "passed 1 of 1", 0 FAIL | 0 |
| pkg_text_search (Browse [upkg]/[UHC] tags, Search bitchx / pg8uhc) | PASS | `Search 'pg8uhc' lists only matches ['[UHC] pg8uhc 1.0 >']` | - |
| pkg_text_fail | PASS after rig fix | fail: rc 13 'Network error'; notfound: rc 12 'Package not found' | 13, 12 |
| pkg_text_uhc (NEW: UHC Remove) | FAIL (rig setup, not product) | Install step rc 14: `UHC:C: device (or volume) is not mounted ... upkg: the script of uhc:pg8uhc returned 10` | 14 |

pkg_text_fail was failing before my change: the feed has grown past the 10-row text window, so `tx_install_row` looked for up-zzfail only in the window. Fix: tx_install_row collects tx_all_rows when the window lacks the name (host test added). The notfound leg also reloaded the list on reopen (the menu loads on open, so the ghost vanished); it now keeps the menu open while `upkg update` drops the ghost.

UHC Remove, by hand on rig 4 with the staged stub: `upkg remove uhc:pg8uhc` is parsed with the prefix (rc 20 "cannot tell which files pg8uhc installed (its script has no Copy line ...); nothing removed"), i.e. the uhc: path is reached, which is what 1d10fc2d fixes (was `remove pipe`-style rc 5 not installed). A full PASS needs the stub to be installable: an `Assign UHC: SYS:UHC` with a `C` drawer and a stub script with a `Copy ... TO <file>` line, so Install marks it Installed and Remove has a file to delete. Open: extend gui rig uhc_catalogue (shared with --pkg-tags) or stage that inside r_pkg_text_uhc_remove.

## Reopen after Refresh

Run uhc3 (earlier): Refresh job rc 0, then "the text menu opens over the terminal" failed (3 x x9_open_menu with tx_leave_menu between, 15 s each). Run uhc4 (rig 4 fresh start, same code, HEAD 0873704a): pkg_text_uhc passed 1 of 1, 0 failed checks, including the reopen after Refresh, the UHC Install, Remove (`remove uhc:pg8uhc`), and the reopened row no longer Installed.

Verdict: not reproducible; no harness or Amarchy fault shown. tx_browse reopens exactly as pkg_text_search_tags does (tx_job closes the job view, tx_leave_menu sends 5 Esc, tx_open_packages retries the toggle 3 times). The failing run had been started on a rig that was not freshly booted (the uhc3 log predates this rig start); the likely cause is a stale or still-closing menu process holding the Super+Space toggle after the job, but this was not observed, so it is unverified. No code change made. If it recurs, capture `ami.py windows` and `requesters` at the moment of failure before re-running.
