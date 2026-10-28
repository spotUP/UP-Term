---
date: 2026-10-28
topic: AMTILE-LAYOUT - layouts, ratio, master count, promote for AmTile
status: planned (0 of 40)
research: thoughts/shared/research/2026-10-28-amtile-layout.md (f8904bf9)
base plan: thoughts/shared/plans/2026-10-28-amarchy-autotile.md (D5 "LAYOUT command reserved for later")
ledger: thoughts/spot/todos.md, rows "AMTILE-LAYOUT" (owner decisions)
---

# Plan: AmTile layouts

## Owner decisions (recorded, not to re-open)
- L1 Layouts, in this order and with these ids: dwindle (NEW, boot DEFAULT, like Omarchy `layout=dwindle`),
  master-stack, mirrored master-stack, columns, rows, grid, monocle.
- L2 Command `AMTILE LAYOUT <name>` and `LAYOUT NEXT` (plus `PREV`), plus one hotkey per layout.
- L3 Layout, ratio and nmaster persist across restarts as hidden amconf keys beside `tile-gap`.
- L4 Extras: ratio grow/shrink, master count (nmaster), promote active window to master.
- L5 Keys, the super+alt family: `l` cycle, `1..7` pick, `minus`/`equal` ratio, `j`/`k` nmaster, `return` promote.
  Omarchy keys clash (super+l Claude, super+minus font, super+shift+arrows move), so they are not used.
- L6 (owner's stated defaults, not asked) Layout is global (the front screen, as v1). Monocle shows the ACTIVE window on
  top. STATUS appends its new fields at the end. Menu rows: one per layout + "Next layout".
- L7 (owner, 2026-10-28) `AMTILE CYCLE NEXT|PREV` is built: activate and raise the next/previous window in tile order
  (works in every layout; the way to reach hidden windows in monocle). Keys super+alt+period (NEXT) and
  super+alt+comma (PREV); super+alt+tab stays `win:cycle`. Hotkey rows only, no menu rows.

## Implementation choices (mine, stated)
- Names (ARexx words, case-insensitive, exact): `dwindle`, `master-stack`, `mirrored`, `columns`, `rows`, `grid`, `monocle`.
- ONE layout entry point `tile_layout(kind, ...)` replaces `tile_master_stack` (removed; its 3 callers move: tile.c:267,
  tools/rig/amarchy_tile_boxes.c:44, tests/test_tile.c). Mirror is a layout KIND (`TL_MIRROR`), not a flag, so one id
  names one layout everywhere (command, hotkey, amconf, STATUS).
- Ratio and nmaster are plain global state, range ratio 20..80 (TILE_RATIO_MIN/MAX), nmaster 1..`TILE_NMASTER_MAX` 8.
- Relative steps (`RATIO +5`, `NMASTER -1`) CLAMP at the bounds and return rc 0; an absolute out-of-range value is rc 10
  as today.
- Every layout cell goes through `cell()` -> `win_fit`, so per-window min/max hold in every layout (as master-stack today).
  A window forced larger than its nominal cell is NOT compensated for: later cells use the nominal geometry
  (same as master-stack today; documented, tested for no crash only).
- Pure helpers go in tile_core (host-testable); the OS part (active window, raise, amconf write) in tile.c.

## Layout rules (the numbers tests and the rig assert)
Fixture screen 800x600, title 14, gap 4 -> work area x0=4, y0=18, w=792, h=578. Order list: in[0] = newest (master).
Box = (left, top, width, height). Ratio R = 55 unless stated.

Rules, all integer division, remainder pixels go to the LAST cell of a run:
- master-stack (nmaster nm, nm = min(nmaster, n)): nm >= n: all windows stacked in one full-width column (n cells,
  ch=(h-(n-1)gap)/n, last takes the rest). Else master column width mw=(w-gap)*R/100 on the left holding in[0..nm)
  stacked evenly; stack column sx=x0+mw+gap, sw=w-gap-mw holds in[nm..n) stacked evenly. nm=1 is byte-identical to
  today's tile_master_stack (existing assertions stay).
- mirrored: same, columns swapped: stack column on the left at x0 width sw, master column at x=x0+sw+gap width mw.
- columns: cw=(w-(n-1)gap)/n, in[0] leftmost, last cell width = x0+w-x.
- rows: ch=(h-(n-1)gap)/n, in[0] on top, last cell height = y0+h-y.
- grid: cols=ceil(sqrt(n)) (integer: smallest c with c*c>=n), rows=ceil(n/cols). Row-major, in[0] top-left. Row r has
  k cells (k=cols, the last row has n-(rows-1)*cols); a row's cells share the FULL width: cw=(w-(k-1)gap)/k, the last
  cell of the row takes x0+w-x (so an incomplete last row spreads out). Row height rh=(h-(rows-1)gap)/rows, the last
  row takes y0+h-y.
- monocle: every window gets the whole work area (x0,y0,w,h) through `cell()`.
- dwindle (Hyprland-like, force_split=2: the newer window goes right/bottom; deterministic, no focus needed). Process the
  windows OLDEST first (in[n-1] .. in[0]). Keep a remainder box B, starting as the work area. For each window i from n-1
  down to 1: split B along its LONGER side (bw >= bh: vertical split into left|right, else horizontal top|bottom; tie
  = vertical). The first part (left/top) takes F=(len-gap)*R/100 of the split length and is in[i]'s cell; B becomes the
  second part (right/bottom: start = first start+F+gap, length = len-gap-F). The last window in[0] gets what remains of
  B. n=1: the work area. So the ratio knob moves dwindle (R is the older window's share of each split); nmaster has no
  effect (exemption: nmaster only means something to master-stack and mirrored; documented in tile_core.h).
- Knob map (the exemption list): ratio moves dwindle, master-stack, mirrored; nmaster moves master-stack, mirrored;
  columns, rows, grid, monocle ignore both (written exemption, tested as "output unchanged").

### Worked numbers (800x600, title 14, gap 4, R 55, no limits)
dwindle (in[k] listed as box):
- n=1: in0 (4,18,792,578).
- n=2: split B=(4,18,792,578) vertical, F=(792-4)*55/100=433. in1 (4,18,433,578); in0 (441,18,355,578).
- n=3: in2 (4,18,433,578). B=(441,18,355,578): 355<578 so horizontal, F=(578-4)*55/100=315. in1 (441,18,355,315);
  in0 (441,337,355,259).
- n=4: in3 (4,18,433,578); in2 (441,18,355,315). B=(441,337,355,259): 355>=259 vertical, F=(355-4)*55/100=193.
  in1 (441,337,193,259); in0 (638,337,158,259).
master-stack nm=1 (existing): n=2 in0 (4,18,433,578), in1 (441,18,355,578).
master-stack nm=2, n=4: in0 (4,18,433,287), in1 (4,309,433,287), in2 (441,18,355,287), in3 (441,309,355,287)
  (ch=(578-4)/2=287).
mirrored nm=1, n=2: in1 (4,18,355,578), in0 (363,18,433,578).
columns n=3: cw=(792-8)/3=261: in0 (4,18,261,578), in1 (269,18,261,578), in2 (534,18,262,578).
rows n=4: rh=(578-12)/4=141: in0 (4,18,792,141), in1 (4,163,792,141), in2 (4,308,792,141), in3 (4,453,792,143).
grid n=3 (cols 2, rows 2): row0 cw=(792-4)/2=394: in0 (4,18,394,287), in1 (402,18,394,287); row1 one cell
  in2 (4,309,792,287). rh=(578-4)/2=287, last row y=309 h=18+578-309=287.
grid n=4: in0 (4,18,394,287), in1 (402,18,394,287), in2 (4,309,394,287), in3 (402,309,394,287).
monocle n=3: all (4,18,792,578).

## Interfaces (exact)

### tile_core.h / tile_core.c (amarchy/)
```c
#define TILE_NMASTER_MAX 8
typedef enum { TL_DWINDLE = 0, TL_MASTER, TL_MIRROR, TL_COLUMNS, TL_ROWS, TL_GRID, TL_MONOCLE, TL_COUNT } tile_layout_kind;

const char *tile_layout_name(int kind);                 /* "dwindle", "master-stack", "mirrored", "columns", "rows", "grid", "monocle"; NULL out of range */
int tile_layout_parse(const char *word, int *kind);     /* case-insensitive exact word, 1 ok / 0 unknown (word may end at ' ' or NUL) */
int tile_layout_step(int kind, int dir);                /* next (dir>0) / previous (dir<0), wrapping over TL_COUNT */

/* REPLACES tile_master_stack. nmaster is used by TL_MASTER / TL_MIRROR only, ratio_pct by dwindle and the two master kinds. */
void tile_layout(int kind, const win_screen *s, int gap, int ratio_pct, int nmaster, int n, const tile_win *in, win_box *out);

/* Moves id to order[0], the rest keep their order. 1 when id was found (changed or already first), 0 when not tiled. */
int tile_promote(long *order, int n, long id);
```
`tile_layout` switches on kind to static `master_stack(s,gap,ratio,nm,mirror,n,in,out)`, `columns`, `rows`, `grid`,
`dwindle`, `monocle`; all call the existing static `cell()` (tile_core.c:47). An unknown kind falls back to TL_DWINDLE.

### amarchy/amconf_core.h / .c, prefs/settings_def.c|.h
- enum before `AM_NKEYS`: `AM_TILELAYOUT, AM_TILERATIO, AM_TILENMASTER` (after AM_TILEGAP, amconf_core.h:22).
- `sd_hidden[]` rows copied from the tile-gap row (settings_def.c:213): `tile-layout` default "dwindle", `tile-ratio`
  default "55", `tile-nmaster` default "1" (all `SD_STRING, SD_AMARCHY, SDF_AMSTR, AO(AM_...)`, hidden = no Prefs
  gadget). `SD_NHIDDEN` 3 -> 6 (settings_def.h:99).
- Readers beside `amconf_tile_gap` (amconf_core.c:92), same style (digits only, range, else the default):
```c
const char *amconf_tile_layout(const am_prefs *p);   /* the raw word; "" -> default "dwindle"; tile_layout_parse validates (no tile_core dependency in amconf) */
int amconf_tile_ratio(const am_prefs *p);            /* 20..80 else 55 */
int amconf_tile_nmaster(const am_prefs *p);          /* 1..AM_TILENM_MAX (8) else 1 */
```
  with `#define AM_TILERATIO_MIN 20 / _MAX 80`, `AM_TILENM_MAX 8` in amconf_core.h next to AM_TILEGAP_MAX (h:53).

### amarchy/tile.c
State: `static int g_layout = TL_DWINDLE, g_nmaster = 1;` (g_ratio exists, tile.c:55-61), `static long g_last_raised;`.
- `gap_load()` (tile.c:231) is renamed `cfg_load()`: after `amconf_load(&p)` it sets g_gap, g_ratio=amconf_tile_ratio,
  g_nmaster=amconf_tile_nmaster, and g_layout from `tile_layout_parse(amconf_tile_layout(&p))` (unknown -> TL_DWINDLE).
  Called at start and on RETILE (config wins after RETILE; commands write through, so they agree).
- `num_arg` extension (tile.c:74, reuse, no second parser): new last parameter `int *rel`.
```c
static int num_arg(const char *cmd, int lo, int hi, int *out, int *rel);
/* rel NULL: as today, digits only, lo..hi.  rel non-NULL: an optional leading '+' or '-' is accepted; *rel = +1/-1/0.
 * With a sign the magnitude must be 1..(hi-lo) and *out holds the magnitude; without a sign lo..hi as before. */
```
  GAP passes NULL (unchanged). Plus `static int word_arg(const char *cmd, char *w, int cap)` (the first word after the verb)
  for LAYOUT.
- Commands (in `command()`, tile.c:297):
  - `LAYOUT <name>` | `LAYOUT NEXT` | `LAYOUT PREV`: unknown word rc 10. Sets g_layout, `cfg_save()`, `tick(1)` if on.
  - `RATIO n` (absolute 20..80, rc 10 outside) | `RATIO +n` | `RATIO -n`: `g_ratio = clamp(g_ratio +/- n, 20, 80)`.
    `cfg_save()`, tick(1).
  - `NMASTER n` (1..8) | `NMASTER +1` | `NMASTER -1`: clamped; `cfg_save()`; tick(1).
  - `CYCLE NEXT` | `CYCLE PREV`: `id = active_id()`; `next = tile_cycle(g_order, g_norder, id, +1|-1)` (pure, tile_core:
    index of id in order, step with wrap; id not tiled -> g_order[0]); `ActivateWindow` + `win_raise` on next; in monocle
    also g_last_raised = next. rc 5 when off or g_norder == 0; other word rc 10.
  - `PROMOTE`: `id = active_id()` (new helper: `LockIBase(0); aw = IntuitionBase->ActiveWindow; id = (long)aw; UnlockIBase`,
    same read as win.c:85 and win.c:107); `tile_promote(g_order, g_norder, id)`; rc 5 (warning, nothing done) when the
    active window is not tiled, else tick(1). Works while off? No: rc 5 "off" too (order is empty then).
- Layout call site tick() tile.c:267 becomes `tile_layout(g_layout, &sn->scr, g_gap, g_ratio, g_nmaster, g_norder, in, out)`.
- Monocle raise: `snap` gets `long active` (read in `walk()`, which already holds LockIBase). After the apply block
  (outside LockIBase), when g_layout == TL_MONOCLE: target = snap.active if it is in g_order else g_order[0];
  if target != g_last_raised, `win_raise((struct Window *)target)` and g_last_raised = target. This runs on every poll
  (not only on retile), so clicking/activating another window re-raises it. `win_raise` is a thin `WindowToFront` wrapper
  in amarchy/win_apply.c (grep win.c for the existing WindowToFront use first and move/reuse it, no second copy).
  Leaving monocle resets g_last_raised = 0.
- STATUS (tile.c:329) -> `"<on|off> <norder> <retiles> <layout> <nmaster> <ratio>"`, e.g. `on 2 5 dwindle 1 55`
  (new fields appended last).
- Persistence write path (the only one): `cfg_save()` = `amconf_load(&p); strcpy(p.val[AM_TILELAYOUT], tile_layout_name(g_layout));
  put_num(p.val[AM_TILERATIO], g_ratio); put_num(p.val[AM_TILENMASTER], g_nmaster); amconf_save(&p, 1, &failed);`
  Pattern and helper reused from `gt_dos_select` (amarchy/gthemes_dos.c:57: `amconf_load` / `amconf_save(&ap, 1, &failed)`
  writes ENVARC: then ENV:). Called from LAYOUT, RATIO and NMASTER only (user commands that change a value); never from
  GAP, RETILE or start. A failed save is logged on the console like the state file's and does not fail the command (rc 0).
  amconf_dos.c is already linked via `$(AMARCHY_LEAF_DEPS)` (Makefile:821); verify with the cross build.

### dist rows (key names verified against config/keymap.c)
Parser facts (cite): key names live in `knames[]` config/keymap.c:32-47: digits `1`..`0` (0x01..0x0A), `minus` (0x0B),
`plus` (0x0C, the =/+ key), letters, `return` (0x44). There is NO `equal` name. `spec_parse` config/keymap.c:566-615: qualifier
`super` is required, `alt` accepted, `ctrl`/mouse refused (rc 2). dist/keymap lines 66-102 list the super+alt keys in
use: o f s r tab shift+tab t left right up down b d; none of `l`, `1`..`7`, `minus`, `plus`, `j`, `k`, `return` is bound with
super+alt (super+minus, super+return and super+l exist WITHOUT alt and are distinct bindings, as super+alt+t / super+t
already coexist, tests/test_keymap.c:725-735).
Hotkey rows to add in the Window block of dist/keymap after line 77 (format `key = action ; label | scope`):
```
super+alt+l       = run:UP-Term:bin/AmTile LAYOUT NEXT          ; Next layout           | window
super+alt+1       = run:UP-Term:bin/AmTile LAYOUT dwindle       ; Layout dwindle        | window
super+alt+2       = run:UP-Term:bin/AmTile LAYOUT master-stack  ; Layout master-stack   | window
super+alt+3       = run:UP-Term:bin/AmTile LAYOUT mirrored      ; Layout mirrored master-stack | window
super+alt+4       = run:UP-Term:bin/AmTile LAYOUT columns       ; Layout columns        | window
super+alt+5       = run:UP-Term:bin/AmTile LAYOUT rows          ; Layout rows           | window
super+alt+6       = run:UP-Term:bin/AmTile LAYOUT grid          ; Layout grid           | window
super+alt+7       = run:UP-Term:bin/AmTile LAYOUT monocle       ; Layout monocle        | window
super+alt+minus   = run:UP-Term:bin/AmTile RATIO -5             ; Shrink master         | window
super+alt+plus    = run:UP-Term:bin/AmTile RATIO +5             ; Grow master           | window
super+alt+j       = run:UP-Term:bin/AmTile NMASTER -1           ; Fewer masters         | window
super+alt+k       = run:UP-Term:bin/AmTile NMASTER +1           ; More masters          | window
super+alt+return  = run:UP-Term:bin/AmTile PROMOTE              ; Promote to master     | window
super+alt+period  = run:UP-Term:bin/AmTile CYCLE NEXT           ; Next tiled window     | window
super+alt+comma   = run:UP-Term:bin/AmTile CYCLE PREV           ; Previous tiled window | window
```
Menu rows (dist/keymap near `window.tile`, line 185, and dist/menu-icons beside `window.tile` line 92):
`window.layout.next` "Next layout" + `window.layout.dwindle` ... `window.layout.monocle` (labels: "Layout: Dwindle",
"Layout: Master and stack", "Layout: Mirrored master and stack", "Layout: Columns", "Layout: Rows", "Layout: Grid",
"Layout: Monocle"), each `run:UP-Term:bin/AmTile LAYOUT <word>` / `LAYOUT NEXT`. Row ids follow the existing flat
`window.<id>` scheme (no sub-levels, e.g. `window.layoutnext`, `window.layoutdwindle`): the implementer copies how
`window.tile` is declared and uses ids without dots after `window.`. Icons: reuse `fa-th_large` U+F009 for all layout rows
except Next (U+F021 fa-rotate) and Monocle (U+F2D0 fa-window_maximize); check each glyph exists in the icon table the
menu-icons file already uses; else U+F009.

## Deviations list (kept current while building)
- D-1 `super+alt+equal` is not parseable (no `equal` in knames, config/keymap.c:32-47). The same physical key (=/+,
  0x0C) is named `plus`: bound `super+alt+plus` = RATIO +5. Listed to the owner in the report.
- D-2 (P1) win_fit lets a window's Min beat the room the screen leaves (existing behaviour), so the window carrying minw 300 may extend past the screen edge when its cell starts far right (e.g. columns, last cell). The min/max test asserts width >= 300 for it and inside-screen only for the other windows.
- D-3 (P1) `make test ONLY=tile` could not link the whole host binary while P2's amconf test (tests/test_amconf.c, amconf_tile_ratio/nmaster) was half-written; the tile suite was verified in an isolated build of test_tile.c + tile_core.c + win_core.c with the Makefile's flags (1551 checks, 0 failures). Re-run `make test ONLY=tile` once P2 lands.
- D-4 (P3) `num_arg` bounds the magnitude while parsing (stops above 100000) so a long digit string cannot overflow; `win_raise` also replaces both WindowToFront calls in win.c (one copy). CYCLE raises under IBase lock (win_raise only queues) and calls ActivateWindow after unlock, as win.c does; rc 5 also when the target died between order and lock. cfg_save logs failure with PutStr (AmTile has no console log helper).
- Others: filled in by the implementer when a row is refused or taken (nearest free key, state which).

## Phase 1 - tile_core layouts (pure) + host tests
Files: amarchy/tile_core.c/.h, tests/test_tile.c, tools/rig/amarchy_tile_boxes.c (call site only, so the host tree
builds), amarchy/tile.c:267 (call site only, TL_MASTER nm 1, so nothing else breaks).
Success: `make test ONLY=tile` passes with every worked number above asserted (box by box); existing master-stack
asserts unchanged at nm=1; min/max test per layout (a window with minw 300 in each of the 7 layouts: its box is >=300
wide, no other box missing, no crash, boxes inside the screen); knob test: ratio 55->65 changes dwindle, master-stack,
mirrored and does NOT change columns/rows/grid/monocle; nmaster 1->2 changes master-stack/mirrored only; n=1 and n=32 do
not crash; `tile_layout_parse` accepts each name in any case, rejects "", "x", "master"; `tile_layout_step` wraps;
`tile_promote` moves middle and last to front, returns 0 for an unknown id, keeps order of the rest.

## Phase 2 - amconf keys
Files: amarchy/amconf_core.h/.c, prefs/settings_def.c/.h, tests/test_amconf.c, plus every test that counts hidden keys
or AM_NKEYS (grep `SD_NHIDDEN`, `AM_NKEYS`, `tile-gap` in tests/ and prefs/; today's settings_def run asserts 1456).
Success: `make test ONLY=amconf` and `ONLY=settings_def` pass. test_amconf (copy test_amconf_tile_gap,
tests/test_amconf.c:72-84): defaults dwindle/55/1; `tile-layout = grid` round-trips through parse+serialize; ratio 19 and
81 and "x" -> 55, 20 and 80 accepted; nmaster 0, 9, "x" -> 1, 8 accepted; a key at its default is not written.

## Phase 3 - AmTile commands, state, persistence
Files: amarchy/tile.c, amarchy/win_apply.c/.h (win_raise).
Success: cross build of AmTile rc 0 (`make` target `amtile`, Makefile:819-821), stack cookie test
(tests/test_amarchy_stack.py) still passes; `make test ONLY=tile` and amconf pass; STATUS shape asserted by a host-visible
pure formatter? NO: STATUS is checked on the rig (P6). Code review items: GAP unchanged (num_arg rel NULL), relative
clamps, rc codes (10 bad, 5 not tiled).

## Phase 4 - dist rows
Files: dist/keymap, dist/menu-icons, tests/test_keymap.c (new rows: add `check_row` lines beside tests/test_keymap.c:725 for
every new `window.*` row, the hotkey lookups via `keymap_find(&km, KM_Q_SUPER|KM_Q_ALT, code)` for the 15 keys with
codes 0x28 (l), 0x01-0x07, 0x0B, 0x0C, 0x26, 0x27, 0x44, 0x38, 0x39, asserting each is found, action text correct, and the plain
`super+<key>` binding still its old action; update the window-level count CHECK_INT at tests/test_keymap.c:1569 (24 today;
the new value is MEASURED from the keymap builder `ix_level_t` output after the rows exist, then compared with a count of
`window.*` run rows in dist/keymap, not a guess).
Success: `make test ONLY=keymap` passes with zero skipped/refused rows (`km.skipped == 0` asserted for dist/keymap); kit
shadow keymap test (tests/test_rig_kit_shadow_keymap.py) and `make dist-check` unchanged apart from the known wasabi pin note.

## Phase 5 - rig code (no rig use yet)
Files: tools/rig/amarchy_tile_boxes.c, tools/rig/amarchy_tile_rig.py.
- boxes helper CLI becomes `amarchy_tile_boxes SCREEN_W SCREEN_H TITLE GAP RATIO N LAYOUT NMASTER [MINW MINH [MAXW MAXH]]`
  (LAYOUT = name; calls `tile_layout`).
- rig: `expected(n)` reads OPT['layout'], OPT['nmaster'], OPT['ratio'] (defaults dwindle, 1, 55). `tile_status()` regex
  becomes `(on|off) (\d+) (\d+) (\S+) (\d+) (\d+)$` returning (on, tiled, retiles, layout, nmaster, ratio); every caller
  (grep `tile_status(`) adapted. `row_index()`/`fire_row()` (rig.py:82-106) take the row mark (`ROW_MARK` param) so any
  AmTile row can be fired by `RUN <n>`.
- New steps appended to `STEPS` before `off` (state persisted in out/state.json as today): see Reachability.
Success: `python3 -m py_compile`; the helper compiles with `cc` and prints the worked numbers above for 800 600 14 4 55
(1..4 dwindle, nm 2 master-stack) - run on the host, diffed against the plan numbers.

## Phase 6 - rig run (rig 7, 800x600 Workbench/DOpus, title 14, purpose "Amarchy s20: AmTile layouts")
Start `UPTERM_RIG=7 rig.py start --max --purpose "Amarchy s20: AmTile layouts"` (060, wasabi built first per ledger rules),
run the full STEPS once (all new steps are new, so a full run is the stated reason), stop the rig at the end with no
write running. Write the evidence to thoughts/shared/research/2026-10-28_amtile-layout-rig7.md.

## Reachability (REACHABILITY_PROTOCOL gates)
ONE wiring test through the real entry: step `layout_row` fires the dist/keymap row `LAYOUT NEXT` through port AMARCHY
as `RUN <row index>` (the dispatch a menu pick or the super+alt+l hotkey reaches, same as `s_row_on`), after `row_on`:
1. before: STATUS `on 2 n dwindle 1 55`; 2. fire the row; 3. sentinel: STATUS retiles > n AND layout == master-stack;
4. both windows' boxes equal the host helper's `master-stack` n=2 boxes; PASS = all three. FAIL looks like layout still
dwindle, or retiles unchanged (code did not run), or boxes of the old layout.
The hotkey itself (super+alt+l) is fired once with the key-injection path run 2 used for super+alt+t
(thoughts/shared/research/2026-10-28_amarchy-tile-rig7.md "Run 2"; implementer reads that section) and asserts the same
sentinel.
Every other behaviour is tested where it lives, via `AMTILE <cmd>` sent to the port (`send('AMTILE', cmd)`, rig.py:48) then
STATUS + `tiled_boxes_match`:
- one step per layout (`lay_dwindle`, `lay_master`, `lay_mirror`, `lay_columns`, `lay_rows`, `lay_grid`, `lay_monocle`):
  `LAYOUT <name>` with 3 windows, STATUS names it, boxes equal helper boxes (the multiset compare, rig.py:191, is kept;
  for monocle all three boxes identical, and the topmost window is asserted in the next step);
- `lay_next_prev`: NEXT from dwindle gives master-stack, PREV returns;
- `ratio`: `RATIO 65` then `RATIO -5` -> STATUS ratio 60 and dwindle boxes recomputed with 60; `RATIO 90` rc 10;
- `nmaster`: `LAYOUT master-stack`, `NMASTER 2` -> STATUS 2 and helper boxes (4 windows needed: opens a fourth, closes it
  again); `NMASTER 0` rc 10;
- `promote`: activate the second-newest window (wasabi activate/click on its title), `PROMOTE` -> that window's box is
  now the helper's box for index 0 (master) in master-stack; PROMOTE with the AmTile menu window... n/a;
- `cycle`: `LAYOUT monocle` with 3 windows; `CYCLE NEXT` three times: each time the front-most window is the next one
  in tile order (STATUS unchanged, the third NEXT returns to the start); `CYCLE PREV` once goes back one;
- `monocle_raise`: `LAYOUT monocle`, activate window A, then window B; after < 1 s the front-most window of the screen
  (wasabi window list order, helper from the existing rig `c.`/windows call) is the active one each time;
- `persist`: `LAYOUT grid`, `RATIO 60`, `NMASTER 2`; `AMTILE QUIT`; wait port gone; start `AmTile ON` fresh; STATUS shows
  `grid 2 60`; ENV:Amarchy/prefs holds the three keys; then `reset` step sends `LAYOUT dwindle`, `RATIO 55`, `NMASTER 1`
  (leaves the rig image at defaults);
- `layout_default`: on a rig whose prefs have no tile-* keys (the first run; the step first removes the three lines from
  ENV: and ENVARC: and restarts AmTile), STATUS is `dwindle 1 55` (boot default L1).
The existing steps (`boxes2`, `third`, `close_one`, `midapply`, `fourth_free`) now compare against `dwindle` boxes via
`expected()` (the boot default changed from master-stack).
Knob check (protocol section 2): every declared knob (layout, ratio, nmaster, promote) has a step that moves output; the
exemptions (ratio/nmaster ignored by columns, rows, grid, monocle; nmaster ignored by dwindle) are asserted in host test P1-10.

## Verification
Automated (host): `make test ONLY=tile`, `ONLY=amconf`, `ONLY=settings_def`, `ONLY=keymap`; cross builds of AmTile and
Amarchy rc 0; `python3 -m py_compile tools/rig/amarchy_tile_rig.py`; helper numbers diffed against this plan.
Automated (rig 7): P6 full run, every step OK.
Manual (owner): see the end of the plan.

## Checklist (43 items; tick in the open, report the running count)
P1-1 enum, names, parse, step | P1-2 `tile_layout` dispatch, master-stack rewritten with nmaster+mirror, `tile_master_stack`
removed, 3 callers moved | P1-3 columns, rows | P1-4 grid | P1-5 monocle | P1-6 dwindle | P1-7 `tile_promote` |
P1-8 worked-number tests per layout (all boxes above) | P1-9 min/max test per layout | P1-10 knob + exemption test | P1-11 `tile_cycle` + host test (wrap both ways, untiled id)
P2-1 enum + sd_hidden rows + SD_NHIDDEN | P2-2 three readers | P2-3 test_amconf cases | P2-4 count tests updated
P3-1 g_layout/g_nmaster + `cfg_load` (rename of gap_load) | P3-2 `num_arg` rel extension + `word_arg` | P3-3 LAYOUT/NEXT/PREV |
P3-4 RATIO relative + NMASTER | P3-5 PROMOTE + `active_id` | P3-6 monocle raise + `win_raise` | P3-7 STATUS fields |
P3-8 `cfg_save` write path | P3-9 tick call site on `tile_layout` | P3-10 cross build + stack test green |
P3-11 CYCLE NEXT/PREV
P4-1 15 hotkey rows | P4-2 8 menu rows | P4-3 menu-icons rows | P4-4 test_keymap rows + count | P4-5 dist-check + kit shadow test
P5-1 boxes helper args | P5-2 `expected` + `tile_status` + callers | P5-3 `row_index`/`fire_row` by mark |
P5-4 reachability step `layout_row` + hotkey | P5-5 seven layout steps + next/prev | P5-6 ratio, nmaster, promote steps |
P5-7 monocle_raise + cycle steps | P5-8 persist + reset + layout_default steps | P5-9 existing steps adapted to dwindle default
P6-1 rig 7 full run, all steps OK | P6-2 evidence doc written, rig stopped

Phases are sized for one sonnet agent each (P1, P2, P3, P4, P5, P6), run in order; P2 and P4 do not touch P1's files and may
run beside P1 when the agent cap allows. Each agent: no sub-agents, grep first and read ranges, build/test output to a log
file in the scratchpad and print only errors and the summary, one test run on the machine at a time
(`nice -n 19`, affected `ONLY=` targets only).

## Manual (owner)
On rig 7 or the A1200 with AmTile on: press super+alt+1..7 and see each layout with 3 to 5 windows; super+alt+l cycles;
super+alt+minus/plus grows and shrinks the master/first split; super+alt+j/k changes the master count in master-stack;
super+alt+return puts the active window in the master place; monocle shows the active window on top and a click on
super+alt+period/comma cycles the tiled windows (raises each in monocle); restart Amarchy and see layout, ratio and master
count come back. PASS = layouts look right and hotkeys respond; the A1200 is the final check only.

## Owner questions
None open. Q1 (monocle switching) answered 2026-10-28: build CYCLE (L7).
