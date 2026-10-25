---
date: 2026-10-25
topic: Amarchy minimal menu mode, a third mode of amarchy-gui with text-only rows and two pens, and the app-icons value none
status: plan (owner reviews before any code; the mode and its shape are decided, the key name and four details are open)
research: thoughts/spot/todos.md, vtcon-05 rows MENU-MINIMAL (owner 2026-10-25 21:55, decision 22:03)
ledger: thoughts/spot/todos.md, section 2026-10-25 Amarchy session vtcon-05
repos: vtcon (config/upconf.h, config/upconf_rows.c, amarchy/amarchy.c, amarchy/cxdesc.c, amarchy/gui.c, amarchy/menu_geom.c/.h, amarchy/menu_theme.c/.h, prefs/*, dist docs, host and rig tests)
---

# Plan: Amarchy minimal menu mode

## 1. Goal and finish line

Owner, 2026-10-25 21:55: "we need a minimal mode as well with as little colors as possible and pure text for the
speed freaks no icons". Decision through AskUserQuestion, 22:03: a third mode in amarchy-gui next to drawn and text;
same window and hotkeys as drawn; text-only rows; no icons; no chevron and no border art; two pens, background and
foreground, the selection shown by swapping them; a mode cycle on the Prefs Amarchy page; `amarchy-app-icons`
gains the value `none`, which the mode reuses.

Done = every checklist item in section 7 ticked; the one reachability test (section 8) green; one rig check
(section 9) PASS; one owner eye check on the A1200 (section 10). Size: one sonnet agent for host work, one for the
rig run, three sessions at most. Starts after GROUP-DRAWER-ICON merges (same `gui.c`, `menu_icon.c`).

## 2. Facts the design rests on (read from the code, 2026-10-25)

- THERE IS ALREADY A MENU MODE KEY. `menu-style = graphical | text` in the default profile
  (`config/upconf_rows.c:139-157`, `upconf_menu_style`, constants `UC_MENU_GRAPHICAL 0` and `UC_MENU_TEXT 1` at
  `config/upconf.h:113-118`). The commodity reads it in `style_load` (`amarchy/amarchy.c:76-98`, global `g_style`)
  and `cx_menu_cmd` (`amarchy/cxdesc.c:52-74`) turns it into the program: `is_text = (style == UC_MENU_TEXT) != other`,
  `amarchy-menu` (text, libupkgui in an XCON window) or `amarchy-gui` (drawn). Shipped docs: `dist/up-term.conf:30`,
  `dist/README.txt:703`, `man/amarchy.1:47`. The owner's decision names a new key `amarchy-menu-mode`; section 3
  puts the single-source-of-truth question to the owner as decision 1.
- The drawn program reads other keys itself from the upconf: `amarchy-app-icons` into `amiga_icons`
  (`amarchy/gui.c:345-346, 572`), `amarchy-menu-font` (`gui.c:1067`), `palette`, `fg`, `bg` in `pens_get`
  (`gui.c:910-930`). Prefs reads and writes the same keys in `prefs/prefs_core.c:76-77, 765-768`; `am_icons` is a 0/1
  int, written as `amiga` or absent (absent, because a written "generic" overflowed the 32-key table in the
  ninth-profile test, `tests/test_prefs.c:353`).
- Pens: `menu_theme_roles_fb` (`amarchy/menu_theme.h:29`) gives five roles `MT_BG, MT_TEXT, MT_DIM, MT_ACCENT,
  MT_SELBOX`; `pens_get` obtains one pen per role (`vr_pen_obtain`, falling back to pen 0 for background and pen 1 for
  the rest). `draw_row` paints the selected row's box in `MT_SELBOX` with text in `MT_ACCENT`; `draw_border` fills the
  four edges in `MT_ACCENT`; `draw_header` draws the node name in `MT_DIM`.
- Geometry: `mg_metrics_for` (`amarchy/menu_geom.c:33-37`) sets the border `bx, by`, the header `head_h`, `icon_gap`,
  `chev_gap`; `mg_width_t` and `mg_layout` add `2*bx + 2*hpad + 2*row_pad`. Row text is drawn at
  `list_x + row_pad`; the icon column is `g->icon_w + icon_gap` wide when `g->icon_w` is nonzero
  (`gui.c:1165-1166`, `draw_row` 400-407); the chevron `>` of a group is drawn at the right edge with width
  `g->chev_w` (`gui.c:410-411, 1136`).
- The text menu (`amarchy-menu`, `amarchy/menu.c`) is a different program in an XCON window with the terminal's own
  colours and no icons; it is not changed by this plan.
- Host tests: `tests/test_keymap.c` holds the style tests (`menu_style_commands` at 572, `menu_style_parse` at 600,
  `STYLE()` macro); `tests/test_prefs.c` holds the Amarchy page round trip (lines 500-565).

## 3. Design

### 3.1 The mode value and where it is read

One enum, one parser, in `config/upconf.h`: add `UC_MENU_MINIMAL 2`. `menu_style_row` (`upconf_rows.c:139`) maps the
value `minimal` to it (`text` stays 1, anything else 0, last row wins, default profile only, exactly as today).
The commodity, via `style_load`, needs no change in how it stores `g_style`; `cx_menu_cmd` changes one line: the
text program is chosen for `UC_MENU_TEXT` only, so `minimal` runs `amarchy-gui` (`is_text` stays
`(style == UC_MENU_TEXT) != other`; for `other` the text menu opens, unchanged). `amarchy-gui` reads the same value
itself next to `amiga_icons` (a new `static int minimal`, set in `index_drawers` right where line 572 reads the
icons key, so every reload sees it). The key name decision is open (decision 1).

### 3.2 Drawing: one place decides, gui.c follows

A pure function in `amarchy/menu_theme.c`/`.h` (host file): `menu_theme_minimal(rgb_in, rgb_out)` sets every role to
one of two colours: `MT_BG = bg`, `MT_TEXT = MT_DIM = fg`, `MT_ACCENT = bg`, `MT_SELBOX = fg` (selection swapped:
selected row box in the foreground colour, its text in the background colour). Result: exactly two distinct pens.
`pens_get` calls it after `menu_theme_roles_fb` when `minimal`; the pens are then obtained as today (roles that share
a colour share the pen request; `vr_pen_obtain` already returns the same pen for the same rgb, which the host test
asserts: section 8). Default colours: the terminal's `fg` and `bg` upconf keys, falling back to the screen's pens 1
and 0 when unset (decision 4).

In `gui.c`, the `minimal` flag selects the shape without a second draw path:
- `draw_full`: skip `draw_border`; the background fill stays.
- `draw_row`: no icon column (`g->icon_w = 0` in the layout block at `gui.c:1165`, so the width and the row text
  position follow without a branch in `draw_row`); no chevron `put` for `MN_IS_GROUP`. A group is then not marked at
  all; decision 3 asks how it is shown.
- `draw_header`: kept or dropped per decision 2.
- Geometry: `mg_metrics_for` gets a `minimal` input (new field in `mg_screen` or an extra argument; decide in the
  code with the other callers listed by grep before editing): `bx = by = 0`, `chev_gap = 0`, `icon_gap = 0`,
  `hpad` unchanged (text must not touch the screen edge). `mg_width_t` receives `icon_w = 0` and `chev_w = 0`
  already; row height stays the font height plus the existing `row_pad` so the speed users get the densest list the
  font allows (row_pad only; verify the number in the test).

### 3.3 App icons: the value `none`

`amarchy-app-icons = generic | amiga | none` (absent = generic). In `gui.c` `amiga_icons` becomes
`app_icons` with three values (new enum in `config/upconf.h`, parser `upconf_app_icons` in `upconf_rows.c`, one
place, tested). `none`: no icon column for any row (`g->icon_w = 0`), no `ig_open`. Minimal mode implies `none`
for its rows: `icon_w = 0` is forced by `minimal` regardless of the key, and the key is not rewritten.
Prefs: `am_icons` is a 0/1 check gadget today; saving it back would ERASE a hand-set `none` (it writes `amiga` or
absent). So the Prefs row must be tri-state: `PG_AM_ICONS` becomes a `PGK_CYCLE` with `Generic`, `Amiga`, `None`
(full words), stored in `am_icons` as 0, 1, 2, written `""`, `amiga`, `none` (the key stays absent for generic, same
reason as above). Existing test lines `tests/test_prefs.c:508, 548-565` are updated, not duplicated.

### 3.4 Prefs Amarchy page: the mode cycle

A new `PGK_CYCLE` row on the Amarchy page (`prefs/prefs_ui.c` near line 201; ids after `PG_AM_ICONS = 146` in
`prefs/prefs_ui.h:44`), label `Menu mode`, values `Drawn`, `Text`, `Minimal` (full words), help text "Drawn: icons and
border. Text: the plain terminal menu. Minimal: text only, two colours." Backing field `am_mode` (0, 1, 2) in
`prefs/prefs_core.h:54`, read in `prefs_core.c:76` with the same parser as the commodity (`upconf_menu_style`
semantics, no second parser), written as `graphical` / `text` / `minimal` under the key chosen in decision 1; drawn is
written as ABSENT (key table pressure, see section 2). Dirty check `prefs_core.c:602` gets the field.

## 4. Why not the alternatives

- A fourth program (`amarchy-min`): second binary, second layout, second event loop; the drawn menu already has the
  window, hotkeys, filter and index. Mode flag in the one program is cheaper and cannot drift.
- Making the text menu faster instead: owner asked for a mode in the drawn program; the text menu starts an XCON
  window and libupkgui, which is what the speed users want to avoid (cold-start numbers: AMARCHY_TIMING marks,
  `amarchy/atiming.h`).

## 5. Risks

- R-A `gui.c` and `menu_geom.c` overlap with APP-ICONS-CACHE and GROUP-DRAWER-ICON: start from `origin/main` after
  both merge.
- R-B Key table: eight profiles full leave no slot; writing `minimal` as a new key adds one slot only when set, as
  with `amarchy-app-icons`; drawn and generic stay absent. Test: the ninth-profile test with both set.
- R-C Two pens on a 4-colour (2 bitplane) Workbench: contrast of selected and unselected rows is guaranteed by the swap
  only when fg and bg differ; if they are equal the selection is invisible. Test: equal fg and bg falls back to
  pens 1 and 0.
- R-D Vertical size: removing the border changes `chrome` in `mg_layout:119`; the window height test in
  `tests/test_keymap.c` (geometry cases) gets one minimal row.

## 6. Checklist rule

Each item is ticked only with its test passing. Running count is reported at every tick.

## 7. Phases and checklist (total 14)

### Phase H: host
- [ ] H1 `UC_MENU_MINIMAL` and the `minimal` value in `menu_style_row`. Test: `menu_style_parse` gains `minimal`,
  `minimal` then `text` (last wins), bogus stays graphical.
- [ ] H2 `cx_menu_cmd` keeps `minimal` on `amarchy-gui`, `other` still opens the text menu. Test: `menu_style_commands`
  rows for `UC_MENU_MINIMAL` (four kinds).
- [ ] H3 `menu_theme_minimal`: two distinct colours over the five roles, selection swapped, equal fg and bg falls back.
  Test: `theme_minimal_two_pens` counts distinct rgb across `rgb[MT_ROLES]` == 2.
- [ ] H4 `mg_metrics_for` minimal: `bx = by = 0`, `chev_gap = 0`, `icon_gap = 0`; `mg_layout` height and width with
  `icon_w = 0`. Test: `geom_minimal` asserts the window is narrower by `2*bx` and `icon_w + icon_gap` than the drawn one.
- [ ] H5 `UC_ICONS_*` enum and `upconf_app_icons` with `none`. Test in `test_keymap.c` or `test_upconf.c`.
- [ ] H6 prefs: `am_icons` tri-state, `am_mode` field, read and write, dirty check, profile-table pressure case.
  Test: `test_prefs.c` round trip: hand-written `none` survives a load, edit of another field and save.
- [ ] H7 `prefs_ui.c`, `prefs_ui.h` rows and ids; `upprefs.c` getter/setter cases (`upprefs.c:253-301`).

### Phase G: amarchy-gui
- [ ] G1 `minimal` flag read in `index_drawers`; `pens_get` applies H3; `draw_full` skips the border; layout block
  forces `icon_w = 0` and `chev_w = 0`; `draw_row` has no chevron branch for minimal.
- [ ] G2 sentinel: `AMARCHY_MODE` published beside `AMARCHY_DRAWS` (`gui.c:446-460`) as 0/1/2 plus
  `AMARCHY_PENS` = number of distinct pens obtained (2 in minimal).
- [ ] G3 `make amiga` rc 0, warnings as errors; `make test ONLY=keymap` and `ONLY=prefs` green.

### Phase R: rig (060 rig, OS 3.2)
- [ ] R1 rig row `minimal` in `tools/rig/amarchy_menu_rig.py` (section 9); `rig.py start --max --os 32 --purpose
  "vtcon-05: minimal menu"`; rig stopped at the end.
- [ ] R2 PASS recorded in the ledger with the sentinel numbers.

### Phase D: docs
- [ ] D1 `dist/up-term.conf` (line 30 and the app-icons comment), `dist/README.txt` (702-710), `man/amarchy.1` (47),
  RULES.md:59-68 name the mode and `none`; `tests/test_amarchy_docs.py` passes.
- [ ] D2 ledger row MENU-MINIMAL closed with commit hashes.

## 8. Reachability (one test per feature)

One host test, `minimal_mode_reaches_the_program`, through the real entry points of the feature, in
`tests/test_keymap.c`: (1) parse a upconf text `menu-style = minimal` with `upconf_menu_style` -> `UC_MENU_MINIMAL`;
(2) build the command with `cx_menu_cmd` for `KA_MENU` -> `"UP-Term:bin/amarchy-gui" menu <node>`, `text == 0` (proves
the hotkey path starts the drawn program, not the text one); (3) run the layout the program runs: `mg_metrics_for`
and `mg_width_t` with the minimal input, assert the window is exactly `2*bx + icon_w + icon_gap + chev` narrower than
the drawn layout of the same tree (numbers computed in the test from the drawn metrics, second source: the same tree
through `mn_children` item count); (4) `menu_theme_minimal` over the roles from `menu_theme_roles_fb` -> exactly 2
distinct colours and `MT_SELBOX == MT_TEXT`, `MT_ACCENT == MT_BG`. Sentinel: the test counts the tree items it laid
out and asserts the count equals `mn_children` for that node, so the loop provably ran. Fail-before: no
`UC_MENU_MINIMAL` symbol, the test does not build; with `cx_menu_cmd` unchanged `minimal` falls to graphical and (1)
fails on the parse. Prefs round trip is the second place a behaviour lives and is tested there (H6), not here. The
`none` parser (H5) and the pen merge (H3) are tested where they live.

## 9. Rig check (Amiga, once)

Row `minimal` in `tools/rig/amarchy_menu_rig.py`, pattern of `r_app_icons` (line 389): write `X9_CONF` with
`menu-style = minimal` (no `amarchy-app-icons` line needed), open the menu through the real Left Amiga + Space hotkey,
read the program's sentinels from the env: `AMARCHY_MODE == 2`, `AMARCHY_PENS == 2`, `AMARCHY_ICON_LOADS == 0`, and
`AMARCHY_DRAWS >= 1`. One screenshot read by a haiku agent: "is there a coloured border or any icon or '>' mark; how many
distinct colours are in the window?" PASS = no border, no icon, no chevron, two colours (selected row inverted).
A second step switches the key to `menu-style = graphical` and repeats: `AMARCHY_MODE == 0`, pens above 2, icons present
(the failure the owner reports if the mode sticks).

## 10. Owner eye check (after deploy, LOCAL on the A1200)

Prefs, Amarchy page, set Menu mode to Minimal, Save; press Left Amiga + Space. PASS = a plain list of words, no icons,
no border, no arrows, selected row inverted, opens faster than Drawn; switch back to Drawn and the border and icons
return. Also: set App icons to None, mode Drawn: rows keep the border but lose the icons.

## 11. Open decisions for the owner (recommended first)

1. Key name: reuse the existing `menu-style` key with a third value `minimal` (recommended: one key already picks
   the menu program, the commodity and `cx_menu_cmd` read it, and a second key `amarchy-menu-mode` would have to be
   kept consistent with it forever). Alternative: the new key `amarchy-menu-mode = drawn | text | minimal` as
   decided 22:03, with `menu-style` kept as a legacy alias read first; costs two parsers and a precedence rule.
2. Header in minimal: keep the node name line (recommended: without it a sub-level has no context; the header is
   one row of plain text in the foreground pen). Alternative: drop it for the shortest window; costs the "where am
   I" cue after drilling down.
3. Group marker without the chevron: append a plain ASCII `>` to the label text (recommended: still pure text, no
   icon or art, a user must tell a drawer from an app; it is one character in the label buffer, no extra draw).
   Alternative: no marker at all; costs ambiguity at the Apps root.
4. Default pens: the terminal's `fg` and `bg` upconf keys, falling back to the screen's pens 1 and 0 (recommended:
   matches the terminal behind it and gives two real colours on a 2-colour screen). Alternative: always screen pens
   0 and 1, ignores the user's colours.
5. The handler text menu (`amarchy-menu`, libupkgui): unchanged by this plan (recommended: it already has no icons,
   and the mode is a drawn-program mode). Alternative: also read `minimal` there and drop the selection bar colour;
   it runs in an XCON window with the terminal's colours, so there is little left to remove.
6. Prefs `None` for app icons as a third entry of the existing row, changing the check gadget to a cycle
   (recommended: otherwise saving Prefs erases a hand-set `none`). Alternative: keep the check gadget and preserve an
   unknown value on save; costs a hidden state the page cannot show.

### Owner decisions 2026-10-25 23:30 (AskUserQuestion, vtcon-05)

1. Key: extend the existing `menu-style` with the value `minimal`; no `amarchy-menu-mode` key. This replaces the
   22:03 decision on the strength of the new fact that `menu-style` already exists. (Owner, 2026-10-25)
2. Header: kept in minimal mode, plain text in the foreground pen. (Owner, 2026-10-25)
3. Group marker: a plain ASCII `>` appended to the label. (Owner, 2026-10-25)
4. Default pens: the terminal's `fg` and `bg` keys, falling back to screen pens 1 and 0. (vtcon-05 default,
   2026-10-25: implementation choice.)
5. Handler text menu unchanged. (vtcon-05 default, 2026-10-25.)
6. Prefs app icons row becomes a cycle Generic / Amiga / None; the check gadget goes. (vtcon-05 default,
   2026-10-25: required so Save cannot erase `none`.)
All six items closed; the plan starts with the checklist as written.
