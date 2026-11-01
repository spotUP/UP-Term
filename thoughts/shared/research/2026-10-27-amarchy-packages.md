---
date: 2026-10-27
topic: Packages inside Amarchy (upkg + UHC Tools behind one drawn menu group)
status: research
---

# Packages inside Amarchy: research

Decisions already taken: thoughts/spot/todos.md:2788 `## AMARCHY-UHC` (one merged Packages group, rows tagged [upkg]/[UHC],
no terminal or console, output captured and drawn by Amarchy, UHC driven through its own scripts, UHC is not an upkg source).
Evidence paths: upkg source = /Users/spot/Code/UP-Term/upterm-ports/build/src/opkg (patched tree, libopkg/*); UHC = vendor/uhc/uhctools_os3.lha
unpacked to the scratchpad `uhc/` folder (…/scratchpad/uhc, referred to as UHC/ below). The A1200 was NOT touched (local copy was complete;
the real installed state under UHC:Packages/Installed on the A1200 is therefore unverified).

## A. upkg

Base is opkg 0.9.0 with 27 patches (upterm-ports/pkgs/opkg/recipe.mk:1-60). Binary UP-Term:bin/upkg, stack 262144 via __stack (recipe.mk:67-71).

Command table: libopkg/opkg_cmd.c:1409-1474. Relevant commands and outputs:

| command | output | exit | evidence |
|---|---|---|---|
| `update` | progress lines per feed, writes lists/<feed> and lists/<feed>.prev | 0 / nonzero | opkg_cmd.c:1410; patch 0023 |
| `list [glob]` | one row per available package: `name - version - description`, sorted by name; glob matches name only; no match: stderr `upkg: no package matches "<glob>"`, rc 1 | 0/1 | opkg_cmd.c:59-90 (print_pkg), 626-668 |
| `find <pat>` | same rows, matches name OR description (fnmatch glob, not "search") | 0 | opkg_cmd.c:671-686 (use_desc=1) |
| `search <path-glob>` | NOT a package search: matches installed FILE paths, prints owning packages, error `no path found matching pattern` | nonzero if none | opkg_cmd.c:1333-1372 |
| `list-installed [glob]` | same row format; empty + terminal prints "No add-ons are installed.", through a pipe prints nothing | 0/1 | opkg_cmd.c:756-792; patch 0020 |
| `list-upgradable` | `name - old_version - new_version` per line; none: "All installed add-ons are up to date." | 0 | solvers/internal/opkg_action.c:268-300 (fprintf at +16, +24) |
| `list-new` | rows of packages the last `update` added (needs .prev; nothing on the first update) | 0 | opkg_cmd.c:677-680 |
| `info [pkg]` | opkg control stanza (Package/Version/Description/...) | 0/1 | opkg_cmd.c:935 (opkg_info_cmd) |
| `install` / `remove` / `upgrade` | human lines, line buffered through a pipe (patch 0024); errors `[ERROR]`-style via opkg_msg; `upgrade` with nothing: "All installed add-ons are up to date."; `remove` of absent: "X is not installed." rc 1; install of installed: "Package X (v) installed in root is up to date." | 0 ok, nonzero failure | patches 0021, 0024; opkg_cmd.c:1436-1440 |
| `browse` | full-screen ncurses UI (upkg_browse.c over libupkgui); refuses without a tty: "upkg browse needs a terminal. Use upkg list." rc 2 | 2 | libopkg/upkg_browse.c:468-476 |

Machine readability: there is none beyond the row format. Rows are `name - version - description`; the first " - " split is unambiguous only if
the version has no " - " (true for opkg versions). With `-size` option a size field is inserted (opkg_cmd.c:67-75). Output is wrapped to the
console width only on a terminal; piped output is unwrapped (patches 0009, 0010), so a captured pipe gives clean one-line rows. No JSON, no
tab-separated mode, no category field, no Launch field, no installed-flag in `list` (installed state needs a second call or a join).
Prompts: none in the CLI commands (install/remove/upgrade do not read stdin; browse reads Return only in its own interactive route,
upkg_browse.c:365). EXCEPTION: add-ons with questions (Files/questions, tools/pkgs.py:266-300) carry `UP-Term-Interactive: yes`
(pkgs.py:295-301); their postinst runs dist/upkg-ask (full-screen questions) and browse runs them in the plain window
(upkg_browse.c:218-260). From a captured install the postinst would block on questions with no tty: this is a gap (D).
Errors: full/absent volume does not raise a DOS requester (patch 0026: pr_WindowPtr = -1), so failure is an rc plus `[ERROR]` text.
Locking: lock_file in config; one upkg at a time.

State on the Amiga (dist/upkg.conf:7-11): `/UP-Term/pkg/upkg/` = UP-Term:pkg/upkg/ with `lists/<feed>` (the downloaded Packages text, plain
Debian control stanzas: Package, Version, Description, Filename, Size, ..., UP-Term-Interactive), `lists/<feed>.prev`, `status` (installed
stanzas, same format), `info/` (<pkg>.control/.list/scripts), `cache/` (30-hex-digit names, patch 0003), `lock`. Reading `lists/*` and `status`
directly is possible and is what upkg_browse.c does for Interactive (upkg_browse.c:218-260 scans the feed texts). It avoids the 3 s per-load cost
(patch 0022 comment: each upkg load is about 3 s on a 68k from PFS3; list-installed 1.1 s; list-upgradable was 41 s before patch 0017).
Version comparison (upgrades) would then have to be reimplemented (dpkg rules), so `list-upgradable` stays the source for Upgrades.
Feed: `src/gz upterm https://github.com/spotUP/vtcon/releases/latest/download` (dist/upkg.conf:6).

Launch field: plan thoughts/shared/plans/2026-10-26-upkg-browse-launch.md (lines 10-27): `launch` column in dist/packages.txt, `Launch:` control
field, upkg reads it from the status file (L3), run with the package stack cookie (L5). Status: NOT implemented (grep Launch in tools/pkgs.py
and dist/packages.txt: no hit; packages.txt has 31 lines and no launch column). Descriptions come from tools/pkgs.py SUMMARY (pkgs.py:314, 351);
there are no categories anywhere in dist/packages.txt or the ipk control (no Section field). Row ids: base, claude, pi, nvim, python, git, perl, vim, bitchx, vbcc, ...

## B. UHC Tools

Source of truth: UHC/ (unpacked uhctools_os3.lha, 259 files, revision 43cc59c0ba2a43587939fd9ca89e38f54f9e1e58; all 5 mirrors serve the same revision).

UHC is two different things:
1. A SEARCH front end for big file archives (Aminet, OS4Depot, AROS, MorphOS storage, WHDLoad, Fish, Modland, ...). Not a package manager.
2. `pkg` (S/pkg 0.91), a tiny package manager for 11 third-party tools in UHC/Packages (amigae, curl, diskwiz, lzx, pipe, split, system,
   ttcp, unrar, unzip, xdms).

pkg (S/pkg:1-96), the only list/install/installed implementation:
- `pkg list`: `Type UHC:packages.cache` (S/pkg:34-41); the cache is `name(20 cols)description`, built by `pkg update` from Packages/*.desc
  (S/pkg:72-85). Sample: `curl                Command line tool for transferring data with URL syntax`. Fixed-width, trivially parseable.
- `pkg installed`: for each file in Packages/Installed prints `name(20 cols)Version <contents>` (S/pkg:45-58). The state is one file per
  package, `UHC:Packages/Installed/<name>` containing the version string (Packages/curl, last lines: `Echo "${PKGVERSION}" >"${UHCBIN}Packages/Installed/${PKGNAME}"`).
- `pkg search <word>`: AmigaDOS `Search NONUM SEARCH word packages.cache` (S/pkg:62-71).
- `pkg install <name>`: runs the stub script Packages/<name> (S/pkg:9-30). Stub e.g. Packages/curl: sets PKGNAME/PKGVERSION/PKGURLPATH/PKGFILENAME
  (`driar.se/amigafiles/curl-7.10.5.lha`), CPU check, pkg-envcheck, MakeDir temp, `aget` download, `lha e`, `Copy` to ${UHC/EXTBIN},
  writes Installed/<name>. Prints progress `Fetching ... Done! Extracting ... Installing ...`; failure text + `SetResult 20` (UHCRC).
- NO remove (options: install, installed, list, search, update; S/pkg:88-95), NO upgrade, NO available-version in any index
  (PKGVERSION lives inside each stub script as a Set line, so "upgradable" means grepping PKGVERSION out of every stub and comparing with the
  Installed file; Packages/ only changes when UHC itself updates).
- Prompts: `Ask "<name> already installed, proceed anyway (y/n)?"` (S/pkg:18) when reinstalling; pkg-envcheck sets ENV defaults silently
  (S/pkg-envcheck:1-25, no prompt in the 25 lines read). Avoidable: Amarchy checks Installed/<name> itself and drops the install call or
  removes the Installed file first.

UHC self-update: `uhccheck` (S/uhccheck) fetches `<mirror>/uhctools_revision` (40 hex, sha1) and compares with UHC:revision, rc 5 when different, 0 same,
10 on mirror failure. `uhcupdate [FORCE]` (S/uhcupdate:1-105) downloads `uhctools_os3.lha` from the mirror, unpacks over UHC:, may copy itself
over S:uhcupdate (`Run >NIL: Execute T:updateuhcupdate`, S/uhcupdate:55-63), warns about aget version, runs UHC:UHCPostUpdate; prints
"You are running the latest version of UHC Tools!" or "All done!". Honour `UHC/KEEPUHCUPDATE`.

Search scripts (e.g. S/aminetsearch = `uhcmirrorsearch aminetsearch <words>`): UHCSearch (C/UHCSearch 1.19) issues an HTTP GET
`http://<host>/uhcsearch?searchtype=<aminetsearch|aminetrecent|depotsearch|...>&searchlimit=N&searchwords=<w>` (strings of the binary; hosts
from UHC/UHCSearchList). It writes results to `${UHC/TEMPDIR}<searchtype>` and prints a numbered table; `aminetget N` / `uhcget` /
`uhcextract` pick result N via S/uhcresultpath (`TypeLine file N`), download with `aget`, optionally unpack. Mirror rotation and
`ENVARC:UHC/SEARCHURL` rewrite happen inside uhcmirrorsearch (S/uhcmirrorsearch). So UHC "search" = remote Aminet/OS4Depot/WHD search, not a
list of UHC-installable packages.

Mirror measurements (curl, 2026-10-27):
- Home pages of all five mirrors (uhc.megaburken.net, .driar.se, .a1k.org, .no0b.de, .hex9d.com): HTTP 200, identical 14036-byte HTML page
  (a text/html description of the tools; no directory index, no listing). /amigafiles/ on megaburken: 404.
- `/uhctools_revision`: identical sha1 43cc59c0... on megaburken, driar, a1k, no0b, hex9d, and equal to UHC:revision in the lha: same content.
- Listing endpoint `/uhcsearch?searchtype=aminetsearch&searchlimit=3&searchwords=vim`: works on megaburken, driar, no0b (UHCSearchList has
  exactly those 4: megaburken, driar, no0b, hex9d). a1k.org and hex9d returned 404 for it (a1k is not in UHCSearchList; hex9d is listed but 404'd for this
  query, unverified why). Response is plain text (Content: header line `# Filename Path Size Description`, then `N file.lha path SIZE description`
  rows, N from 0), e.g. `1 m_100.lha  mods/misc 129K (mono) - 100% Sh*t by Vim!`; `aminetrecent` works with no words
  (`0 RSGUI.lha comm/net 53K Roadshow GUI ...`). Search with `nvim` returned only the header (zero hits).
  The row layout is column-aligned (fixed-width, filename/path/size/description) and sizes are rounded, so it is parseable but not delimiter-safe.
- Verdict: this endpoint is an Aminet/OS4Depot-style FILE search with N-th result download, NOT an index of installable UHC packages. Usable for
  a "UHC Search" row (and "Recent") but unusable as the package index for Browse/Installed/Upgrades: no versions, no installed notion, no
  upgrade data, no remove. The only upgrade signal UHC has is the revision hash for UHC Tools itself, and the 11 stubs' PKGVERSION.
  Mirrors serve the same content (revision and home page identical; searched rows identical in the two mirrors checked, megaburken
  and driar).

Prompt and window summary (grep over S/, UHCHelp, startup files): `Ask` in S/pkg:18, S/installdeps:7-8 (TestDevice, TSGUI third-party installer script);
`Installer`/`RequestChoice`/`RequestString`/`CON:` found in no script of the S/ set I grepped beyond those; UHCSetDirectory and UHCHelp/AskMore
(a pager for output) use interactive pieces (UHCHelp/AskMore, AskMore binary C/AskMore pages long output and waits for a key: must not be called).
Everything else prints to stdout and sets rc. Unverified: the 94 scripts were grepped for prompt keywords, not read one by one.

Env that UHC needs (ENV:UHC/*, UHCBIN): `UHCBIN` = install dir with trailing slash, `UHC/TEMPDIR`, `UHC/EXTBIN`, `UHC/AMINETMIRROR`, `UHC/SEARCHURL`,
`UHC/SHOWPROGRESS` (S/pkg-envcheck, S/uhcmirrorget). Scripts are AmigaDOS scripts needing C:SetFromCmd etc. from UHC:C in the Path.

## C. Amarchy (worktree prefs-a1, branch prefs-inline)

- Menu structure: the drawn menu is a separate process `amarchy-gui` (amarchy/gui.c:1-6), same CLI as amarchy-menu (`menu [node]` | `viewer`). Its rows come
  from the keymap `[menu]` table (km_menu {kind, id, label, arg}, config/keymap.h:69-90 comment) walked by mn_children_t (amarchy/menu_tree.c:152),
  plus the app index: ix_level_t (index_core.c:~600-660) adds category groups and index rows (kind `app`/`game`, TAB separated, ENVARC:Amarchy/apps,
  index_core.c:23-47; index.c:4-9). A filter typed in the menu searches every index row (index_core.c:617). Dynamic list hook already exists:
  `mn_list_hook` (menu_tree.c:107, set at leaf.c:169) gives one choice row per Theme/Profile name. So a Packages group plugs in two ways:
  (1) a static `[menu]` group `packages.*` for Search/Browse/Installed/Upgrades (kind: new KA_PACKAGES or reuse KA_SETTINGS-style node,
  keymap.h enum), and (2) rows below each node generated by a new hook like ix_level_t/mn_list_hook that reads a package cache file. Closest
  pattern to copy: the app index (kind-tagged TAB file, loader index_load.c, watcher ixwatch.c). Kinds today are only app/game (index_core.c:36-40),
  so a package row needs a new kind or a separate cache with its own parser (single source of truth: one parser in a *_core.c file, like index_core.c).
- Running a child with captured output: YES, partly. act.c:46-60 `act_run` = SystemTags(cmd, SYS_Input NIL:, SYS_Output NIL:, SYS_Asynch, NP_StackSize 32768,
  NP_Path copy); leaf.c:359-360 same with a given in/out file and 32768 stack; leaf.c:73 runs in a UP-Term window (65536 stack). Output capture to
  a T: file or PIPE: is therefore a matter of passing a different SYS_Output; nothing reads the output yet. Stack rule: NP_StackSize is
  set at each call (65536/32768); upkg has its own __stack (opkg_STACK), UHC scripts run under the Shell (set NP_StackSize 65536+ and for aget check its cookie).
- Scrolling text view: YES in amarchy-gui: `viewer` and `message` modes show lines of text with no selection and scroll_lines (gui.c:118-121, 594-600,
  1074-1100; viewer text from mn_viewer_text, gui.c:1269). A package log pane = a third text mode fed from a file; it needs a refresh timer (below).
- Wait loops: the commodity (amarchy.c:328) waits on `cxs|rexx|notify|launch|wb_reply|clipboard|CTRL_C`; picks arrive on the launch port
  (LF_LAUNCH_PORT, amarchy.c:233-250) and a program is started async (SYS_Asynch), so the commodity does not block while a child runs. amarchy-gui's
  event_loop (gui.c:962-985) waits only on the window IDCMP port and CTRL_C; so streaming output into the gui needs either a timer.device signal
  (poll the log file every 200 ms) or a PIPE:/message port signal added to that Wait; a long install would otherwise only block the gui if run
  synchronously inside it (do not).
- Reusable: lf_ui refusal messages (leaf.c:run_request), the launch port + act_run for async start with path copy, index_core.c row/parse/level
  pattern, ixwatch.c watcher (RESCAN-FEEDBACK) for "work in progress" status, gui.c message/viewer modes, mn_list_hook, filter typing as the Search row
  (index_core.c:617), wblaunch.c (stack from icon, wblaunch.c:179) for launching installed packages with the program's stack; amconf_dos for a prefs key.
  Not present: any grep hit for upkg/uhc in amarchy/*.c or config.

## D. Gaps

For the UP-Term session as UPKG-AMARCHY-API (upkg side, in upterm-ports; not edited here):
1. Machine-readable output: `upkg list --tsv` (or `--machine`): `name<TAB>version<TAB>installed(0/1)<TAB>upgradable-to<TAB>size<TAB>launch<TAB>section<TAB>description`,
   one call instead of list + list-installed + list-upgradable (each 1-41 s on a 68k, patches 0017 and 0022 comments). Same for `list-upgradable` and `list-installed`.
2. A real package search over name AND description with a plain substring (not fnmatch glob), exposed as a subcommand name that does not clash
   (`find` exists; `search` is a file-path search).
3. `Launch:` and a category (`Section:`) field in the ipk control and the status file (plan upkg-browse-launch L1-L3 not implemented; no Section exists).
4. A non-interactive install route: either `--answers FILE` for packages with Files/questions (dist/upkg-ask), or `info` exposes the question list so Amarchy can
   ask in its own drawn input mode and pass the answers; today an Interactive add-on blocks without a tty.
5. Documented stable exit codes and a progress line format (`[PROGRESS n/m]` or the existing lines) for a streamed log; `upkg update` phases.
6. Optionally a `--quiet-progress`/no-wrap flag so captured output never depends on TIOCGWINSZ/COLUMNS.

Blocked on UHC behaviour (no upkg change can fix):
1. No remove for UHC packages: pkg has install, installed, list, search, update only (S/pkg:88-95). Amarchy must implement remove itself
   (delete Installed/<name> plus the EXTBIN file, whose name is only known inside each stub script): not "UHC's own tools". Needs the owner's decision.
2. No upgrade list: no available-version index; version is a Set line inside each stub script; only the UHC bundle has a revision hash (uhccheck).
3. Search/Browse of UHC means Aminet/OS4Depot/WHD file search (uhcsearch endpoint), whose results are files, not packages: no installed notion, no upgrades.
   Browse = `*recent` searchtypes (aminetrecent etc.); install = `aminetget N` (download only) / `uhcextract` (unpack), with no install state.
4. Prompts: pkg `Ask` on reinstall (avoidable by Amarchy), installdeps Ask, AskMore pager, interactive pieces in UHCSetDirectory; stubs print progress
   without a machine format; rc is 0/20 (UHCRC).
5. Result rows are column aligned text, not delimited (sizes rounded); a1k.org and hex9d.com gave 404 on /uhcsearch in my queries.
6. uhcupdate rewrites UHC: in place and may copy itself to S: in the background (S/uhcupdate:55-63): Amarchy must call it only as the "Upgrade UHC Tools"
   row and not run it concurrently with a package install.

## Proposed shape

Back-end interface (one C module pk_core.c/pk_dos.c in amarchy, portable parser in *_core.c like index_core.c): `pk_list(kind)` returning rows
`{src (UPKG|UHC), name, version, installed, upgrade_to, size, launch, section, desc}`; `pk_search(words)`, `pk_install(row)`, `pk_remove(row)`,
`pk_upgrade(row|all)`, `pk_launch(row)`. Two drivers behind it. upkg driver: runs `upkg` through SystemTags with SYS_Output = T:amarchy-pk.<n>
(Input NIL:), parses `name - version - description` rows until UPKG-AMARCHY-API gives TSV. UHC driver: builds its rows from `pkg list` / `pkg installed`
(fixed 20-column rows, 11 packages) plus Search = `uhcmirrorsearch <type> words` result tables; install = `pkg install` (after dropping a stale
Installed file to avoid the Ask), remove = delete (decision needed), upgrade = UHC self-update only.

Menu tree: Packages (Left Amiga+Space) > Search (typing in the menu filters the merged list, like index_core.c:617), Browse (grouped by
Section/source), Installed, Upgrades; each row `[upkg] name - version` or `[UHC] name - version`; Enter on a row = Install / Launch / Remove /
Upgrade depending on state (same keys as other Amarchy rows; delete key = remove). Rows are generated from a cache file (ENVARC:Amarchy/packages or T:)
refreshed by a scan job like amarchy-index, so opening the menu never waits on a 3 s upkg load.

Views: list (existing), plus a new log mode in amarchy-gui built on the viewer/message line mode (gui.c:594-600, 1074) showing the captured file
with a header (`Installing nvim [upkg]`), a progress/rc footer, scroll keys; closes on Esc, shows `[ERROR] ... (exit N)` on rc != 0.

Process model: the commodity (amarchy.c:328 loop) starts the job async via act_run-style SystemTags (SYS_Asynch, NP_StackSize 65536+, Input NIL:, Output a
T: file, NP_Path copy) so its Wait loop is never blocked; the gui tails the file on a timer.device signal (200 ms) added to event_loop's Wait
(gui.c:962) and learns the rc from the commodity (launch-port message or a small `.rc` file the job wrapper writes). One job at a time (upkg lock_file,
UHC shared T: files and UHC:Packages/Installed); queue other picks. UHC jobs run as a wrapper script that sets ENV:UHC defaults, so no console opens.
Interactive add-ons: ask the questions in a drawn input mode (gui.c input mode) once UPKG-AMARCHY-API gives the question list.

## Open questions for the owner

1. UHC remove: UHC has none. Amarchy implements remove for the 11 pkg stubs itself (delete the EXTBIN file + Installed record), or UHC rows show no Remove?
2. UHC "Search/Browse" shows Aminet/OS4Depot/WHD files (UHC's search service), not UHC packages. Include those as [UHC] rows with Install = download+unpack
   (no installed/upgrade state), or limit the [UHC] rows to the 11 pkg packages plus "Update UHC Tools"?
3. Upgrades for [UHC]: only UHC Tools itself (uhccheck) and the stubs' PKGVERSION after an UHC update. Acceptable?
4. Interactive add-ons (up-vim etc. with Files/questions): ask the questions in Amarchy's drawn input mode (needs UPKG-AMARCHY-API), yes?
5. Cache refresh policy: scan on the first open of Packages and on explicit Refresh only (upkg update is a network call), or on every open?
6. A1200 check of the installed UHC state (UHC:Packages/Installed, packages.cache) was skipped; run it when the crash agent is done?
