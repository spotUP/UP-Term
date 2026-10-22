---
date: 2026-10-22
topic: Amarchy, the Omarchy Super-key system on the Amiga: one keymap file, terminal side first, then a system-wide commodity
status: plan (owner reviews before any code)
research: thoughts/shared/research/2026-10-21_amarchy-super-keys.md
ledger: thoughts/spot/todos.md, item "SUPER-KEYS / AMARCHY"
repos: vtcon (keymap parser, handler, commodity, dist, host and rig tests), upterm-ports (upkg-ui --menu filter field)
---

# Plan: Amarchy (keymap file, terminal side, commodity)

## Goal and finish line

Left Amiga is Super. One file, `ENVARC:up-term/keymap`, lists every Super binding. The terminal (vtcon-handler)
and a new commodity `Amarchy` read it through one parser. Super+Space opens the quick menu, Super+K shows the
bindings generated from the file, and the window menu's command keys are generated from the same file.
Done = Phases A to F all ticked, with the named reachability test (R1, below) passing on the rig. A partial
run is reported as "n of N open", never "done".

## Owner decisions already taken (not re-litigated)

- Super = LEFT Amiga. Meta moves to Alt: the existing `meta = alt` option (dist/up-term.conf line 26, read at
  handler/vtcon_handler.c:1247) becomes the default.
- Left Amiga+M, Left Amiga+N and Left Amiga+mouse stay Intuition's: never bound; the parser refuses them.
- Viewer = Super+K. Clear screen moves from Right Amiga+K to Right Amiga+Shift+K.
- Menu command keys are generated from the keymap file.
- First scope = the ~40 bindings with an Amiga equivalent (research section 3).
- Terminal side first; the commodity reads the same file.

## What exists, what is missing (searched)

Searched all sibling repos for `CxBroker|HotKey|InputHandler|AddIEvents|commodities` in .c/.h/.md/Makefile:
no hits outside thoughts/. There is NO commodity or input-handler code to reuse; the commodity is new.
Reusable:

- `config/upconf.c` / `config/upconf.h`: INI parser (`upconf_parse`, `upconf_get`, `upconf_str`, `upconf_int`,
  `upconf_profiles`, `upconf_set`, `upconf_save`), portable C89, no allocation, host test tests/test_upconf.c,
  already linked into the handler (Makefile HANDLER_SRC) and upprefs. Comments survive write-back.
- `handler/slash.c` / `slash.h`: the `/x` command table and parser; every slash command is bindable for free.
- `handler/menu_ids.h` and `menu_def` (vtcon_handler.c:1493): `MENU_*` ids are the action ids.
- `dist/upkg-browse` lines ~211-242 drive `upkg-ui --menu`; upkg-ui source lives in upterm-ports (opkg patch
  set; locate the exact patch file in Phase D step 1, the grep with --include globs found no hit and must be
  redone with `grep -rn "upkg-ui" upterm-ports` without globs).
- tools/rig (wasabi MCP, `rig.py start --max`, `make test-rig`, `*_rig.py` pattern with PASS/FAIL, verdict files).
- Toolchains: handler and tools built with vbcc (`$(VC)`, Makefile:434, `-cpu=$(CPU)`, NDK at `$(VTCON_NDK)`);
  gcc ports with `$(AGCC)` (m68k-amigaos-gcc 6.5 Bebbo, `-mcrt=ixemul`). The commodity is a small native
  program: build with `$(VC)` like upprefs (Makefile:592-594), no ixemul, no heap-heavy code, so it stays
  resident cheaply.

Gap: upconf is keyed `[profile] key = value` (unique keys per profile). The keymap needs repeatable rows and
a `|` label field. Decision K1 below: reuse upconf's section and line tokeniser by adding one read-only
iteration call, not a second parser.

## 1. Keymap file contract

Path: `ENVARC:up-term/keymap`, mirrored to `ENV:up-term/keymap` by the same helper as up-term (read ENV: first).
Shipped default: `dist/keymap`, installed beside `dist/up-term.conf`; the install script never overwrites a
user-edited file (compare to the previous shipped checksum, else write `keymap.new`).
Text, ISO-8859-1, LF or CRLF, no JSON.

Syntax (one line per item; a line scanner, no recursion):

    ; comment to end of line (also after a row, separated by " ; ")
    [keymap]
    version = 1                       ; integer; parser refuses a newer major
    super   = lamiga                  ; fixed to lamiga in v1; the key is read, other values rejected with a log line
    [bind]
    ; key spec          = action                ; label                | category
    super+space         = menu:root             ; Menu                 | menu
    super+k             = viewer                ; Keybindings          | menu
    super+t             = tab:new               ; New tab              | window
    super+1..9          = tab:goto %n           ; Switch to tab %n     | workspace
    super+shift+f       = slash:/font bigger    ; Bigger font          | style
    super+return        = run:tmux              ; Tmux                 | apps
    [menu]
    ; dotted id (tree)  = label                 | action
    apps.upkg           = Install add-ons       | run:upkg-browse
    style.theme         = Theme                 | slash:/theme

Key spec: `super` plus optional `shift`, `ctrl`, `alt`, `rshift` joined with `+`, then one key name. Key names
are lower-case words: `space return tab esc help del up down left right f1..f10`, and letters/digits by RAW
code (not by keymap character), so the Swedish keymap does not move them; `1..9` expands to nine rows with `%n`.
Refused at load (logged once, row skipped): `super+m`, `super+n`, any spec with the mouse, and any spec with
`ctrl` (Ctrl+both Amigas = hardware reset; refusing all super+ctrl rows is the simple safe rule).

Action prefixes (closed set): `menu:<id>` open the quick menu at a tree node; `viewer`; `tab:<new|close|next|prev|goto N>`;
`menuid:<MENU_NAME>` any `MENU_*` id by name (generated name table from menu_ids.h, so a new menu item is
bindable without parser changes); `slash:/cmd args` via slash.c; `run:<command line>` (asynchronous); `rexx:<script
or string>` (commodity only, v1.1); `open:uterm` (open a UPTerm window; commodity only; in the terminal the same
key does `tab:new`). Each action has a scope flag in the table: `T` terminal-only (tab:, menuid:, slash:),
`C` commodity-only (open:, rexx:), `B` both (run:, menu:, viewer).

User overrides: the file is the single list; the user edits the one file. A later row for the same key spec
replaces an earlier one (so a short `ENVARC:up-term/keymap.user`, optional, read after the shipped file, can
override single rows without touching the default; `key = none` unbinds). No merging of menu rows: `[menu]` ids
from .user replace by id.

Versioning: `[keymap] version = 1`. Unknown sections and unknown `[keymap]` keys are ignored (forward
compatible); unknown action prefixes skip the row and log. A missing or unparsable file falls back to the
compiled-in default table (the same text, generated into a C string at build time from dist/keymap), so the
terminal never loses its keys.

Generation (single source): a host tool `tools/gen_keymap_menu.py` is NOT needed at build time for menu keys.
At run time the handler builds the NewMenu command-key letters from the parsed table (Right Amiga keys remain
Intuition command keys; Left Amiga/Super keys are handler or commodity matched). A build-time check
(`tests/test_keymap.c`) asserts the shipped dist/keymap and menu_def have no command-key disagreement.

Shared parser: new `config/keymap.c` / `keymap.h` (C89, no allocation, fixed table `KM_MAX_BIND 192`, `KM_MAX_MENU 128`)
built ON `upconf` (reuse its section split and comment handling; add `upconf_rows(c, section, cb)` that
walks a section's lines in order including duplicate keys, the only change to upconf). Linked into the handler,
the commodity, and a host test. Output: arrays of `{qual mask, rawcode, action kind, arg, label, category}`,
used identically by both callers. Resolves the open item "INI format/action prefixes".

## 2. Commodity design

Program: `C:Amarchy` (or `SYS:WBStartup/Amarchy`, tooltypes `CX_PRIORITY=0`, `CX_POPUP=NO`, `DONOTWAIT`), vbcc, no
ixemul, stack set by `__stack`/cookie (project rule: any crash, first probe Stack 262144; give it 16 KB).

- Broker: `CxBroker` with `NewBroker` name `Amarchy`, title "Amarchy Super keys", descr, `nb_Unique = NBU_UNIQUE|NBU_NOTIFY`
  (a second start tells the first to show/reload), `CX_PRIORITY` from tooltype/args (default 0, below
  Exchange-hotkey tools, above nothing important), `COF_SHOW_HIDE` so Exchange has Show/Hide (Show opens the
  viewer window, Hide closes it). Exchange Enable/Disable toggles the hotkey set (`ActivateCxObj`), Kill ends it
  (`CXCMD_KILL` -> `DeleteCxObjAll`).
- Hotkeys: for each parsed `[bind]` row with scope `B` or `C`, `CxFilter` with an input-description string
  built from the row: `lcommand <key>`, `lcommand shift <key>` (commodities syntax `lcommand` is the Left Amiga
  qualifier), `CxSender` to a message port, `CxTranslate` null (swallow the key so the focused window never sees
  the keypress; this also prevents a double action when the terminal side matches the same key, see below).
  Message id = row index. Main loop is a single `Wait` on the broker port + the file-notify signal + the viewer
  window signal.
- Reload: `StartNotify` on `ENVARC:up-term/keymap` (dos.library notification, message signal) plus an Exchange
  command via ARexx port `AMARCHY` (`RELOAD`, `SHOW`, `QUIT`, `RUN id`). On change: parse into a second table,
  swap only if parse succeeds (a bad edit leaves the old set active and logs), then rebuild the CxObj list.
  Also reload on `CXCMD_APPEAR`? No: appear shows the viewer only.
- Coexistence with the terminal side (who wins when an UPTerm window is active): the commodity is above
  Intuition in the input chain, so it sees Left Amiga keys first. Rule: while the active window belongs to
  UPTerm, the commodity PASSES keys with scope `T` and `B`-handled-in-terminal rows (no `CxTranslate` swallow;
  the filter is a plain `CxFilter` whose sender is skipped by the `IEQUALIFIER` check), the handler matches them,
  because the terminal knows tabs and menu items and can act without a round trip. Detect the active window:
  `IntuitionBase->ActiveWindow->UserPort` compared to a registered UPTerm list is brittle; instead the handler
  publishes `ENV:up-term/active` is slow; chosen mechanism: the handler sets a flag in a public semaphore
  `UPTERM.active` (SignalSemaphore with a counter of focused UPTerm windows, set on IDCMP_ACTIVEWINDOW and cleared
  on INACTIVEWINDOW). The commodity reads it (ObtainSemaphoreShared) in its filter callback via `CxCustom` rather
  than a bare CxFilter. One action per keypress, always: either the handler (UPTerm active, scope T or B) or the
  commodity (otherwise, and scope C or B). Test R3 proves no double fire.
- Left Amiga vs Intuition and Workbench reserved keys: Left Amiga+M/N and Left Amiga+mouse are never bound
  (parser refuses). Ctrl+both Amigas never bound (no `ctrl` rows). Left Amiga+key combos that Workbench uses as
  menu shortcuts only exist with RIGHT Amiga; Left Amiga with a letter is delivered to the application as a
  RAWKEY with the LCOMMAND qualifier, so a global Left Amiga+letter swallow breaks any app that uses Left Amiga
  chords (Directory Opus, editors). Mitigation: only the keymap's rows are filtered, nothing else; every other
  Left Amiga chord passes untouched; Exchange Disable restores them all.
- Swedish keymap: the filter uses rawkey codes (`cx` accepts a `rawkey` name or a numeric `0x..`; generate numeric
  descriptions `lcommand rawkey 0x35` from the key table), so layout does not move bindings. Rig check: boot rig
  with Input prefs keymap `s` (Swedish), press Left Amiga+Alt+key and confirm a national character (Alt is a
  character modifier) does not trigger rows that don't list alt, and that `meta = alt` still delivers ESC+letter
  to vsh/vim on that keymap (Phase A test).

## 3. Actions

- run: `SystemTags` with `SYS_Asynch`, `NP_StackSize 32768`, `NP_Path` copied from the commodity's path (`DupPath`
  idiom: lock-list clone of `Cli->cli_CommandDir`, owned by the child, so the child outlives the commodity and
  `C:`/`UP-Term:bin` are found), input `NIL:`, output `NIL:` unless the row ends in `>CON:` (then a `CON:` window
  for the output). UP-Term commands run as `Run >NIL: <` style via ixemul programs needing their own stack: the
  cookie `$STACK` is set per tests/ixemul rules; tmux/vsh run only inside UPTerm (`open:uterm`).
- open UPTerm: start `UPTerm` (the handler's window launcher; find the exact existing entry, `NewShell UPTERM:` or
  the Workbench tool, in Phase C step 1) asynchronously; when UPTerm is already active, `tab:new` instead (terminal
  side).
- quick menu: see section 3a. Super+Space.
- viewer (Super+K): a window listing category / key / label, generated from the parsed table at open time (a
  second reload always shows current file). Terminal side: drawn as a handler overlay or by launching
  `upkg-ui --menu`-style read-only dialog fed from the table (same code path as the quick menu with no Enter
  action). Commodity side: the same program called with `--keys`.

### 3a. Quick menu: link a library or exec upkg-ui?

Open question: upkg-ui load time. Today every upkg-ui is a process start plus curses, and BROWSE-SNAPPY
(ledger, in 1.7) measured "long pauses" from per-view process starts and a ~3 s opkg load; that is why browse is
moving to ONE long-lived in-process `upkg-ui --browse`. Position:

- The menu needs a window in <0.3 s on a 68030 and must not load opkg. exec'ing `upkg-ui --menu` pays process
  start + dialog init each press; acceptable only if measured under 0.3 s on rig 2 (060) AND on the A1200 profile.
  Phase D step 1 measures `upkg-ui --menu` cold and warm, written to build/<rig>/amarchy/menu-timing.txt.
- Recommended: build the menu on the same in-process static widget library BROWSE-SNAPPY creates (a list widget
  with a filter field), linked into a small `amarchy-menu` helper (and into the commodity as a second binary, not
  the commodity itself, so the resident commodity stays small). The commodity starts `amarchy-menu` per press, or
  keeps it resident hidden and signals it (decide after measurement: signal if the process start exceeds the
  budget). Until BROWSE-SNAPPY lands the library, Phase D uses `upkg-ui --menu` with the filter added; the
  swap to the library is Phase D2 and is gated on the library existing. This avoids building the filter field twice:
  the filter field is added to the library, and `upkg-ui --menu` only gets a thin `--filter` flag as stopgap.
- Behaviour: typing filters, Enter runs, Esc goes back one level (Omarchy rules); the tree comes from `[menu]`
  rows; `menu:root` opens at the top.

## 4. First-scope bindings (Left Amiga as Super)

From research section 3. "Clash" column: R = the key has a Right-Amiga menu twin today (no clash: different
qualifier), M = Intuition/Workbench reserved, S = Swedish keymap check, E = existing handler or menu key that moves.

| Key (Super+) | Action | Clash/note |
|---|---|---|
| space | menu:root (quick menu) | none known; test S |
| shift+space | menu:apps | none |
| k | viewer | Right Amiga+K Clear screen moves to Right Amiga+Shift+K (E) |
| return | run:tmux in new tab (`tab:new` then tmux) | none |
| t | tab:new | R (RAmiga T stays) |
| w | tab:close (menuid:MENU_TAB_CLOSE) | none |
| q | menuid:MENU_CLOSE | commonly an app Quit key; only in UPTerm |
| 1..9 | tab:goto n | R; existing handler 1-9 stays on Right Amiga |
| . / , | tab:next / tab:prev | R |
| c / v / a | copy / paste / select all | R; Left Amiga+C/V reach terminal programs as Meta today (E, solved by meta = alt) |
| f / g | find / find next | R |
| plus / minus | font bigger / smaller | R; plus key position differs on Swedish keymap (S) |
| l | menuid:MENU_CLAUDE | R |
| shift+t | slash:/theme | none |
| shift+b | slash:/bg | none |
| shift+f | slash:/font | none |
| shift+s | slash:/screen own | none |
| shift+w | slash:/screen wb | none |
| shift+d | slash:/size 132x43 | none |
| shift+r | slash:/size 80x24 | none |
| p | menuid:MENU_PREFS | none |
| h | slash:/help | none |
| shift+h | slash:/tutorial | none |
| shift+l | slash:/demo | none |
| u | run:upkg-browse (Install/Update) | none |
| shift+u | run:upkg upgrade | none |
| i | run:upkg-browse (Install) | none |
| e | run:nvim (editor) | none |
| shift+e | run:emoji picker (dist/emoji) | maybe; owner may drop |
| x | run:Sound Prefs launch (Workbench Prefs programs by name) | launch only |
| shift+x | run:Screenmode launch | launch only |
| y | run:Palette launch | launch only |
| z | run:Internet prefs launch | launch only |
| left / right | tab:prev / tab:next | none |
| up / down | previous / next prompt | R (existing RAmiga Shift+Up/Down) |
| f1..f4 | tab:goto 1..4 (Omarchy workspace on F keys) | none |
| backspace | menuid:MENU_TAB_CLOSE alt | none |
| shift+q | reboot confirm (`Reboot` via menu) | destructive, confirm dialog, scope C |

Count: 39 distinct rows plus the 1..9 expansion; the exact ~40 is frozen when the owner approves the table (Decision D5).
Never bound: Super+M, Super+N, Super+mouse, any Super+Ctrl. Rows that Omarchy has but the Amiga does not
(tiling, workspaces move, transparency, gaps, screenshot, OCR, notifications, audio panels) are absent, not stubbed.

## 5. Phases

Every phase: `make test` stays green; type-check is the vbcc build (`make amiga`) with warnings-as-errors; run
only affected host tests (`make test ONLY=keymap`).

### Phase A: parser, file, default flip (host only, no rig for logic)
Files: config/keymap.c, config/keymap.h, config/upconf.c (add `upconf_rows`), dist/keymap, dist/up-term.conf
(`meta = alt` default; header text), handler/vtcon_handler.c (default of `meta` at :1247), Makefile (CONF list,
test list), tests/test_keymap.c, tests/test_upconf.c (rows).
Tests (host unit): syntax cases (comments, CRLF, `..` expansion, `%n`, unknown prefix skipped, newer version
rejected, refused `super+m/n/ctrl`, `none` unbind, .user override, duplicate-key replace, table overflow), the
shipped dist/keymap parses with zero skipped rows and no disagreement with menu_def. Existing meta test updated:
default is alt; `meta = amiga` still works.
Rig test (one, `tools/rig/meta_alt_rig.py`): Alt+b/f reach vsh line editor as M-b/M-f on the Swedish keymap.
Done: all host tests pass, parser linked into handler build, `meta = alt` default verified on the rig.

### Phase B: terminal side in vtcon-handler
Files: handler/vtcon_handler.c (match in the key path before `vtwin_key`, next to the find_open hook at :5264;
Left Amiga qualifier now free because Meta is Alt), handler/keymap_act.c (new: action dispatch table mapping
`tab:`, `menuid:`, `slash:` to the existing functions; no logic copied), handler/menu_ids.h name table,
the generated NewMenu command keys, Clear screen -> RAmiga+Shift+K, dist/keymap.
Tests: host unit for dispatch (action name -> MENU id; unknown id refused); handler-level sentinel counter
`km_hits` per row, printed by the existing trace (`make amiga DEBUG=1`) and by a `/state` line.
Rig tests: `tools/rig/keymap_rig.py` sends each first-scope key via amiga_key_press and asserts the effect
(tab count, font size, theme, find prompt); a table-driven case file, resumable, verdicts in build/<rig>/keymap/.
R1 reachability (terminal half): key sent to the real UPTerm window, `km_hits` for that row increments, the
spec's numbers hold (tab count +1, font size +step).
Done: all first-scope terminal rows PASS on rig; Left Amiga+M/N and Left Amiga+mouse verified still Intuition's;
Clear screen on RAmiga+Shift+K.

### Phase C: commodity skeleton
Files: amarchy/amarchy.c (new dir amarchy/), amarchy/cx.c (broker, filters, notify), amarchy/act.c (run, open),
Makefile (`build/amiga/Amarchy`, `make amiga` includes it), dist (install script, tooltypes icon, WBStartup
entry optional), dist/packages.txt row.
Steps: 1 locate how UPTerm windows are launched (grep dist/ and handler), 2 broker with Exchange show/hide/
enable/disable/kill, 3 hotkeys from keymap.c rows, 4 run + open:uterm actions, 5 ARexx port, 6 semaphore
`UPTERM.active` published by the handler (handler change in Phase B files).
Tests: host unit for the CxFilter description builder (key table -> string) in tests/test_keymap.c.
Rig tests (`tools/rig/amarchy_rig.py`): commodity starts and appears in Exchange (CxBroker list via `amiga_ui_list`
or `Exchange` ARexx), Disable stops a hotkey, Enable restores it, Kill removes it, second start is refused and
notifies the first, reload on edited keymap (sentinel: a row `run:C:Echo >T:amarchy.sentinel hit` creates the file).
R1 reachability (commodity half): with NO UPTerm window active (Shell on a plain console focused), send
Left Amiga+<row key>; the sentinel file appears; the commodity's hit counter (ARexx `STATUS`) increments by 1.
R3 no double fire: UPTerm active, same key, exactly one action (km_hits +1, commodity counter +0).
Done: R1 and R3 pass; Exchange operations pass; commodity idle CPU ~0 and under 20 KB resident.

### Phase D: quick menu and Super+K viewer
Files: amarchy/menu.c or `amarchy-menu` binary, handler overlay for viewer, upterm-ports patch for `upkg-ui --menu --filter`
(stopgap), dist/keymap `[menu]` rows.
Steps: 1 measure `upkg-ui --menu` cold/warm on rig 2 and A1200 (decision input), 2 filter field (stopgap flag),
3 tree from `[menu]`, Esc back one level, 4 viewer from the table, 5 (D2, gated on BROWSE-SNAPPY shipping the widget
library) swap to the in-process library and delete the stopgap.
Tests: host unit for the filter match function (prefix, substring, case); rig: Super+Space opens the menu, typing
"th" leaves Theme, Enter runs `/theme` (sentinel), Esc returns a level; viewer lists every `[bind]` row (count read
from the file by the test, second source: count the rows the viewer printed).
Done: both pass; open-to-visible under the measured budget, number recorded.

### Phase E: clash and keymap checks
Files: tools/rig/amarchy_clash_rig.py. Left Amiga+M, +N, +mouse drag behave as Intuition's with the commodity running;
Swedish keymap run of the whole row table; Left Amiga chord in a third-party app (a rig program using LCOMMAND)
passes untouched; Disable restores everything.
Done: all PASS, results table in the handoff.

### Phase F: docs and release wiring
Files: README/user guide section "Amarchy", tutorial lesson (dist/tutorial-lessons), man page, dist/packages.txt,
upkg feed row (`make dist-ipk` row check), RULES.md `## Commands` rows for `make test ONLY=keymap` and the two rig scripts.
Done: `make dist dist-check` green; install on a fresh rig kit gives the commodity running after reboot.

## Ledger (tick in the open; N of 6 phases, items below)

- [ ] A1 keymap.c/h  [ ] A2 upconf_rows  [ ] A3 dist/keymap  [ ] A4 meta=alt default  [ ] A5 host tests  [ ] A6 rig meta test
- [ ] B1 handler match  [ ] B2 dispatch  [ ] B3 generated menu keys  [ ] B4 Clear screen move  [ ] B5 rig table  [ ] B6 R1 terminal
- [ ] C1 broker  [ ] C2 hotkeys  [ ] C3 actions  [ ] C4 semaphore  [ ] C5 ARexx  [ ] C6 R1 commodity  [ ] C7 R3
- [ ] D1 timing  [ ] D2 filter field  [ ] D3 tree  [ ] D4 viewer  [ ] D5 library swap
- [ ] E1 clash rig  [ ] F1 docs and packaging

## 6. Decisions the owner must make

D1. Super = Left Amiga is decided, but its cost is that every Left Amiga chord in other applications is
    swallowed only for the keymap's rows. Recommended: filter only listed rows (what this plan does). Alternative:
    none cheaper. Trade-off: apps lose those few chords while Amarchy is enabled.
D2. Meta default flip to Alt. Recommended: flip in Phase A for everyone (the file header and release notes say so).
    Alternative: flip only when the keymap is active. Trade-off: the second keeps old users' Meta but makes behaviour
    depend on a second file.
D3. Parser extension. Recommended: add `upconf_rows` to upconf (one shared tokeniser). Alternative: a standalone
    keymap parser. Trade-off: standalone is simpler to test in isolation but duplicates comment and section handling.
D4. Quick menu implementation. Recommended: stopgap `upkg-ui --menu --filter` first if timing is under budget, then
    the BROWSE-SNAPPY static widget library (Phase D2). Alternative: only exec upkg-ui, never the library.
    Trade-off: library link is fastest per press but waits on BROWSE-SNAPPY; exec keeps one binary but pays a process start.
    DECIDED 2026-10-22 (owner): link libupkgui.a directly, no upkg-ui stopgap.
D5. Final binding table. Recommended: approve section 4 as the first release, dropping the low-value rows
    (emoji, Workbench Prefs launchers, reboot) to keep ~30 solid ones. Alternative: ship all ~40. Trade-off: fewer rows
    means less to test and fewer surprise clashes; more rows matches the "~40" scope.
    DECIDED 2026-10-22 (owner): ship all ~40 rows of section 4.
D6. Super+Ctrl forever banned. Recommended: ban (reset chord safety). Alternative: allow with Left Amiga only. Trade-off: banning removes four bindings but removes any chance of a reset hit.
D7. Commodity vs handler priority rule (handler wins when UPTerm active, via the `UPTERM.active` semaphore).
    Recommended as designed. Alternative: commodity handles everything and sends ARexx to the handler. Trade-off: one place for logic, but needs an ARexx round trip per key and a handler port.
D8. User overrides file `keymap.user`. Recommended: yes (upgrades never clobber edits). Alternative: single file. Trade-off: two files to explain.
D9. Resident commodity vs start-on-demand. Recommended: resident in WBStartup (hotkeys must work with no UPTerm open). Alternative: none; it cannot hotkey otherwise.

## Risks and unverified items

- Intuition/Workbench clash list is from OS knowledge and unverified until Phase E.
- Left Amiga qualifier name in the commodities input description (`lcommand`) and the filter/translate behaviour
  with CxCustom must be confirmed against the NDK autodoc in Phase C step 2 before wiring.
- BROWSE-SNAPPY dependency for D2; if it slips, the stopgap stays.
- Assumption: `NP_Path` cloning idiom works under the 3.x dos.library on the rig kit; confirm in C3.
