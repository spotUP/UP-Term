---
date: 2026-10-28
topic: AMARCHY-AUTOTILE - standalone tiling commodity driven by Amarchy
status: plan (owner decisions recorded)
research: thoughts/shared/research/2026-10-28_amarchy-autotile.md
ledger: thoughts/spot/todos.md, AMARCHY-AUTOTILE
---

# Plan: AmTile, an automatic tiling commodity

## Owner decisions (recorded, not to re-open)
- D1 Opt-in. Off until switched on (Amarchy menu row + hotkey).
- D2 Standalone commodity with its own Exchange entry and ARexx port. Works without Amarchy.
  Amarchy drives it over ARexx.
- D3 Layout math shared with amarchy/win_core.c, not duplicated.
- D4 (revised 2026-10-28 s18, owner) Tiles the FRONT screen (Workbench/DOpus or an UP-Term screen); was "only the
  UP-Term public screen", which tiled nothing on rig 7 / A1200 where UP-Term windows open on Workbench. Non-resizable windows, requesters, backdrop windows
  and the Amarchy menu window float.
- D5 Layout: master-stack (only layout in v1; LAYOUT command reserved for later).
- D6 A newly opened window becomes master; old master moves to stack top.
- D7 Gap 4 px default, Amarchy config setting.
- D8 Not running until first switched on. Amarchy starts it on toggle. On/off is remembered;
  Amarchy restarts it at boot when it was on.

## Implementation choices (mine, stated)
- Name: binary `AmTile` (UP-Term:bin/AmTile, beside the other Amarchy programs; there is no UP-Term:Amarchy drawer), broker "AmTile", ARexx port `AMTILE`.
- Master width ratio 55 % default, ARexx `RATIO <20..80>`.
- Window watch: timer.device poll every 250 ms (pattern: handler/pty_handler.c:168-200),
  walk screen->FirstWindow under LockIBase, compare the window set to the previous one.
  No Intuition open/close notification is assumed on 3.2.
- Single instance: when `AmTile <cmd>` starts and port AMTILE exists, it forwards `<cmd>` to the
  running instance and exits. So the Amarchy row is a plain run of `AmTile TOGGLE`; no new
  Amarchy action kind needed.
- State file: ENVARC:Amarchy/tile and ENV:Amarchy/tile, "on" or "off", written by AmTile on change.
- ARexx host: extract amarchy.c `rexx_serve` + port setup (amarchy.c:203, :301) into
  amarchy/rexx_host.c/.h, used by both Amarchy and AmTile. Client `rexx_send` (leaf.c:427)
  moves to the same module (no second copy).
- Commands: ON, OFF, TOGGLE, RETILE, RATIO n, GAP n, STATUS, QUIT.
  STATUS returns "on|off <tiled count> <retile count>" (retile count = rig sentinel).
- Floats rule (tile_core, pure): tile iff WFLG_SIZEGADGET set, not WFLG_BACKDROP, not
  borderless, title != "Amarchy", MaxWidth/MaxHeight allow the box (else float), not a
  requester window (no size gadget covers system requesters; document the rule).
- Box clamp: expose win_core.c:126 `fit` as `win_fit` in win_core.h; tile_core uses it.
- Hotkey: default `lamiga t` if no existing Amarchy binding clashes (implementer greps the
  binding table and records the result); else pick a free one and record it here.

## Phase 1 - tile_core (pure) + host tests
Files: amarchy/tile_core.c/.h (new), amarchy/win_core.c/.h (expose win_fit), tests/test_tile.c (new),
Makefile host test target (copy the test_win rule).
- `tile_classify(const tile_win *)` -> TILE / FLOAT.
- `tile_order_update(order[], n, current set[], m)`: removes closed, new windows go to front (D6).
- `tile_master_stack(const win_screen *, gap, ratio, n, const tile_win in[], win_box out[])`:
  n=1 fills work area minus gaps; n>=2 master left at ratio, stack splits the right column
  evenly; last stack cell takes remainder pixels; every box through win_fit with min/max.
  Work area = screen below title bar (s->title), gap on outer edges and between cells.
Success: `make test ONLY=tile` passes; asserts exact boxes for 800x600 title 12 gap 4 ratio 55
with n=1,2,3,4; a min-width window; float classification for each D4 class; order: new = front.

## Phase 2 - AmTile binary
Files: amarchy/rexx_host.c/.h (extracted), amarchy/amarchy.c + leaf.c (use it), amarchy/tile.c (new
commodity: broker from cx.c pattern, ARexx port, timer poll, apply via win.c apply_box path).
Shared apply: move `box_of`/`apply_box` from win.c into a module both link (win_apply.c) if
win.c is not linkable as is.
- `long __stack = 16384;` and "$STACK: 16384" cookie.
- On ON: retile now, start poll. OFF: stop poll, windows stay where they are. QUIT/Exchange kill:
  clean exit (close port with drain, broker, timer).
- Makefile target `amtile` from the `amarchy-index` template (Makefile:807); add to dist-base (:1400),
  dist-check stack line (:1578), tests/test_amarchy_stack.py MIN_BY_NAME, dist/parts.txt (:52-55),
  dist/install.dos copy (:403), tools/rig/rig.py stage_upterm tuple (:966).
Success: cross build clean; `make test` amarchy groups pass; refactor proof: existing Amarchy
ARexx host tests and tools/rig/amarchy_window_rig.py still pass on a rig.

Phase 2 as built (main, 2026-10-28):
- win.c `box_of`/`apply_box` moved to amarchy/win_apply.c as `win_box_of`/`win_apply_box`; ChangeWindowBox is
  called under LockIBase after a screen/window membership check, as win.c already did.
- AmTile tiles the FRONT screen (D4 revised): tile.c front_scr() = handler/frontscr.c lock_front_screen(), so any public screen (UP-Term, Workbench Screen under DOpus) works; a non-public front screen is not tiled. When the front screen changes, tile_screen_switch() drops the tracked order, so no boxes of screen A reach screen B. Rig-only: the live screen switch and the DOpus case.
- Exchange: the broker is Active only while tiling; Disable = OFF, Enable = ON, Kill = QUIT (state unchanged).
- Gap is TILE_GAP_DEFAULT until P3-3.

## Phase 3 - Amarchy wiring
- Menu row "Tile windows" under the existing window section, runs `UP-Term:bin/AmTile TOGGLE`
  (shows on/off from ENV:Amarchy/tile if the row model supports a state label).
- Hotkey default (see choices).
- Config key `tile_gap` (amconf_core) default 4; AmTile reads it at start and on RETILE.
- Boot: Amarchy at start runs `AmTile ON` when ENV:Amarchy/tile is "on".
Success: host tests for the config key and menu row; dist-check passes.

### Phase 3 as built
- P3-1 menu row `window.tile` ("Tile windows", `run:UP-Term:bin/AmTile TOGGLE`, existing KA_RUN kind) in
  dist/keymap and dist/menu-icons. The on/off label is SKIPPED: menu rows are static `Label | action`, no state label.
- P3-2 hotkey: `super+t` (lamiga t) CLASHES with `tab:new`; `super+shift+t` is `slash:/theme`. Chosen `super+alt+t`
  (free), bound as `run:UP-Term:bin/AmTile TOGGLE`.
- P3-3 key is `tile-gap` (hyphen, as the other keys), a hidden key (no Prefs gadget), default 4, digits 0..64, else
  the default (`amconf_tile_gap`). AmTile reads it with `amconf_load` in `gap_load()` at start and on RETILE.
  Rule: RETILE re-reads config, so config wins after RETILE; `GAP n` wins until the next RETILE.
  AmTile now links `$(AMARCHY_LEAF_DEPS)`.
- P3-4 `tile_boot()` in amarchy.c, after `cx_install` at boot: reads ENV:Amarchy/tile, `tile_state_on` (pure, in
  tile_core) then `act_run("UP-Term:bin/AmTile ON")` (async). Amarchy now links tile_core.c.
- Tests: tile (79), amconf (375), keymap (12335), settings_def (1456) pass. Cross builds Amarchy, AmTile,
  amarchy-menu, amarchy-gui rc 0. dist-check fails only in WasabiDaemonOption (wasabi pin hash note), not from this phase.

## Phase 4 - Reachability (rig 7)
New tools/rig/amarchy_tile_rig.py (template tools/rig/amarchy_window_rig.py), rig purpose
"Amarchy s17: autotile reachability".
1. Two UP-Term windows open. Fire the Amarchy menu row through the AMARCHY port (the real entry).
2. AMTILE STATUS shows "on 2 n" with n >= 1 (sentinel: code ran).
3. Window boxes equal tile_core numbers for the rig screen size (computed host-side by the same
   C code via a tiny host helper, or the same formula in the script fed from STATUS screen size).
4. Open a third window: within 1 s it is master, others stacked; retile count grows.
5. Close one: remaining two retile.
6. TOGGLE off: open a fourth, it is not moved.
7. Restart Amarchy with ENV on: AmTile running again (port present).
Stop rig 7 at end (no write running).

## Manual (owner)
- Look at tiling on rig 7 or the A1200: gaps, the feel of new = master, Exchange entry shows AmTile.

## Checklist
P1-1 win_fit exposed | P1-2 tile_classify | P1-3 tile_order_update | P1-4 tile_master_stack | P1-5 test_tile in make test
P2-1 rexx_host extracted, Amarchy + leaf use it | P2-2 AmTile broker + Exchange | P2-3 ARexx commands | P2-4 timer watch | P2-5 apply | P2-6 single-instance forward | P2-7 state file | P2-8 stack cookie | P2-9 Makefile + dist + parts + install.dos + rig.py + stack test
P3-1 menu row | P3-2 hotkey | P3-3 tile_gap config | P3-4 boot restart
P4-1 rig script | P4-2 rig run steps 1-7 pass
