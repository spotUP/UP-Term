# AmTile LAYOUT command: layouts beyond master-stack (research, facts only)

Plan of record: thoughts/shared/plans/2026-10-28-amarchy-autotile.md (D5 line 19: "master-stack only in v1; LAYOUT command reserved for later"). Other research docs in this directory carry no frontmatter, only a heading and prose; this file follows that.

## 1. tile_core (amarchy/tile_core.c 88 lines, tile_core.h 50 lines)

- Layout function (tile_core.h:43): `void tile_master_stack(const win_screen *s, int gap, int ratio_pct, int n, const tile_win *in, win_box *out)`. Defined tile_core.c:61-82.
- Inputs: `win_screen {width,height,title}` (win_core.h), gap, ratio_pct (20..80, TILE_RATIO_* tile_core.h:11-13), n, `in[]` ordered master first (each tile_win: id, flags TW_*, title, box with minw/minh/maxw/maxh; layout ignores left/top/width/height, tile_core.h:16-21). Output `out[n]` win_box. There is NO master-count input and no layout-id input.
- Computation (tile_core.c:61-82): work area x0=gap, y0=title+gap, w=width-2*gap, h=height-title-2*gap. n==1: one cell filling the area. n>=2: mw=(w-gap)*ratio/100 on the left, stack column sx=x0+mw+gap, sw=w-gap-mw; stack cells ch=(h-(cells-1)*gap)/cells, last cell takes remainder pixels (`y0+h-y`).
- Shared cell helper `cell()` (tile_core.c:47-53): copies the window's box, sets left/top, width/height = `win_fit(cw, min, max, screen room from the corner)` (win_core.h:42). Any new layout can reuse `cell()` (static, so a new layout lives in tile_core.c or cell() is exposed).
- Other pure functions: `tile_classify` (c:8-17; float if no size gadget, backdrop, borderless, title "Amarchy", or fixed size), `tile_order_update` (c:28-44; closed ids dropped, survivors keep place, new ids go to FRONT in set order = new window becomes master), `tile_screen_switch` (c:55-60; differing screen id drops order, returns 0), `tile_state_on` (c:84-88).
- Host tests: tests/test_tile.c, 146 lines, harness.h macros CHECK / CHECK_INT (37 CHECK-family occurrences by grep). Fixture: `scr = {800,600,12}` (line 5), `mk(id,title)` builds a tile_win with TW_SIZEGADGET, min 50x30 (7-16), `check_box(b,l,t,w,h)` (18-24). Test functions: state_file, master_stack_boxes, min_size_beats_the_cell, classification, order_update, screen_switch; registered in `suite_tile()` (line ~138). Run: `make test ONLY=tile` (Makefile:91).

## 2. AmTile binary (amarchy/tile.c, 468 lines)

- Single instance, ARexx port AMTILE (tile.c:33). A second start forwards its argv line to the port (main 419-439); no args = "ON".
- Commands in `command()` (297-343): ON, OFF, TOGGLE, RETILE (reloads gap from amconf then retiles), RATIO <20..80>, GAP <0..64>, STATUS, QUIT; anything else returns rc 10. Parsing helpers `starts()` (64), `num_arg()` (74; one integer arg only, no word arguments).
- State (tile.c:55-61): g_on, g_quit, g_gap, g_ratio, g_order[TILE_MAX=32], g_norder, g_scr_id, g_retiles. No layout, nmaster or per-screen state. g_ratio is NOT persisted (resets to 55 on every start; only on/off and gap persist). The `Exchange` broker (347-405) maps Enable/Disable to ON/OFF.
- Layout call site: single, tile.c:267 `tile_master_stack(&sn->scr, g_gap, g_ratio, g_norder, in, out)` inside `tick(force)` (239-277). tick: front public screen (`front_scr`, 173), snapshot under LockIBase (`walk`, 209), classify, `tile_screen_switch` (257), `tile_order_update` (260), retile when order changed or force; boxes applied via `win_apply_box` under lock (268-273); `g_retiles++` (274).
- Order / new=master: order lives only in g_order; new windows land at index 0 (master) via tile_order_update. Windows never reorder by focus. There is NO focus/active-window handling in tile.c (grep "active" finds only a comment); no swap/promote command exists.
- Poll: timer.device 250 ms (TILE_POLL_MICROS, tile.c:35); retile only when the tiled set/order changed, or force (ON, RETILE, RATIO, GAP).
- STATUS format (tile.c:329-336): `"on|off <g_norder> <g_retiles>"`, e.g. `on 2 5`. Rig parses it (`tile_status()` amarchy_tile_rig.py:59-66). A new field must be appended to keep that parser valid or the parser updated.
- State file: ENV:Amarchy/tile and ENVARC:Amarchy/tile hold "on\n"/"off\n" (state_write 102-118); Amarchy reads it at boot (amarchy.c:89-99, runs `UP-Term:bin/AmTile ON`). Layout choice would need either a second file or a format change (`tile_state_on` only reads the first word, so extra words after a space would still parse as on: tile_core.c:86-88 accepts buf[2]==' ').

## 3. Amarchy wiring

- Menu row: dist/keymap:185 `window.tile = Tile windows | run:UP-Term:bin/AmTile TOGGLE`; icon dist/menu-icons:92 (`window.tile = U+F009`). Hotkey: dist/keymap:77 `super+alt+t = run:UP-Term:bin/AmTile TOGGLE ; Tile windows | window`. Installed via dist/install.dos:403, dist/parts.txt:56. These are the only rows sending AMTILE commands (grep of amarchy/, dist/, tools/rig/*.py).
- A new command is added as a keymap row of the same form (`run:UP-Term:bin/AmTile RATIO 60`, etc.) plus a menu-icons row; no C change needed in Amarchy for rows. The `window` scope token ends the hotkey row (menu scope names seen: window, workspace).
- Existing super+alt+ combos used: o f s r tab shift+tab t left right up down b d (keymap lines 66-102). Free letters with super+alt: a c e g h i j k l m n p q u v w x y z and digits (not verified against the parser: comment at keymap:64 says super+m and super+n are refused by the parser). Plain super+ letters used: k h t w q o z c v a f g p l e u i d; also super+plus/minus/return/space/period/comma/left/right/backspace/tab/shift+left..down (move). Note super+alt+left/right/up/down already mean win:size; super+shift+arrows mean win:move, so arrow combos near tile are taken.
- amconf key tile-gap: defined in prefs/settings_def.c:213 in `sd_hidden[]` (string, default "4", SD_AMARCHY, SDF_AMSTR, AO(AM_TILEGAP), hidden = no Prefs gadget); enum `AM_TILEGAP` amconf_core.h:22 (last before AM_NKEYS); `AM_TILEGAP_MAX 64` (h:53); reader `amconf_tile_gap(const am_prefs*)` amconf_core.c:92-108 (digits 0..max else default); called from tile.c:231-236 `gap_load()` (start and RETILE). Test: tests/test_amconf.c:72-84. To add a key (e.g. tile-layout, tile-ratio, tile-nmaster): add an enum before AM_NKEYS, an sd_hidden row (and SD_NHIDDEN count), a typed reader beside amconf_tile_gap, a test in test_amconf.c, call it from gap_load().
- Plan line 27 says ratio is ARexx only; no amconf key for it exists.

## 4. Rig (tools/rig/amarchy_tile_rig.py 360 lines, amarchy_tile_boxes.c 49 lines)

- Expected boxes: `expected(n)` (rig.py:153-158) runs the host helper `amarchy_tile_boxes SCREEN_W SCREEN_H TITLE GAP RATIO N [MINW MINH [MAXW MAXH]]` (built with `cc` from tile_core.c + win_core.c, rig.py:142-150) and parses `box i l t w h` lines, master first. The helper calls `tile_master_stack` directly (boxes.c:44): a new layout needs a layout argument/arg in the helper CLI and in `expected()`; OPT dict (gap, ratio) is filled in main() from --gap/--ratio (lines 331-343).
- Comparison: `tiled_boxes_match(n, secs)` (191) compares the multiset of boxes, so master position/order is not asserted.
- Steps list `STEPS` (323-325): start, row_on, boxes2, second_on, env_on, third, close_one, midapply, off, fourth_free, restart; each step is `s_name()` returning (ok, detail); `--only <name>` runs one (main 331+); state persisted in out/state.json. A new layout step = a new `s_xxx` function (send `AMTILE LAYOUT x` via `send('AMTILE', cmd)` line 48, then `tiled_boxes_match`) added to STEPS.
- Open helper: `open_win()` 160, `close_win()` 169, `tile_status()` 59, `wait_for()` 68.

## 5. win_core / win verbs (amarchy/win_core.h, win.c)

- Verbs (win_core.h:10-13): CYCLE, FRONT, BACK, ZIP, FULLWIDTH, FULLSCREEN, SAVE, RESTORE, MOVE, SIZE, FOCUS, CLOSEDRAWERS, SCREEN, TITLE.
- `win_fullwidth(b,s)` (win_core.c:156) and `win_fullscreen(b,s)` (166: the area below the title bar) operate on one active window's box; applied in win.c:231-232 on the ACTIVE window only. They ignore gap and take no window list, so they do not give a monocle of all windows. `win_fit` (win_core.h:42) is the clamp both share with tile_core. `win_pick(boxes,n,target,dir)` (h:72) is the geometric neighbour pick used by FOCUS/CYCLE (win.c ~250+); a tile-order-based focus/swap would not use it.
- ZipWindow (win.c:~223) toggles the window's zoom box; does not interact with AmTile (tile.c has no handling; a zipped window is moved on the next forced retile).
- win_apply_box / win_box_of: amarchy/win_apply.c, shared by win.c and tile.c (win_apply.h).

## 6. Candidate layouts: inputs present vs new (facts only)

Present for all: screen box, title, gap, n, ordered in[], per-window min/max, `cell()`, `win_fit`, g_gap, order with new=front.

- monocle (every window full box): needs no new numeric input (one box = master-stack n==1 formula, repeated for all n). New: layout selector; which window is on top (WindowToFront of the active/master one; no focus tracking in tile.c today); retile on focus change (poll only detects set changes, tile.c:261).
- columns (equal vertical splits): ch/cw arithmetic same pattern as the stack column, along x. New: selector only.
- rows: same along y. New: selector only.
- grid: cols = ceil(sqrt(n)) rule and remainder-pixel rule are new; no existing function. New: selector plus grid dimension rule.
- master-stack mirrored (master right): same ratio_pct and cells; x mirrored. New: selector or a mirror flag.
- master ratio adjust (grow/shrink): `RATIO n` absolute exists (tile.c:317-322, range 20..80, g_ratio not persisted). New: relative form (RATIO +n / -n, or separate GROW/SHRINK) in `num_arg` (accepts digits only) and a hotkey row; optional persistence key.
- master count (nmaster): NO input exists; `tile_master_stack` hard-codes one master. New: nmaster parameter in the signature, cells for several masters in the left column, command (NMASTER n / INC / DEC), STATUS/field, bounds (1..n), test and rig helper args.
- Any new layout also touches: tile.c:267 (single call site), amarchy_tile_boxes.c, rig `expected()`, tests/test_tile.c, dist/keymap + menu-icons rows, STATUS if the layout is reported.

## 7. Open questions for the owner

1. Which layouts ship (monocle, columns, rows, grid, mirrored, others)?
2. Command shape: `LAYOUT <name>` plus `LAYOUT NEXT` cycling, or separate commands per layout? Hotkey for cycling (super+alt+t stays TOGGLE?) and which key for ratio grow/shrink and nmaster?
3. Persistence: should layout, ratio and nmaster survive restart (ENV/ENVARC file format change, new amconf hidden key, or both)? Today only on/off and tile-gap persist.
4. Per-screen vs global layout (state is global today; tile_screen_switch drops order on screen change).
5. Monocle semantics: which window is shown (active vs master), and should focus changes re-raise (needs active-window tracking, absent today)?
6. Should the windows' order be changeable (promote/swap with master, rotate) in this work, or stay new=master only?
7. Should STATUS gain fields (layout, nmaster, ratio)? Appending is safe for the rig parser only if the parser is updated or the fields come last.
8. Menu rows: one cycling row or one row per layout?
