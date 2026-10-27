---
date: 2026-10-27
topic: Amarchy merged Packages group (Browse, Search, Installed, Upgrades, Refresh) with Install, Remove, Upgrade and Launch through the job view
status: plan (owner reviews before any code)
ledger: thoughts/spot/todos.md row AMARCHY-UHC
research: thoughts/shared/research/2026-10-27-amarchy-packages.md
builds on: thoughts/shared/plans/2026-10-27-amarchy-packages-submenu.md (Installed rows), thoughts/shared/plans/2026-10-27-amarchy-job-view.md (job: action, gui_run_job)
---
# Amarchy Packages group

## Owner decisions (ledger section AMARCHY-UHC, 2026-10-27, not re-asked)
- Amarchy drives ONLY the upkg command line. UHC rows arrive through upkg's UHC repo; the [upkg] or [UHC] tag comes from
  tsv field 8 (src). Amarchy has no UHC catalogue parser and no UHC install runner.
- One merged Packages group: Browse, Search, Installed, Upgrades show upkg and UHC rows together.
- Search shows package rows only (Aminet/OS4Depot/WHD file search is a later, separate feature).
- The list loads on the first Packages open per boot, reloads after every install, remove or upgrade, and on an explicit
  Refresh row. Never on every open (a full upkg load costs about 3 s).
- No console window and no terminal ever opens for package work. Native Amarchy look: same rows, keys, theme and views as
  the other menus.
- Remove and Upgrade for UHC rows are upkg's job (upkg UHC plan U5); Amarchy only runs `upkg remove` and `upkg upgrade`.
- Interactive add-ons (their questions as input rows) belong to the later two-way job view (job-view plan, Deferred); this
  plan runs installs with upkg's recorded or default answers and shows rc 15 as "Needs answers".

## Design
### Existing work reused (not rebuilt)
- TSV parser: amarchy/pkgs_core.c (pk_parse, pkgs_core.h:23-24). Today it keeps only installed rows with a launch path and
  stores two arrays (name, launch; PK_MAX 128, pkgs_core.h:10-12). It is extended, not duplicated: a second parse mode
  keeps every row with all nine fields (see PG.1).
- Loader: amarchy/pkgs_dos.c pk_dos_load_rows (pkgs_dos.h:15) runs `upkg list-installed --tsv` into a T: file, reads and
  deletes it. It grows a `list --tsv` call and a once-per-boot cache; the DOS plumbing stays as is.
- Menu rows: IX_KIND_PKG, IX_PKGS_NODE "install.packages", ix_add_pkgs, ix_pkgs_due (index_core.h:29-30, 122-125;
  index_core.c:294-322, 611-624). The Installed rows already flow through these.
- Launch: lf_open_index `pkg:` branch (leaf.c:328, run_in_uterm). Launch stays this path.
- Jobs: KA_JOB, LF_JOB, lf_ui.run_job (keymap.h:81, leaf.h:41, 186), jc_* (job_core.h), gui_run_job, upkg exit code table in
  job_core. Install, remove and upgrade are `job:upkg ...` lines through this; no new runner.
- Text input: lf_ui.ask_value (leaf.h:175), used at leaf.c:117, 213, 243, 526. It is the Search input row in both styles.
- Node generation: mn_list_hook (menu_tree.c:107, set at leaf.c:177) for the action rows of a picked package.

### Menu shape
- A top-level group `packages` in dist/keymap `[menu]` (opened by the existing menu hotkey, listed beside Install):
  `packages.browse`, `packages.search`, `packages.installed`, `packages.upgrades`, `packages.refresh`, plus icons in
  dist/menu-icons (every menu node needs one). The old Install > Packages node (install.packages, dist/keymap:242) is
  removed; Installed replaces it with the same rows plus all installed packages, not only those with a launch path.
- Browse: every package from `upkg list --tsv`, grouped by tsv field 6 (section) as sub-nodes, sorted by name without case.
- Installed: rows with installed = 1. Upgrades: rows with upgradable = 1. Both are filters of the one loaded list.
- Search: an ask_value line "Search packages"; the result node lists rows whose name or description contains the text,
  case-insensitive substring. upkg has no substring search (`find` is an fnmatch glob over name and description and
  `search` is an installed-file path search, research section A), and the list is already loaded, so Amarchy filters in a
  pure function `pk_filter` (host tested) and costs no extra 3 s load. No upkg change is requested.
- Refresh: a row that runs `job:upkg update` (lists refresh, shown in the job view), then reloads the cached list.
- Row label: `[upkg] name  version  state` or `[UHC] ...`; state words: Installed, Upgrade to <v> (field 4 gives the flag; the
  version shown is field 2), or empty. Full English words, no abbreviations.
- Picking a package row opens an action node of that row (mn_list_hook pattern) with only the fitting rows:
  not installed: Install; installed: Remove, Launch (only when field 7 is not empty, else omitted), Upgrade (only when
  upgradable = 1). Install is `job:upkg install <name>` for src upkg and `job:upkg install uhc:<name>` for src uhc (bare name
  means ipk, ledger UPKG-AMARCHY-API); Remove `job:upkg remove <name>`; Upgrade `job:upkg upgrade <name>`; Launch is the existing
  `pkg:` path. Upgrades also offers one "Upgrade all" row (`job:upkg upgrade`, same as install.upgrade).
- Names are checked before they reach the command line (letters, digits, `.-+_`, as upkg names): a name that fails is not
  offered, so no shell metacharacter can reach `vsh -c`.

### Cache and reload
- One cache in the index struct: the full pk_rows array, filled on the first entry into any `packages.*` node per boot
  (ix_pkgs_due widens from the single IX_PKGS_NODE to the group). After a package job ends (AMARCHY_JOB_DONE seen with rc 0 or 5)
  the cache is marked stale and reloaded when the menu next returns to a packages node. A failed job (rc other than 0 or 5)
  also reloads, because a half-done install changes state. Refresh does the same.
- The list grows past PK_MAX 128: rows are heap allocated, counted, capped at a stated limit (4000) with a footer row
  "More packages than the list can show" instead of silent truncation.
- A failing or missing upkg gives one inert row with the reason (as the Installed rows do today: IX_PKG_NONE).
- One job at a time (job view rule): a package pick while a job runs is refused with the existing refused() message.

### Sentinels
- AMARCHY_PKGS_ROWS: rows loaded (published like AMARCHY_DRAWS, gui.c:538). AMARCHY_PKGS_LOADS: number of upkg list loads
  (proves "first open only" and "reload after job"). The job sentinels (AMARCHY_JOB_LINES, AMARCHY_JOB_DONE) already exist.

## Checklist
- [ ] PG.1 pure rows core: extend amarchy/pkgs_core.c/.h with `pk_rows` (all nine fields kept, heap, any installed state),
  `pk_parse_all`, `pk_by_section`, `pk_filter` (substring over name and description, case-insensitive, empty text = all),
  `pk_state` (installed, upgradable words), `pk_action_line` (the job line per action and src, with the name check). Host test
  tests/test_pkgs.c: mixed upkg and uhc rows, empty launch, CRLF, short lines, over-limit list, name with a metacharacter
  refused, `uhc:` prefix only for src uhc, filter case and description hits.
- [ ] PG.2 keymap and menu tree: dist/keymap `packages.*` group, dist/menu-icons, remove `install.packages`; leaf.c action node
  for a picked row (mn_list_hook), refusal while a job runs. Host test tests/test_keymap.c: group lists the five rows, a
  not-installed row offers Install only, an upgradable installed row offers Remove, Launch, Upgrade, a row with empty launch
  has no Launch, install.packages is gone and every node has an icon.
- [ ] PG.3 index and lazy load: index_core.c/.h widen ix_pkgs_due to the group, add the section sub-nodes, stale flag and the
  reload rule; amarchy/pkgs_dos.c loads `list --tsv` once and reloads on the stale flag. Host test tests/test_keymap.c (index
  cases): first entry loads once, second entry does not, stale flag reloads once, Installed and Upgrades are filters of one load
  (call count of the loader fake = 1).
- [ ] PG.4 Search input in both styles through lf_ui.ask_value (leaf.c): row `packages.search` asks, builds the result node
  with pk_filter; empty answer or Escape returns to the group. Host test tests/test_keymap.c with a fake ui: typed text gives
  exactly the matching rows, Escape gives none, no extra list load (loader count unchanged).
- [ ] PG.5 jobs: Install, Remove, Upgrade and Upgrade all resolve to `job:` lines and run through lf_ui.run_job (drawn:
  gui_run_job). After the job ends the stale flag is set from the footer state (rc 0, 5, other). Host test tests/test_job.c plus
  test_keymap.c: every package action ends in LF_JOB with the PG.1 line, never LF_WINDOW; a fake run_job returning rc 0, 5 and 12
  all mark the list stale; a style with no run_job falls back to run: (lf_job_view) and is stated in the row's refusal text.
- [ ] PG.6 text menu (menu.c, libupkgui): the same group, rows, Search and actions through the shared leaf code. The job view for
  this style is job-view plan RJ.2t (blocked on UPKGUI-JOB-API); until it lands, picks in the text menu run as run: in a window
  (the lf_job_view fallback). Host test: tests/test_keymap.c rows identical in both styles (one node walk, two ui structs).
  This item is not ticked until the text menu shows the job view, so it stays open while RJ.2t is blocked.
- [ ] PG.7 sentinels AMARCHY_PKGS_ROWS and AMARCHY_PKGS_LOADS in gui.c and menu.c next to the job sentinels; host test that the
  publish function receives rows and load counts from a fake load.
- [ ] PG.8 rig reachability (rig 7, 060, both styles; started with `UPTERM_RIG=7 rig.py start --max --purpose "Amarchy
  session: Packages group"`; against a local feed only, never the real feed): hotkey, Packages, Browse lists the feed's packages
  with [upkg] and [UHC] tags, Search "<word>" lists only matches, Install on a row shows the job view with upkg lines and Done,
  the row now reads Installed, Remove returns it to not installed, Upgrades lists a seeded upgradable package, Refresh reloads;
  AMARCHY_PKGS_LOADS is 1 after three opens and 2 after one install; a failing job (unknown package, rc 12) shows Failed with its
  word; no console or terminal window opened (screen text check, windows list). Numbers asserted: rows == feed size, loads as stated.
- [ ] PG.9 docs: dist/README.txt AMARCHY section (the Packages group, the five rows, the action rows, that nothing opens a
  terminal, Refresh, the [upkg]/[UHC] tag, upkg and its UHC repo as the only back end), dist/keymap comment for the group.
  Test: tests/test_keymap.c reads the shipped dist/keymap and checks the group; docs verified by grep of each row label.

## Deferred
- Interactive add-on questions drawn as input rows (needs the two-way job view, job-view plan Deferred, and `upkg questions`
  already landed upstream); rc 15 shows "Needs answers: run it from a terminal" until then.
- Aminet, OS4Depot and WHD file search (uhcsearch), a later separate feature.
- Package details view (`upkg info`) and a dependency list.
- Launch of uninstalled rows (field 7 empty until upkg TSV-LAUNCH lands; those rows just omit Launch).

## Dependencies
- upkg U1-U6 (UHC rows, search, install, remove, upgrade) and TSV-LAUNCH on the UP-Term side; the plan works with upkg rows only
  until they land, UHC rows appear as soon as upkg emits them. m68k upkg md5 c5c38bbb7f48dd3e9765a608bad5afbc or newer.

## Results
(open)

## Open decisions for the owner
1. Where does Installed live? Recommended: the new top-level Packages group only, and the old Install > Packages node is
   removed (one place, no duplicates). Alternative: keep Install > Packages as a second entry. Trade-off: removing it changes
   a path you may already use; keeping it duplicates the Installed list.
2. Does Remove ask for confirmation? Recommended: yes, one confirm row ("Remove <name>: Yes / No") before the job starts,
   because Remove deletes files. Alternative: Remove runs at once like Install. Trade-off: one extra keypress versus an
   accidental removal from a mistyped pick.
3. Should "Upgrade all" sit in the Upgrades node? Recommended: yes, as the first row, same command as Install > Upgrade all.
   Alternative: only per-package Upgrade there. Trade-off: one more way to upgrade everything versus a shorter list.
4. Text menu before RJ.2t lands: Recommended: ship the group in both styles at once and let text-menu package jobs run in a
   window as run: until the job view exists (breaks "no terminal" for that style temporarily). Alternative: ship the group in
   the drawn menu only and add the text menu when libupkgui has the job view. Trade-off: feature in both styles now versus
   keeping the no-terminal rule strictly true.
