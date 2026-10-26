---
date: 2026-10-26
author: vtcon-a1
status: plan (owner reviews before any code)
topic: Amarchy gets its own preferences window; UP-Term Prefs drops the Amarchy page
---

# Plan: Amarchy preferences live inside Amarchy (AMARCHY-OWN-PREFS)

## Owner report (2026-10-25, about 23:55)

"The Amarchy menu font also opens UP-Term [Prefs], and the UP-Term Prefs, Amarchy and UP-Term need to be
separate apps." Today both Settings rows in `dist/keymap` (lines 104-105, `settings.amarchy.preferences` and
`settings.amarchy.font`) run `run:'C:UP-Term Prefs' AMARCHY`, so the two rows are duplicates and both open the
terminal's preferences program.

## Owner decisions (AskUserQuestion, 2026-10-25)

1. Settings file: Amarchy's settings move to their own file, `ENVARC:Amarchy/prefs` (ENV: copy as usual). The
   UP-Term file `ENVARC:up-term/up-term` no longer holds `amarchy-*` keys. A one-time migration reads the old keys
   when the new file is absent.
2. No separate program: the preferences window lives INSIDE the Amarchy commodity ("it could live inside amarchy,
   no need for a separate app"). Natural entry points: Exchange "Show Interface" (CXCMD_APPEAR, mapped to
   CXE_APPEAR in `amarchy/cx.c:164` but not acted on today) and the Settings menu rows.
3. All Amarchy preferences move into Amarchy, the menu font included ("move all amarchy prefs into amarchy").
   The Menu font row opens Amarchy's own window (design decides whether it opens straight on the font field or
   the font requester).
4. UP-Term Prefs loses the Amarchy page and the `AMARCHY` argument (`prefs/upprefs.c:1868-1871`).
5. Menu style is a field in the Amarchy preferences window (owner, 2026-10-26: "add menu style to the amarchy
   prefs plan"). Today `menu-style` (graphical | text; MENU-MINIMAL adds minimal) can only be set by editing
   `ENVARC:up-term/up-term` by hand (`dist/README.txt:703`); no prefs program shows it. The window offers every
   value the reader accepts (a cycle gadget), the key moves to `ENVARC:Amarchy/prefs` with the other Amarchy keys
   (migration reads the old line from the UP-Term file), and the commodity re-reads it on Save so Super+Space
   picks the new style without an Amarchy restart. Reader: `upconf_menu_style` (`config/upconf_rows.c:139-157`,
   called from `amarchy/amarchy.c:92`); the value list comes from that one reader, not a second table. Test: host
   test that every value the reader accepts is offered by the gadget; rig case: change the style in the window,
   Save, Super+Space opens the other menu program.

## Goal and finish line

Amarchy owns its settings. One file, `ENVARC:Amarchy/prefs` (plus the ENV: copy), holds every Amarchy key. The
Amarchy commodity opens its own preferences window from three places: Exchange "Show Interface", the Settings
menu row "Amarchy preferences" and the Settings menu row "Menu font". UP-Term Prefs no longer shows Amarchy
settings and no longer accepts `AMARCHY`.

Finish line: on a rig (OS 3.2, `rig.py start --max`), a disk with only the old keys in `ENVARC:up-term/up-term`
boots, the commodity migrates them into `ENVARC:Amarchy/prefs` once, Exchange "Show Interface" opens the window,
a changed menu font and menu style are saved and take effect without an Amarchy restart, both menu rows open the
same window, and `C:UP-Term-Prefs AMARCHY` is refused with its usage line. All checklist items below ticked.

## Facts (2026-10-26, read from origin/main 7f5ec51d)

Keys. Eight keys, all read from the `[profile default]` section of the UP-Term file today (counted from the
UP-Term Prefs item table `prefs/prefs_core.h:51-56` plus the two index readers):

| Key | Values | Default (absent key) | Readers today |
|---|---|---|---|
| `amarchy-music-drawers` | path list | empty | gui.c index_drawers 579-586, UP-Term Prefs |
| `amarchy-demo-drawers` | path list | empty | same |
| `amarchy-hide-drawers` | path list | empty | same |
| `amarchy-menu-font` | `name:size` | empty = built-in font | gui.c pick_font 1100-1110 (`prefs_font_parse`), UP-Term Prefs |
| `amarchy-app-icons` | generic, amiga, none | generic | gui.c 579-586 via `upconf_app_icons` (`config/upconf_rows.c:157-164`) |
| `menu-style` | graphical, text, minimal | graphical | amarchy.c style_load 75-98 via `upconf_menu_style` (`upconf_rows.c:166-171`); gui.c via `upconf_menu_style_value` (139-146) |
| `amarchy-index-skip` | path list | empty | index.c conf_read 197-212 only (no prefs field today) |
| `amarchy-index-volumes` | volume list | empty | same |

- Three separate config readers exist for these keys: `amarchy/gui.c:913-928` (ENV:up-term/up-term then ENVARC),
  `amarchy/index.c:197-212`, `amarchy/amarchy.c:75-98` (style_load, called at startup 305 and in reload 186-192).
  amarchy-gui also reads `palette`, `fg`, `bg` (terminal theme, `gui.c:935-940`); those stay in the UP-Term file.
- upconf: `config/upconf.h` UC_MAX_KEYS 32 per profile, UC_MAX_VALUE 160, UC_MAX_FILE 16384; parse, str, set,
  del, save (176), rows; UC_MENU_* 116-118, UC_ICONS_* 120-122. MENU-MINIMAL has landed (UC_MENU_MINIMAL).
- UP-Term Prefs Amarchy plumbing: fields `prefs/prefs_core.h:51-56` (am_music, am_demo, am_hide, am_font,
  am_icons, am_mode); read `prefs/prefs_core.c:73-78`; equal 602-604; stage 764-774 via `pc_put` (688; empty
  deletes); `prefs_install` 806; `prefs_conf_dir/path` 833-834; `prefs_install_target` 836-842 with hardcoded
  tmp `ENVARC:up-term/up-term.new` and orig `.orig` (commit e89e187c: ENV: written in place, rename dance only
  on ENVARC:, because the OS 3.2 ram-handler 47.57 copies the ENVARC file into ENV on lookup of a missing name).
  `prefs/prefs_dos.c` prefs_dos_dir, prefs_dos_save 59-79 (also writes PREFS_HLCAT_VAR). Host test
  `install_env_fallthrough` in tests/test_prefs.c about 120-150 (memfs fake).
- Layout: `prefs/prefs_ui.h` ids PG_AM_MUSIC 140 ... PG_AM_MODE 147, PG_PAGE_AMARCHY, common PG_PAGE 1, SAVE 2,
  USE 3, CANCEL 4, STATUS 5; PL_MAX_IDS 160. pl_layout, pl_hit, pg_find, pg_for_key walk the global
  `pg_items`/`pg_nitems` (`prefs/prefs_ui.c:11-249`, layout_with about 364). Amarchy rows 192-215.
  `pg_page_for_args` (AMARCHY keyword) 502-517.
- `prefs/upprefs.c` (2002 lines): icons_labels 140, mode_labels 141 ("Drawn", "Text", "Minimal"),
  pick_menu_font 1487-1503, AMARCHY usage 1873-1875. GadTools builder `gad` 879, `make_gadget` 897,
  `build_gadgets` 949 is tied to `struct app`. `prefs/prefs_asl.c` asl_pick_font is reusable.
- Makefile: AMARCHY_SRC 687 (vbcc; links config/upconf_rows.c, not config/upconf.c, no prefs_*, no
  gadtools/asl); amarchy-gui 726 links prefs_core.c and upconf.c; amarchy-index 735 links upconf.c; amarchy-menu
  717 (gcc/ixemul) reads none of these keys.
- Commodity: `amarchy/cx.c` cx_open 54-70 (NBU_UNIQUE|NBU_NOTIFY, COF_SHOW_HIDE); cx_next 143-169 returns
  CXE_APPEAR / CXE_DISAPPEAR (ignored by the main loop) and CXE_UNIQUE (calls reload). `amarchy/amarchy.c` main
  288-372 waits on cx, ARexx (port AMARCHY: STATUS, RELOAD, RUN, WIN, QUIT, SHOW), notify, launch, window and
  child-window signals. notify_paths 47-51 (keymap, keymap.user, up-term/up-term on ENV and ENVARC; NNOTIFY 6).
- `ENVARC:Amarchy` already exists as a drawer (apps, icons; CreateDir in index.c 359 and icon_gfx.c 326).
- Menu rows: `dist/keymap:137-138` `settings.amarchy.preferences` and `settings.amarchy.font`, both
  `run:C:UP-Term-Prefs AMARCHY`; checked in tests/test_keymap.c 2819-2820. Menu leaves already reach the
  commodity port directly: LF_WIN sends "WIN <verb>" to port AMARCHY (`amarchy/leaf.c:178-197`), no RexxMast
  needed; LF_REXX runs `RX` (`leaf.h:24,34`).
- Rig tools write the keys to `ENV:up-term/up-term` (tools/rig/amarchy_menu_rig.py 267, 434, 437, 503, 506, 557,
  768); other files naming them: tests/test_stage_rig.py, tests/test_rig_ixp6.py, tools/test_rig_upterm.py,
  tools/rig/rig.py, tests/amiga/scrprobe.c, tools/rig/prefs_rig.py. Docs: dist/README.txt Amarchy section from
  652 (menu-style 703/730, app-icons 737, index-skip 783, index-volumes 789); dist/up-term.conf 30-32;
  tests/test_amarchy_docs.py; tools/keymap_doc.py.

## Design

### 1. File format

`ENVARC:Amarchy/prefs` (and `ENV:Amarchy/prefs`) uses the upconf syntax the UP-Term file already uses, so the
existing parser and writer apply unchanged:

```
; Amarchy preferences. Written by the Amarchy preferences window (Exchange: Show Interface).
[profile default]
menu-style = text
amarchy-menu-font = topaz.font:11
amarchy-music-drawers = Work:Music
```

- Same key names as today (no renames), so docs, rigs and greps keep one spelling.
- A key at its default is absent (as UP-Term Prefs does today via `pc_put`). An empty file with the header is a
  valid file: all defaults.
- Install: tmp `ENVARC:Amarchy/prefs.new`, orig `ENVARC:Amarchy/prefs.orig`; ENV: written in place. Same rules
  as e89e187c, through the same function (section 3).

### 2. One key table, one reader (single source of truth)

New pure module `amarchy/amconf_core.c/.h` (host-tested, no DOS calls):

- `am_keys[]`: name and default for the eight keys. Every reader, the window, the migration and the UP-Term
  Prefs legacy strip walk this table; nothing else lists the names.
- `amconf_parse(buf, len, struct am_prefs *out)`: defaults first, then values via upconf_parse/upconf_str.
  Enumerated values go through the existing readers (`upconf_menu_style_value`, `upconf_app_icons`), not a copy.
- `amconf_serialize(const struct am_prefs *, buf, cap)`: upconf_set for non-default values, upconf_del for
  defaults, upconf_save.
- `amconf_legacy(upterm_buf, len, out)`: reads the same keys from the `[profile default]` section of the UP-Term
  file (migration and pre-migration fallback).
- `amconf_need_migrate(envarc_new_exists)`: the migration decision as a pure function (section 4).

DOS side `amarchy/amconf_dos.c`: `amconf_load(out)` reads `ENV:Amarchy/prefs`, else `ENVARC:Amarchy/prefs`, else
the legacy UP-Term file (ENV then ENVARC). `amconf_save(prefs, envarc)` writes through the generalized install.
Linked by the commodity, amarchy-gui and amarchy-index. The three conf_read copies stop reading these keys
(gui keeps its UP-Term read for the theme only).

### 3. Reuse of the install path

Generalize, do not copy: `prefs_install_target_at(fs, target, const struct prefs_paths *p, buf, len)` where
`prefs_paths` holds the ENV and ENVARC paths, tmp, orig and the drawers to create. `prefs_install_target`
becomes a wrapper with the UP-Term path set; `prefs_dos_dir` takes the path set the same way. Amarchy passes
its own set. The existing `install_env_fallthrough` test runs once per path set (both sets covered).

### 4. Migration rule

- When: commodity startup, before the first `amconf_load`. Marker: `ENVARC:Amarchy/prefs` absent. ENVARC (disk)
  is the marker, not ENV, because of the 3.2 fallthrough copy.
- What: read legacy keys (ENV:up-term/up-term first, ENVARC fallback), write `ENVARC:Amarchy/prefs` then
  `ENV:Amarchy/prefs`. The file is written even when no legacy key exists (header only), so the marker is set and
  the next start does nothing: idempotent.
- Both files exist: the new file wins, no migration, legacy keys ignored.
- Legacy keys left in the UP-Term file are removed by UP-Term Prefs on its next Save (`prefs_stage` runs
  upconf_del over `am_keys[]`); the commodity does not rewrite the UP-Term file (open question 1).
- A failed write (read-only ENVARC) keeps reading legacy keys and logs one line; it retries next start.

### 5. Window layout

One page, font-sensitive, same layout engine as UP-Term Prefs:

```
+ Amarchy Preferences --------------------------------------+
| Menu _style      [ Drawn  | Text | Minimal ] (cycle)      |
| Menu _font       [topaz.font 11        ] [Choose] [Clear] |
| App _icons       [ Generic | Amiga | None ] (cycle)       |
| _Music drawers   [Work:Music                            ] |
| _Demo drawers    [                                      ] |
| _Hidden drawers  [                                      ] |
| Index s_kip      [                                      ] |
| Index _volumes   [                                      ] |
| status line                                               |
| [ Save ]            [ Use ]                  [ Cancel ]   |
+-----------------------------------------------------------+
```

- Layout refactor: pl_layout, pl_hit, pg_find, pg_for_key take a `pg_table {items, n, npages}`. The current
  names stay as wrappers over the UP-Term table, so UP-Term Prefs code does not change. Amarchy table in
  `amarchy/amprefs_ui.c` reuses ids 140-147, adds two ids for index skip/volumes (below PL_MAX_IDS 160) and the
  common SAVE/USE/CANCEL/STATUS ids.
- Gadget builder: `gad`/`make_gadget`/`build_gadgets` move from upprefs.c into `prefs/prefs_gt.c` with a small
  context struct instead of `struct app`; upprefs.c and Amarchy both call it.
- Cycle labels come from the readers' value lists (one table per enum in upconf_rows), so the gadget offers
  exactly what the reader accepts (decision 5 test).
- Save: amconf_save to ENVARC and ENV, reload. Use: ENV only, reload. Cancel: close, no write. Window close
  gadget = Cancel.
- Font: Choose runs `asl_pick_font` (prefs/prefs_asl.c), Clear empties the field (built-in font).
- The window runs in the commodity task, non-modal: its UserPort signal joins the main Wait. The ASL requester is
  modal; hotkeys queue while it is open (open question 3).

### 6. Entry points

- Exchange "Show Interface": CXE_APPEAR opens the window or brings it to front; "Hide Interface" (CXE_DISAPPEAR)
  closes it as Cancel.
- Second `Run Amarchy` (CXE_UNIQUE): reload as today, then show the window (standard commodity behaviour).
- New ARexx command on port AMARCHY: `PREFS` opens the window; `PREFS FONT` opens it and starts the font
  requester (open question 2).
- Menu rows: `dist/keymap:137-138` become a direct port send through the LF_WIN sender (leaf.c:178-197),
  generalized to send any command: new row kind `amarchy:PREFS` / `amarchy:PREFS FONT` (open question 5).
- After Save/Use: the commodity re-reads (style takes effect at the next Super+Space). notify_paths swap the two
  up-term entries for `ENV:Amarchy/prefs` and `ENVARC:Amarchy/prefs` (keymap entries stay); amarchy-gui reads
  the file each time it opens, so the font change shows at the next menu open.

### 7. Removal from UP-Term Prefs

Drop the Amarchy page rows (prefs_ui.c 192-215), PG_PAGE_AMARCHY, am_* fields, read/equal/stage lines,
icons_labels/mode_labels and pick_menu_font in upprefs.c, `pg_page_for_args` AMARCHY keyword and usage text.
`C:UP-Term-Prefs AMARCHY` then fails ReadArgs with the usage line. Stage keeps one job: delete legacy keys.

### 8. Docs rows

dist/README.txt Amarchy section (new file, window, three entry points, key list from section 2), dist/up-term.conf
30-32 comment lines removed (pointer to the Amarchy window), tools/keymap_doc.py row text, tests/test_amarchy_docs.py
expectations. The peer docs session (vtcon-05) owns dist/README.txt wording; this plan gives the rows.

## Phases and checklist

Phase C, core (host):
- [ ] C1 `amconf_core` key table and parse with defaults. Test: test_amconf_parse_defaults.
- [ ] C2 Serialize: default value absent, non-default written, round trip. Test: test_amconf_roundtrip.
- [ ] C3 Legacy read from the UP-Term `[profile default]` section. Test: test_amconf_legacy.
- [ ] C4 Migration decision function (absent, both exist, empty legacy). Test: test_amconf_migrate_rule.
- [ ] C5 `prefs_install_target_at` with path sets; `prefs_install_target` wraps it. Test: install_env_fallthrough (both sets).
- [ ] C6 `prefs_dos_dir` takes a path set. Test: test_prefs_dos_dir_amarchy.
- [ ] C7 `amconf_dos` load chain ENV, ENVARC, legacy ENV, legacy ENVARC. Test: test_amconf_load_chain (memfs).
- [ ] C8 Makefile host test binary links amconf_core. Test: make test-host.

Phase R, readers:
- [ ] R1 Commodity style_load uses amconf_load. Test: test_amconf_style (host) plus G2.
- [ ] R2 amarchy-gui drawers, app icons, menu font via amconf_load; theme read stays. Test: G3.
- [ ] R3 amarchy-index skip/volumes via amconf_load. Test: test_amconf_index_keys.
- [ ] R4 notify_paths watch ENV/ENVARC:Amarchy/prefs instead of up-term. Test: G2.
- [ ] R5 Makefile links amconf into amarchy, amarchy-gui, amarchy-index (vbcc). Test: make amiga build.

Phase M, migration:
- [ ] M1 Commodity startup migration before first load. Test: G4.
- [ ] M2 Idempotence: second start no write; both-exist keeps new file. Test: test_amconf_migrate_idempotent.
- [ ] M3 UP-Term Prefs Save strips legacy keys over `am_keys[]`. Test: test_prefs_strip_legacy_amarchy.

Phase U, window:
- [ ] U1 `pg_table` parameter for pl_layout/pl_hit/pg_find/pg_for_key; old names wrap. Test: existing test_prefs layout cases unchanged.
- [ ] U2 Amarchy item table (ids 140-147 plus two), unique shortcuts. Test: test_amprefs_table.
- [ ] U3 GadTools builder moved to prefs/prefs_gt.c; upprefs.c uses it. Test: prefs_rig existing case.
- [ ] U4 Window open/close in commodity loop, UserPort in Wait. Test: G1.
- [ ] U5 Save/Use/Cancel semantics and reload. Test: test_amprefs_actions (host, fake fs) plus G2.
- [ ] U6 Font Choose/Clear via asl_pick_font. Test: G3.
- [ ] U7 Cycle labels from reader value lists; every accepted value offered. Test: test_amprefs_cycle_values.
- [ ] U8 Commodity links gadtools/asl, prefs_ui, prefs_gt, prefs_asl. Test: make amiga build.

Phase E, entry points:
- [ ] E1 CXE_APPEAR opens/fronts, CXE_DISAPPEAR closes. Test: G1.
- [ ] E2 CXE_UNIQUE reloads and shows. Test: test_cx_unique_shows (host) plus G5.
- [ ] E3 ARexx `PREFS` and `PREFS FONT`. Test: test_amarchy_rexx_prefs.
- [ ] E4 Row kind `amarchy:` through the generalized LF_WIN sender; dist/keymap 137-138 rewritten. Test: test_leaf_amarchy_kind.
- [ ] E5 tests/test_keymap.c rows 2819-2820 updated. Test: test_keymap.

Phase X, removal from UP-Term Prefs:
- [ ] X1 Amarchy page rows and PG_PAGE_AMARCHY gone. Test: test_prefs page count.
- [ ] X2 am_* fields, read, equal, stage lines gone (stage keeps strip). Test: test_prefs.
- [ ] X3 AMARCHY argument, pg_page_for_args keyword, usage text gone. Test: test_prefs_args_no_amarchy.
- [ ] X4 tests/test_prefs.c 544-660 Amarchy cases moved to amconf tests or deleted. Test: make test-host.

Phase D, docs and tools:
- [ ] D1 dist/README.txt Amarchy section rows (vtcon-05 wording). Test: test_amarchy_docs.
- [ ] D2 dist/up-term.conf 30-32 comment lines replaced by a pointer. Test: test_amarchy_docs.
- [ ] D3 tools/keymap_doc.py and tests/test_amarchy_docs.py rows. Test: test_amarchy_docs.
- [ ] D4 Rig tools write `ENV:Amarchy/prefs` (amarchy_menu_rig.py X9_CONF users and the other listed files). Test: amarchy_menu_rig minimal step.

Phase G, rig (`rig.py start --max --purpose "Amarchy session: own prefs window"`, stop the rig at the end):
- [ ] G1 Exchange Show Interface opens the window; Hide closes it. Test: rig own_prefs_show.
- [ ] G2 Change menu style, Save: ENVARC:Amarchy/prefs written, Super+Space opens the other menu program. Test: rig own_prefs_style.
- [ ] G3 Change menu font, Save: amarchy-gui opens with the new font. Test: rig own_prefs_font.
- [ ] G4 Disk with only legacy keys: first start writes ENVARC:Amarchy/prefs with them, second start no write. Test: rig own_prefs_migrate.
- [ ] G5 Both Settings rows open the window; Menu font row starts the requester. Test: rig own_prefs_rows.

Total: 8 + 5 + 3 + 8 + 5 + 4 + 4 + 5 = 42 items (C1-C8, R1-R5, M1-M3, U1-U8, E1-E5, X1-X4, D1-D4, G1-G5).

## Reachability (one test per feature)

One rig test drives the product entry point: G2. It boots Amarchy as installed, opens the window through Exchange
"Show Interface", changes menu style, presses Save, then presses Super+Space. Sentinel: the menu program that
opens is the other style's program (process name), and `ENVARC:Amarchy/prefs` holds `menu-style`. Migration has
its own entry point (first boot), proved once by G4. Every other behaviour is tested where it lives (host tests
above).

## Risks

- ASL requester blocks the commodity task: hotkeys wait until it closes (open question 3).
- Commodity binary grows (GadTools layout, ASL); no size budget for Amarchy, but the rig must load it.
- Layout refactor touches UP-Term Prefs: U1 keeps old names as wrappers so its tests run unchanged.
- Peer rigs and scripts that still write `ENV:up-term/up-term` keys stop affecting Amarchy after migration
  (D4 must land with M1).
- ENV fallthrough on 3.2: never test ENV for the marker; covered by C5 and C7.

## Open questions for the owner

1. Legacy keys in the UP-Term file. Recommended: UP-Term Prefs strips them on its next Save (each program writes
   only its own file). Alternative: the commodity rewrites the UP-Term file at migration (clean at once, but
   Amarchy then writes a file it does not own).
2. Menu font row. Recommended: opens the window and starts the font requester at once (one click to the
   requester). Alternative: opens the window only (rows then differ only by name).
3. Window task. Recommended: in the commodity task, ASL modal (no new process, simple). Alternative: child
   process (hotkeys stay live during the requester, more code and shared-state locking).
4. Index skip/volumes fields. Recommended: show them in the window (all Amarchy keys in one place, decision 3).
   Alternative: leave them hand-edited (smaller window).
5. Menu row mechanism. Recommended: new `amarchy:` row kind using the existing direct port sender (no RexxMast
   needed). Alternative: `rexx:` rows (no new kind, but needs RexxMast running).
