---
date: 2026-10-23
topic: Amarchy app index (AM-IDX)
status: reviewed by owner, ready to implement
---

# Amarchy app index

Owner 2026-10-23: "how will it index all amiga apps... setups are different", then "we need it to scan for
the apps" before the 1.0 release. Design decisions were made by the owner on 2026-10-23 (todos.md AM-IDX):
cached index, all hard-disk volumes, Apps level plus a root filter.

## Design

- New native tool `amarchy-index` (C, built next to Amarchy in the Makefile). Amarchy itself never scans.
- Volumes: LockDosList(LDF_VOLUMES|LDF_READ). Keep hard-disk handlers. Skip RAM:, trackdisk floppies (DF0-3)
  and CD handlers. Never assume DH0:/DH1: names.
- An app is any `.info` of type WBTOOL found by a recursive Examine/ExNext walk, depth cap 6.
  Skip drawers Devs, Libs, L, Fonts, Locale, Env, T, Prefs/Env-Archive, Trashcan.
- WHDLoad installs: WBPROJECT icons whose default tool is WHDLoad are indexed too and launched via the icon. They go to a
  separate Games level (cache rows carry a kind field), so a big WHDLoad set does not bury the Apps list.
- Skip list: upconf key `amarchy-index-skip = <paths>` (comma separated, prefix match).
- Cache: ENVARC:Amarchy/apps, one row per entry: `kind<TAB>label<TAB>path` (kind `app` or `game`). Written to a temp file, then renamed, so a
  reader never sees half a file.
- When the scan runs: at boot in the background (installer adds `Run >NIL: amarchy-index` to the user-startup
  block that starts Amarchy), and from a "Rescan apps" menu row. Never at menu open.
- Menu: the Apps level lists keymap `[menu]` rows plus index rows, sorted by label. A `[menu]` row wins over an
  index row with the same label. Duplicate index labels show the volume as a dim suffix.
- Root filter: typing at the root level also searches the index rows (apps and games).
- Launch like Workbench: OpenWorkbenchObject (workbench.library V44) on the path, else a WBStartup message.
  Never system().
- Pure parts live in host-testable files (row parse, sort, dedupe, skip match, label from file name), following
  menu_core.c / menu_tree.c and the keymap suite.

## Owner review 2026-10-23

Answered through the question tool: scan at boot plus the Rescan row; depth cap 6; WHDLoad games indexed
under their own Games level; the root search covers all index rows. Temp file plus rename and the skip-drawer
list are implementation defaults.

## Checklist

- [x] X1 (58edb0fc) Pure index core (parse, sort, dedupe with volume suffix, skip-list prefix match, `[menu]` wins) plus host tests in the keymap suite.
- [x] X2 (18ff3482) `amarchy-index` scanner: volume filter, recursive walk with depth cap, skip drawers, WBTOOL and WHDLoad projects, temp-file plus rename. Makefile target; part of make dist.
- [x] X3 (6432d059) Amarchy Apps level reads the cache and merges app rows; a Games level lists game rows (text menu and drawn menu both, through the shared tree code).
- [x] X4 (6432d059) Root filter searches index rows.
- [x] X5 (db140a79, 2c0ed178) Launch through OpenWorkbenchObject, WBStartup fallback; no system().
- [x] X6 (db140a79, 2c0ed178) "Rescan apps" row runs the scanner in the background; the next menu open shows the new rows.
- [x] X7 (ae767f38) Boot scan: installer adds the Run line; upconf key `amarchy-index-skip` documented and read.
- [x] X8 (ae767f38) Docs: Amarchy guide section on the app index, skip key, rescan row.
- [x] X9 (7560a029, aa0a0f89 on am-idx-icon before rebase; 19/19 twice on rig 3, 2026-10-24) Reachability rig case (amarchy rig): put a fake WBTOOL app in a drawer on the rig hard disk, run the scanner, open the menu, Apps level shows the row, launching it runs the tool (sentinel file written by the tool). Also: a fake WHDLoad project icon shows under Games, not Apps; a drawer in the skip list is not indexed; a `[menu]` row with the same label wins.
- [x] X10 (ix_level returning keymap rows only: case fails at "3 Apps level lists the row", 2026-10-24) Revert-check of X9: with X3 reverted the rig case fails.
- [x] X11 Owner A1200 check (2026-10-25 PASS, owner): the Apps level lists the owner's real apps with icons, launch works, the menu opens in under 1 s.

Done = X1-X11 ticked with commits; make test, make dist and make dist-check pass.

## Follow-up: app icons (AM-IDX-ICON), owner decision 2026-10-24

- The drawn menu (amarchy-gui) shows each index row with the app's own `.info` icon, colour-mapped to the
  screen and scaled to the row height. Icons load only for visible rows and are cached per path. A row
  whose icon fails to load gets the generic glyph. The text menu stays text only.
- Ships in 1.0, after am-idx is merged, as its own change with its own rig case.

- [x] I1 (b7ac9164) Icon load (GetDiskObject or GetIconTagList with screen remap) plus scale to row height, cached per path, loaded for visible rows only.
- [x] I2 (b7ac9164, d913b0ef sentinel; host only, rig proof is I3) Drawn menu Apps and Games levels draw the icon; fallback glyph on failure.
- [x] I3 (2a86726b mask fix, 5285f4ad rig case app_icons; 10/10 on OS 3.2 rig 5, revert of ig_draw fails it; app_index 19/19 twice on default rig 3) Rig case: the fake WBTOOL app's icon appears in its row (pixel check), a row without an icon shows the fallback; revert-check fails.
- [x] I4 Owner A1200 check (2026-10-25 PASS, owner): real icons in Apps, menu open under 1 s.
