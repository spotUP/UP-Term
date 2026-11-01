---
date: 2026-10-26
topic: Launch installed packages from `upkg browse` (Enter on an installed row)
status: decided, not started
---

# Launch installed packages from upkg browse

## Owner decisions (2026-10-26)
- Enter on an installed row = Launch. Enter on a not-installed row = Install (as now).
- Remove moves to the Delete key, shown in the footer hints ("Delete: Remove").
- Launch target comes from a new `launch` column in dist/packages.txt (vtcon), carried into the .ipk control file as a `Launch:` field and into Packages. Packages without one (git, perl, libraries) show no Launch hint; Enter on them does nothing new.
- Terminal browse first: pause curses, run the program in the same console, resume browse on exit. The Workbench upkg window (ports branch upkg/wb-ui, gated on the kit ixemul) comes later.

## Where the code is
- ports pkgs/opkg: `upkg browse` (rows: upkg_browse_list, pkgs/opkg/check/rows_test.c).
- ports pkgs/upkg-ui/src: libupkgui (ui.c, chrome.c; footer hints, test_chrome.c).
- vtcon dist/packages.txt (the table every tool reads, tools/pkgs.py) and the ipk control writer (tests/test_dist_ipk.py).

## Steps (L1-L7)
- L1 packages.txt `launch` column + tools/pkgs.py parse + tests/test_dist_versions/test_dist_ipk fixtures.
- L2 ipk control `Launch:` field and Packages index carry it (test_dist_ipk).
- L3 upkg reads `Launch:` from the installed status/control for a row.
- L4 browse: Enter on installed + Launch set -> endwin, run, wait, refresh; Delete -> remove path; footer hints updated (test_chrome).
- L5 Launch runs with the package's stack cookie / __stack (stack rule) and returns the exit code to a status line.
- L6 Reachability: rig test drives `upkg browse` on rig 8, Enter on an installed package with Launch, proves the program ran (sentinel output), returns to browse; Delete removes.
- L7 Fill launch values for app rows (nvim, vim, python, pi, claude, ...), rebuild feed, publish (owner approves the release).
