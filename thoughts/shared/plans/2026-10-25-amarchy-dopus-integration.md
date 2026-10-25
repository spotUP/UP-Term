---
date: 2026-10-25
topic: Amarchy and Directory Opus 5 Magellan II: six separately optional integration pieces (menu group over ARexx, optional add-on, UP-Term as DOpus shell and viewer, theme, native amarchy.module, DOpus-aware index)
status: plan (owner reviews before any code)
research: thoughts/shared/research/2026-10-25_dopus-magellan-integration.md
ledger: thoughts/spot/todos.md AM-FM (row 1441), AMB3 (row 1436)
repos: vtcon (dist/keymap, dist/menu-icons, dist/packages.txt, dist/addons, amarchy/, tests/, tools/rig); dopus5 upstream (P5 only, as a pull request)
---

# Plan: Amarchy and Directory Opus 5 integration

## Goal and finish line

Amarchy gets a `DOpus` menu group that drives a running Directory Opus 5 (Magellan II) over ARexx and hides itself when
none runs; DOpus can optionally be installed as an add-on; and, each separately, UP-Term can be DOpus's shell and
viewer, DOpus can wear the UP-Term palette, a native `amarchy.module` can act on the active lister, and Amarchy's
app index can see DOpus listers. The base kit never depends on DOpus (ledger decision: "not everyone likes it").
Finish line per piece: its checklist ticked with its reachability test green (section 8). A plan is 100% or failed:
the report is "n of N open". Scope cuts (dropping a piece) are the owner's call (section 7); nothing here is built yet.

## 1. Facts the design rests on

All from the research doc (commit `c8fbeb66b0952fe493aab9dc0e4c8ac559fa602e` of BlitterStudio/dopus5, v5.104):
- ARexx port `DOPUS.<n>`, first free n from 1 (`rexx_proc.c:43`); two top-level verbs `lister` and `dopus`, 135 table
  keywords; replies are RESULT string plus RC (F1).
- `dopus screen` returns `<screen name> <w> <h> <depth> ...`; on its own screen DOpus is the public screen `DOPUS.n`
  and the default public screen (F4). `dopus version` returns `5 99 104` for v5.104.
- Modules are `*.module` libraries in `DOpus5:Modules/`, found by directory notification; entry gets the selection, a
  lister handle and an `EXTCMD_*` callback table; APL 1.1 headers in the repo (F2).
- Filetypes and button banks are binary IFF `FORM OPUS`, readable and checkable with upstream `archive/tools/filetype_tool.py` (F3).
- No clipboard hook in DOpus source (F5): not a piece of this plan, the clipboard probe stays a separate ledger row.
- v5.104 os3 lha 1,810,459 bytes, APL 1.1 (`COPYING`), needs 68020 or better (F6).
- Amarchy side: `rexx:` row kind runs `RX "script"` through `system()` (`amarchy/leaf.c:185`); rows live in `dist/keymap`
  `[menu]` as `dotted.id = Label | action`; icons are Nerd Font code points in `dist/menu-icons`; hotkeys are `[bind]`
  rows; add-ons are non-base rows of `dist/packages.txt` with an archive and a `dist/addons/<id>/` drawer
  (`tools/mkinstaller.py:71-98`); AMB3 already keeps everything on the user's screen (`handler/frontscr.c`).
- `dist/`, `tools/mkinstaller.py` and the Makefile belong to another session while this plan is written: this plan
  only describes what their entries would hold.

## 2. P1: the `DOpus` menu group over ARexx (size M)

Why M: no new action kind (`rexx:` exists), but a probe, a script set, a conditional group, tests on a rig. The one
real unknown is hiding a group when the port is absent.

- Rows (ids, all `rexx:` unless noted; labels in full English words):
  - `dopus.newlister` = `Open lister` | `rexx:UP-Term:Rexx/dopus-newlister.rexx`
  - `dopus.listerhere` = `Open lister at current folder` | `rexx:...dopus-listerhere.rexx`
  - `dopus.front` = `Bring Directory Opus to front` | script sending `dopus front`
  - `dopus.back` = `Send Directory Opus to back` | `dopus back`
  - `dopus.parent` = `Parent folder in active lister` | `command Parent`
  - `dopus.refresh` = `Refresh active lister` | `lister refresh`
  - `dopus.devicelist` = `Show devices` | `command DeviceList`
  - `dopus.ftp` = `FTP connect` | `command FTPConnect` (module command; present only if `ftp.module` is installed: probe row)
  - `dopus.shell` = `Open UP-Term here` | `open:uterm` (P3 gives it the lister path)
  - `dopus.configure` = `Configure Directory Opus` | `command Configure`
  Command names come from the DOpus command list, not guessed: D1 reads them from the running DOpus with
  `dopus` / `command` listing (U until D1: the argument templates were not read, research section 4).
- Scripts live in one drawer, `UP-Term:Rexx/` (`dopus-*.rexx`), each starting with the same probe: `IF ~SHOW('P',
  'DOPUS.1')` then exit with RC 5 (no port). Port number: the script tries `DOPUS.1` to `DOPUS.9` and takes the first
  that answers `dopus version`; a later piece of work if two copies matter.
- Probe and hiding: the group must not show when no DOpus runs. Search first (reuse): how `trigger.clipboard`,
  the app index (`amarchy/index_core.c`, `ix_drop_shadowed`) and `ix_skip_match` drop rows; D3 reads them and extends
  the same mechanism with one condition ("port present") instead of a second hiding path. If no such mechanism
  exists, the probe is one `amarchy-gui` call at menu build that does `FindPort("DOPUS.1")`; the group node is skipped
  when it is absent. The base menu never lists `DOpus` with no DOpus.
- Icons from the existing families in `dist/menu-icons` (Nerd Font, md/fa/cod): group `dopus` = `U+F07B ; fa-folder`;
  `dopus.newlister` = `U+F07C ; fa-folder_open`; `dopus.parent` = `U+F062 ; fa-arrow_up`; `dopus.refresh` = `U+F021
  ; fa-refresh` (as `apps.rescan`); `dopus.configure` = `U+F013 ; fa-gear` (as `system`); `dopus.shell` = `U+F120 ;
  fa-terminal` (as `apps.upterm`); `dopus.devicelist` = `U+F0A0 ; fa-hdd_o`; `dopus.ftp` = `U+F0AC ; fa-globe`. Code
  points are fa names to be confirmed in the generator's font at D4 (U).
- Hotkeys `[bind]` on free combos. Taken (grep on `dist/keymap`): super+space, k, h, t, w, backspace, q, 1..9, f1..f4,
  period, comma, left, right, c, v, a, f, g, plus, minus, p, return, l, e, u, i; shift+t, b, f, s, w, d, r, l, h, u;
  alt+space, alt+k; M and N are Intuition's. Free (grep found no row, D2 re-checks with the parser, which refuses
  duplicates): `super+o` = `Open lister`, `super+shift+o` = `Open lister at current folder`, `super+d` = `Bring
  Directory Opus to front`. Three keys only: the rest stay menu-only. A hotkey for an absent DOpus does nothing and shows
  a one-line "Directory Opus is not running" message through the menu's existing notice path (U: name of that path, D3).
- Screen: scripts that open requesters or listers inherit nothing; DOpus listers land on DOpus's own screen by
  design. Rows that open UP-Term windows (`dopus.shell`) use AMB3's screen helper, so the window opens on the screen the
  user is on, including `DOPUS.1`.

## 3. P2: the optional add-on that repackages upstream v5.104 os3 (size M)

Why M: no code, but an Installer page, a manifest entry, licence text, and the upgrade policy decision.
Describe only; `dist/` is another session's.
- Entry: one non-base row in `dist/packages.txt` (id `dopus`, archive `up-dopus.lha`, a `dist/addons/dopus/` drawer
  with its own `Install` and `Uninstall` the way `up-python` has them), so `tools/mkinstaller.py` lists it on the
  add-on page with "(drawer found)" or "(will unpack lha)". The exact column layout of `packages.txt` is read at D5 (U).
- Content: the unmodified upstream `DOpus5_104_os3.lha` unpacked as a nested archive (pinned by name and an md5 in
  the row), plus `Files/COPYING` (APL 1.1 text), `Files/THIRD_PARTY_NOTICES.md` if present in the lha, and a
  `SOURCE.txt` naming https://github.com/BlitterStudio/dopus5, tag `v5.104`, commit to be read from the tag at D5
  (the clone here is at `c8fbeb6`, not necessarily the tag: U). `dist/LICENSES.txt` gets a DOpus paragraph.
- Installer page: tick "Directory Opus 5 (Magellan II) file manager, optional"; the page runs upstream's own
  `Install DOpus5` script (Installer program) with every option ticked (rig rule), destination default; UP-Term does not
  edit DOpus's start mode. Minimum CPU: 68020 (F6); on a 68000 the row says "needs 68020" and is not selectable
  (the os3-68000 archive is not shipped: owner decision, section 7).
- Upgrade: if `DOpus5:` already exists the add-on does nothing (never overwrites a user's setup); an explicit
  "Update Directory Opus" row is out of scope.
- P1 works with this add-on or any existing DOpus; P2 never makes P1 required.

## 4. P3: UP-Term as DOpus shell and viewer (size M)

Why M: two shipped binary files from a generator and a text-mode viewer path to test; the shipping format is binary IFF.
- Filetype file: `UP-Term:Dopus/Filetypes/UP-Term` (binary `FORM OPUS`), generated at build by a small script modelled
  on upstream `archive/tools/filetype_tool.py` (its encoder is the reference; the file is verified by the tool's
  `check` round trip and `lint`). Entries: text files (`MATCHNAME` `#?.(c|h|txt|md|rexx)`) with "Double-click" = AmigaDOS
  instruction `Run >NIL: UP-Term:bin/uterm-edit "%s"` and "User1" = viewer; the launcher opens `nvim`/`less` in an UP-Term
  window with the console spec `XCON:0/20/640/300/UP-Term/CLOSE/SCREEN <screen>` (spec from `LF_UTERM_SPEC`,
  `amarchy/leaf.h:10`; the screen from `dopus screen`, F4).
- Button bank: `UP-Term:Dopus/Buttons/UP-Term` (binary `FORM OPUS`, bank), buttons "UP-Term here", "Edit", "Page", "Shell";
  each an AmigaDOS function with `{p}`-style path args (DOpus's own substitution, names read from its guide: U). Not
  installed over the user's `Buttons/Toolbar`: the user opens the bank from DOpus's Buttons menu or the add-on
  loads it with `command LoadButtons UP-Term:Dopus/Buttons/UP-Term` (D8).
- No change to the user's existing filetypes: the file is added alongside; priority below the user's own.
- "Open in UP-Term" from the Amarchy side: `dopus.shell` runs a script that asks `lister query <handle> path` and
  passes it to the UP-Term launcher as the working directory.
- Global alternative (setting `output_device` to `XCON:` for every DOpus output window) is not shipped by default
  (invasive, section 7).

## 5. P4: theme, DOpus pens from the upconf palette (size S to M)

Why S: DOpus exposes `dopus pens` and `dopus palette` verbs (`RXCMD_PENS`, `RXCMD_PALETTE` in the table, F1); M if
the lister background and button-bank pens need separate `lister set` calls (U: arguments not read).
- Source of truth: the five roles `amarchy/menu_theme.c` already maps from the upconf palette: `MT_BG` (index 0),
  `MT_TEXT` (7), `MT_DIM` (8), `MT_ACCENT` (14), `MT_SELBOX` (4), computed by `menu_theme_roles_fb` from the palette and the
  `bg`/`fg` keys. P4 reuses `menu_theme_roles_fb` through a small commodity entry that formats the five RGB triples
  as an ARexx script call; no second palette table.
- Apply: a `dopus.theme` row (`Match Directory Opus colours to UP-Term`) runs `dopus pens`/`palette` once; a
  `Restore Directory Opus colours` row sends the saved original (read first with `dopus pens` and written to
  `UP-Term:Dopus/colours.saved`), because a bad palette must stay recoverable (research v2 risk, same rule: a 15 s
  auto-revert).
- Scope limit: DOpus screen pens when it has its own screen; on Workbench the Workbench palette is not touched here.

## 6. P5: native `amarchy.module` (size L)

Why L: a new C library with the Opus ABI, built on the m68k toolchain, a second licence regime, and an upstream pull
request. Do it last and only if the owner wants a function ARexx cannot give.
- What it can do that ARexx cannot (F2): receive the selected files and the source/destination lister in-process
  (`FUNCF_NEED_SOURCE`, `NEED_FILES`), add entries to a lister, show DOpus progress and requesters, use `EXTCMD_GET_SCREENDATA`
  so UP-Term windows open with DOpus's pens. Commands: `AmarchyOpen` (selection to an UP-Term window), `AmarchyRun`
  (run a menu row on the selection), `AmarchyHere` (shell in the lister's path). ARexx can do the path and the first
  selected entry, but not a multi-entry selection without long `lister query` loops.
- Build: against the dopus5 repo headers only (`source/Include/libraries/module.h`, `dopus5.h`, `proto/`), never from
  the 1998 SDK. Layout copied from `source/Modules/cleanup/` (`cleanup.c`, `_data.c`, `_strings.c`, `.cd`, `init`,
  `makefile.os3`). Output `amarchy.module` into `DOpus5:Modules/` by the P2 add-on.
- Stack: every Amiga program keeps `__stack` and the `$STACK` cookie (project rule): the module's own startup code and
  any helper executable it launches carry both; the module's callbacks run on the Opus caller's stack, so the module
  keeps its large buffers off the stack and checks free stack on entry (D13).
- Licence: the module is derived from APL 1.1 headers and examples; the file headers state APL 1.1 and the module's
  source ships with the add-on (`dist/addons/dopus/src/`). Repo-wide licence note added to `dist/LICENSES.txt`.
- Upstream: the module goes to BlitterStudio/dopus5 as a pull request from a fork branch only to create the PR; no
  long-lived private fork (ledger rule). Content: `source/Modules/amarchy/` plus `make` wiring. The Amarchy-specific
  glue (launching UP-Term) stays in vtcon; upstream gets only a generic "run an external launcher on the selection"
  command if upstream accepts that, otherwise the module ships only in the add-on (decision, section 7).

## 7. P6: Amarchy's index aware of DOpus listers (size L)

Why L: the index (`amarchy/index_core.c`, `ix_*`) is built for apps (volumes, drawers, `ix_row`); listers are live state.
- Idea: when DOpus runs, `lister query` on each open lister gives its path; the index lists "Folders open in Directory
  Opus" as a rows group so the menu can jump to a lister. Needs a refresh trigger (no event feed except `addtrap`), a
  volume-ownership rule (`ix_volume_of`), and shadow handling (`ix_drop_shadowed`).
- Order: after P1 (needs its probe) and P5 if the module path exists (the module can push, ARexx can only poll).
  Optional; the owner may drop it without effect on P1 to P5.

## 8. Phases and checklist

Running count is reported at every tick. A test column names the layer; "A1200 eye" means the owner looks, marked
unverified until then. 25 items.

### Phase A: facts (read, then lock)
- [ ] D1 Read the `lister`/`dopus` verb templates from `documents/DOpusM2_ARexx.guide` and one live `dopus` session on a rig (`dopus version` returns `5 99 104`, port name, command list); record in the research doc. Test: a rig case in `tools/rig/amarchy_menu_rig.py` prints `dopus version` and `dopus screen` (rig, text check).
- [ ] D2 Re-check the three hotkeys against the parser (duplicate refusal). Test: host test in `tests/test_keymap.c`, `dopus_binds_parse`.

### Phase P1: menu group
- [ ] D3 Port-present condition for a group (mechanism chosen after reading how rows are dropped). Test: host test in `tests/test_main.c` (or a new `tests/test_dopus.c`): group hidden with no port, shown with a fake port (a host stub of `FindPort`).
- [ ] D4 Rows, labels, icons in `dist/keymap` and `dist/menu-icons`; generator accepts every glyph. Test: host `tests/test_keymap.c` row count and id uniqueness; icon generator run is reported by the other session.
- [ ] D5 Script drawer `UP-Term:Rexx/dopus-*.rexx` with the probe (RC 5 when no port). Test: rig case: scripts run with no DOpus give RC 5 and no window.
- [ ] D6 Hotkeys wired. Test: rig case: `super+o` opens a lister (sentinel: lister count from `lister query`).
- [ ] D7 UP-Term row `dopus.shell` opens on the user's screen. Test: rig case with DOpus on its own screen: window is on `DOPUS.1`.

### Phase P2: add-on
- [ ] D8 `dist/packages.txt` row, `dist/addons/dopus/` drawer, pinned archive and md5, `COPYING`, `SOURCE.txt`, `LICENSES.txt` paragraph. Test: host `tests/test_dist_addons.py` (names and licence files present).
- [ ] D9 Installer page tick and run. Test: rig install of the add-on with every option ticked (rig rule), then `dopus version`.
- [ ] D10 68020 floor is enforced. Test: host test that the row's CPU field is 68020.

### Phase P3: shell and viewer
- [ ] D11 Filetype generator and file (`FORM OPUS`). Test: upstream `filetype_tool.py check` round trip and `lint` clean; host golden of the bytes.
- [ ] D12 Button bank generator and file. Test: same tool if it covers banks, else a rig load: `command LoadButtons` returns RC 0 (U).
- [ ] D13 Double-click on a text file opens an UP-Term window on the user's screen. Test: rig case, screen name asserted; A1200 eye check (owner).

### Phase P4: theme
- [ ] D14 `menu_theme_roles_fb` output formatted for DOpus. Test: host `tests/test_main.c` (five triples equal the role colours).
- [ ] D15 `dopus.theme` and restore rows; auto-revert. Test: rig case: pens read back with `dopus pens` equal the roles, then equal the saved set after restore.

### Phase P5: module
- [ ] D16 Module skeleton from `source/Modules/cleanup/` layout, `Module_Identify`, one command. Test: rig: DOpus module cache lists the command.
- [ ] D17 `__stack` and `$STACK` cookie on the module and every helper; large buffers off stack. Test: host check that the build flags and cookie string are present (as for other Amiga programs); rig run under a small `Stack`.
- [ ] D18 `AmarchyOpen`, `AmarchyRun`, `AmarchyHere` act on the selection. Test: rig case: select three files, run, sentinel: the module's call count in STATUS equals 1 and three files reached the launcher.
- [ ] D19 APL headers and notices; source ships in the add-on. Test: host dist test.
- [ ] D20 Upstream pull request opened (no private fork). Test: PR URL recorded; not a test of code. Owner approves before opening.

### Phase P6: index
- [ ] D21 Lister rows in the index with refresh. Test: host `tests/test_main.c` for the pure part; rig for the live part.
- [ ] D22 Group gone when DOpus quits. Test: rig case, quit DOpus, rows vanish.

### Phase Z: finish
- [ ] D23 Docs: tutorial and README mention, in plain words. Test: `tests/test_amarchy_docs.py`.
- [ ] D24 Ledger rows closed with commit hashes; AM-FM option (c) recorded as not started.
- [ ] D25 Owner eye check on the A1200 (UNVERIFIED until then): the whole flow.

## 9. Risks

- Hiding a group on a missing port may need a new mechanism in the menu builder (D3); size could rise to L.
- DOpus verbs and templates are partly unread (D1 gates P1's rows); a wrong command name makes a row do nothing.
- DOpus on its own screen is the default public screen: any program started without a screen name lands there.
  AMB3 handles ours; other programs are not ours.
- Binary FORM files cannot be reviewed as text: only round-trip and rig loads prove them.
- A bad pen set can make DOpus unreadable (P4): save-and-restore plus auto-revert are not optional.
- The module runs inside DOpus: a crash takes DOpus down. Stack discipline and small first command (D17).
- Upstream's os3 build targets 68020 (F6); a 68000 user gets no add-on.
- The clipboard probe stays unexplained (research F5): do not close it with this work.
- Shared files: `dist/`, `tools/mkinstaller.py`, Makefile belong to another session; items D4, D8, D10 wait for it.

## 10. Open decisions for the owner (recommended first)

1. Order and scope: (Recommended) P1 then P2 then P3, decide P4 to P6 after using them; alternative: all six now.
   Trade-off: P5 and P6 are L and need P1's findings.
2. Hotkeys: (Recommended) three, super+o, super+shift+o, super+d; alternative: menu-only.
3. Add-on pin: (Recommended) pin v5.104 os3 only; alternative: also ship os3-68000 for 68000 machines (+1.8 MB).
4. Existing DOpus: (Recommended) the add-on never touches an existing `DOpus5:`; alternative: offer upgrade.
5. UP-Term as shell: (Recommended) per-filetype AmigaDOS instruction (no change to the user's config); alternative:
   set DOpus's output device to `XCON:` globally.
6. Module upstream: (Recommended) PR a generic "run launcher on selection" command and keep Amarchy glue here;
   alternative: ship `amarchy.module` only in the add-on.
7. P6: (Recommended) defer; alternative: do it with P5.

## 11. Reachability (one test per piece)

- P1: `tools/rig/amarchy_menu_rig.py` case boots a rig with DOpus running, opens the drawn menu, picks
  `dopus.newlister` through amarchy-gui, asserts RC 0 and that `lister query` on the returned handle reports the
  path; sentinel: DOpus lister count rose by 1 AND Amarchy's row counter (`g_hits` in STATUS) rose by 1. The same case
  with DOpus quit asserts the group is absent (`dopus` not in the menu's node list).
- P2: rig install of the add-on, then `FindPort DOPUS.1` after starting it; sentinel: `dopus version` returns `5 99 104`.
- P3: rig: open a text file by DOpus double-click path (`command` of the filetype action) and assert an UP-Term
  window titled `UP-Term` exists on the asserted screen; sentinel: the launcher's counter file.
- P4: rig: apply, read `dopus pens`, assert equal to `menu_theme_roles_fb` output; restore, assert original.
- P5: rig: with `amarchy.module` installed, run the command through ARexx `command AmarchyOpen` with three files
  selected; sentinel: module call count 1 in STATUS and three paths in the launcher log.
- P6: rig: open two listers, assert two rows in the index; quit one, assert one.
- Every other behaviour (parsing, role mapping, binary encoding, licence files) is tested where it lives, host side.
