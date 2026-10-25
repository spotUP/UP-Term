---
date: 2026-10-25
topic: Amarchy drawn menu, icons for the kind groups (games, demos, music) and for scanned drawer groups
status: plan (owner reviews before any code)
research: thoughts/spot/todos.md, vtcon-05 rows GROUP-DRAWER-ICON (2026-10-25 22:05, 23:45, 23:48, 00:05)
ledger: thoughts/spot/todos.md, section 2026-10-25 Amarchy session vtcon-05
repos: vtcon (dist/menu-icons, amarchy/menu_icon.c and .h, amarchy/gui.c, tests/test_keymap.c, one rig row)
---

# Plan: drawer and kind icons in the drawn Amarchy menu

## 1. Goal and finish line

Owner, 2026-10-25 22:05: "the groups needs a drawer/dir icon they have no icon now". Done = in the drawn menu
(amarchy-gui) every group row of the Apps tree shows a glyph: the kind groups Games, Demos and Music each their own,
every scanned drawer group (Apps > Utilities and the like) a folder glyph; index rows shown under Games stop falling
back to the Apps glyph. Every checklist item in section 6 ticked, one reachability test (section 7) green, one rig
check (section 8) PASS. Size: one sonnet agent, host work plus one rig run.

## 2. Cause (read from the code, 2026-10-25)

- Group rows of the index levels are made by `add_group` (`amarchy/index_core.c:372`): `row = MN_ROW_GROUP`, `seg` =
  the category segment. Two callers: the root level adds one group per non-empty kind from
  `kind_node[IX_NKINDS] = { "apps", "games", "demos", "music", "settings" }` (`index_core.c:297`, call at :495); the
  scanned drawers of a level call it with the drawer's category (call at :485).
- `draw_icon` (`amarchy/gui.c:348`) resolves a non-index item with `mn_child_node(g->node, it, id, ...)`
  (`amarchy/menu_tree.c:85`: `node.seg`), then `mi_find(id)` (`amarchy/menu_icon.c:26`). The drawer ids
  (`apps.utilities`, `games.shoot`) and the kind ids `games`, `demos`, `music` have no row in `dist/menu-icons`
  (36 lines; `apps` is the only kind with a row), so `mi_find` returns 0 and `draw_icon` returns with the icon column
  empty. `settings` is covered by hand-written rows, `apps` has U+F003B.
- Index rows (`MN_IS_INDEX`) use the level's own id when `mi_find(g->node)` hits, else `apps`. Under `games`,
  `demos`, `music` there is no hit, so those rows show the Apps glyph (found by the APP-ICONS-GENERIC agent).
- Violated invariant: the icon column of a row is empty only for a node that has no glyph by design; a group of
  the index tree has none by accident, because the glyph table was written for the static `[menu]` tree only.

## 3. Facts the fix rests on (each verified 2026-10-25)

- Where the table lives: `dist/menu-icons`, one line `id = U+XXXX ; name`; `tools/gen_menu_icons.py` parses it
  (`parse_table`: id matches `[a-z0-9.]+`, 4 to 6 hex digits, 1 to 256 rows; 36 rows now) and `Makefile:262-265`
  builds `build/amiga/menu_icons.inc` from it and `$(SYMBOLS_TTF)`. The converter raises `U+%04X is not in the font`
  (gen_menu_icons.py:73) for a missing glyph, so the build itself proves a code point exists.
- One font file carries every glyph: `build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf` (Makefile:11, fetched and
  SHA-256 checked at Makefile:297-310). Looked up in its cmap with fontTools:
  U+F11B = fa-gamepad, U+F008 = fa-film, U+F001 = fa-music, U+F07B = fa-folder, U+F0FCE = md-movie_open.
  CORRECTION to the ledger: the ledger row writes md-movie-open as U+F0FD8; in this font U+F0FD8 is `md-protocol`.
  The right code point for md-movie_open is U+F0FCE.
- `mi_find(id)` returns the glyph from the generated page by linear search of `mi_ids` (`menu_icon.c:26-33`); the
  table cap is 256 rows (`MI_COUNT`), 36 used, so four more rows fit.
- `draw_icon` is in `gui.c`, an Amiga-only file, so the resolution cannot be tested on the host where it stands.
  The existing host test is `tests/test_keymap.c` near 1145-1200: it walks every `[menu]` node of `dist/keymap` and
  asserts `mi_find(node)`, and walks every table entry asserting nonzero coverage at 16 and 32 pixels.

## 4. Fix

### 4.1 Rows in `dist/menu-icons`

```
games   = U+F11B   ; fa-gamepad
demos   = U+F008   ; fa-film
music   = U+F001   ; fa-music
drawer  = U+F07B   ; fa-folder
```

The row comment at the top of the file ("a node without an entry leaves its icon column empty") gets one sentence:
`drawer` is the glyph of any index group with no entry of its own.

### 4.2 One resolution function, `mi_item_id`, in `amarchy/menu_icon.c`

Single source of truth: the id resolution leaves `draw_icon`.

```
void mi_item_id(const char *node, const mn_item *it, char *id, int size);
```

Rules, in order:
1. `MN_IS_INDEX(it)`: `node` when `mi_find(node)` hits, else `"apps"` (today's behaviour, now also right for `games`,
   `demos`, `music`).
2. otherwise `mn_child_node(node, it, id, size)`; if the item is a group of the index tree (`MN_IS_GROUP(it)`) and
   `mi_find(id)` misses, the id becomes `"drawer"`.

`menu_icon.h` includes `menu_geom.h` already; it must also see `menu_core.h` for `mn_item`. Both are host files.
`draw_icon` keeps only the `ig_draw` attempt for `amiga_icons` index rows (needs `ixs`, Amiga-side) and then calls
`mi_item_id` for everything else. Nothing else in `gui.c` changes.

The `[menu]` groups of `dist/keymap` (apps, style, window, learn, system, install, trigger and the rest) all have rows
today (the test at test_keymap.c:1165 enforces it), so rule 2 never turns a static group into a drawer.

## 5. Why not the alternatives

- A drawer fallback inside `mi_find`: the table lookup would then lie ("found" for an unknown id) and the
  every-node test at test_keymap.c:1165 would stop catching a missing static row. The fallback belongs to the
  resolver, not the lookup.
- One row per scanned drawer name: drawers come from the user's disk; a table cannot list them.

## 6. Phases and checklist

Each item is ticked only with its test passing. Running count is reported at every tick (total 9).

### Phase H: host (no rig)
- [ ] H1 four rows in `dist/menu-icons`; `make` builds `menu_icons.inc` (proves the four code points exist).
- [ ] H2 `mi_item_id` in `amarchy/menu_icon.c`, declared in `menu_icon.h`. Test: `menu_item_id_cases`.
- [ ] H3 `draw_icon` calls `mi_item_id`; the inline resolution is deleted (grep `strcpy(id, "apps")` in `gui.c`: zero).
- [ ] H4 `tests/test_keymap.c`: `mi_find` for `games`, `demos`, `music`, `drawer`; blank-icon walk still green
  (it covers the new rows by itself).
- [ ] H5 `make test ONLY=keymap` green; vbcc `make amiga` rc 0 with warnings as errors.

### Phase R: rig (060 rig, OS 3.2)
- [ ] R1 one row `group_icons` in `tools/rig/amarchy_menu_rig.py` (section 8). Start with
  `rig.py start --max --os 32 --purpose "vtcon-05: group drawer icons"`; stop the rig at the end.
- [ ] R2 PASS recorded in the ledger with the sentinel numbers.

### Phase D: docs and ledger
- [ ] D1 `dist/README.txt` icon-table paragraph (near line 702-710) lists the four new ids; `tests/test_amarchy_docs.py`
  still passes. Ledger row ticked with the commit hash.
- [ ] D2 ledger: GROUP-DRAWER-ICON row closed; MENU-MINIMAL unblocked (same `gui.c`).

## 7. Reachability (one test per feature)

One host test, `menu_item_id_cases` in `tests/test_keymap.c`, drives the real top-level path of the feature: it parses
`dist/keymap` with `keymap_parse`, then builds a real index with `ix_load_text` from a fixed text of rows (one game
under `Games/`, one demo, one song, one app in `Apps/Utilities/`), takes the items from `mn_children` for the nodes
`""`, `games`, `apps` (the same calls `amarchy-gui` makes), and passes each item to `mi_item_id`. Sentinel: a counter
in the test that counts the items resolved, asserted equal to the number of items `mn_children` returned (so the loop
provably ran, not an empty list). Asserts, with the spec's numbers:
- group `games` at root -> id `games`, group `demos` -> `demos`, group `music` -> `music`;
- scanned group `utilities` at node `apps` -> `drawer`;
- index row at node `games` -> `games`; index row at node `apps` -> `apps`; index row at an unknown node `foo` -> `apps`;
- static leaf `style.theme` -> `style.theme` (unchanged), static group `style` at root -> `style`;
- every resolved id is nonzero in `mi_find` (no empty icon column left in the tree).

Fail-before: with the four rows absent and the function undeclared the test does not build; with the function
present and the rows removed, the first group case fails (`mi_find(games)` is 0). Revert of the resolver rule 2 makes
the `utilities` case return `apps.utilities` and fail. Every other behaviour (the size arithmetic, coverage, the
table walk) stays where it lives in the existing tests.

## 8. Rig check (Amiga, once)

Owner-visible effect on a real drawn menu. Row `group_icons` in `tools/rig/amarchy_menu_rig.py`, pattern of
`r_app_icons`: a fixed index (one game, one demo, one song, one app in a drawer `Utilities`) is staged as the cache the
program reads, then amarchy-gui opens the root. The check reads the program's own sentinel (`AMARCHY_DRAWS`,
`sentinel_rows` list) and one screenshot read by a haiku agent: question "does every group row at the root show a
glyph left of its label, and do the four glyphs differ?" PASS = four distinct glyphs; the failure reported by the
owner = blank column. Steps for the owner afterwards: LOCAL on the A1200 after deploy, press Left Amiga + Space, open
Apps; PASS = Games, Demos, Music each show their own glyph, a drawer group shows a folder.

## 9. Risks

- R-A `gui.c` is touched by APP-ICONS-CACHE (ig_open call and sentinel line): start from `origin/main` after
  `am-iconcache` merges; the overlap is small.
- R-B Link dependency: `menu_tree.c` (mn_child_node) and `menu_icon.c` are both already in the host test link line (Makefile:180, 182) and amarchy-gui already calls `mn_child_node`, so `mi_item_id` adds no new object file.
- R-C Glyph choice is taste; the table is one line per id, a veto costs one edit.

## 10. Open decisions for the owner (recommended first)

1. Demos glyph: fa-film U+F008 (recommended: same Font Awesome family as games and music, simple at 16 pixels).
   Alternative md-movie_open U+F0FCE (Material Design, heavier, different style from its neighbours).
2. Settings sub-groups and any other index group below Settings: take the `drawer` glyph (recommended: one rule, no
   special case). Alternative: a separate `settings` row for them; today `settings` already has its own icon, so this
   only matters for deeper settings drawers.
3. A scanned drawer named like a kind (a drawer `Music` below Apps): its id is `apps.music`, not `music`, so it takes the
   `drawer` glyph (recommended: the kind glyphs belong to the root kind groups only, no depth check needed).
   Alternative: also give it the music glyph; costs a name match in `mi_item_id`.

### Owner decisions 2026-10-25 23:30 (AskUserQuestion, vtcon-05)

1. Demos glyph: fa-film U+F008. (Owner, 2026-10-25)
2. Settings sub-groups and deeper settings drawers take the `drawer` glyph; no separate `settings` row. (vtcon-05
   default, 2026-10-25: implementation choice, one rule.)
3. A scanned drawer named like a kind (`apps.music`) takes the `drawer` glyph; kind glyphs belong to the root kind
   groups only. (vtcon-05 default, 2026-10-25.)
All three items closed; the plan starts with the checklist as written.
