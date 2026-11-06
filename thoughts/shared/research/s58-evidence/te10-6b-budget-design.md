---
date: 2026-11-06
topic: TE-10.6b flat.c per-screen pens, FLAT-C3 stack budget fix
status: design, not implemented
branch: te10-6b (worktree /Users/spot/Code/UP-Term/vtcon-te10-5b2b), commit 0ec78880 carries the failing version
---

# TE-10.6b budget fix: hooks read a table the main task keeps

## What fails

Commit `0ec78880` "flat.c: pen numbers per screen (TE-10.6b)" is not on origin/main. `make flat-budget` fails on it:

| Entry | Own frame | Deepest chain | Headroom | Limit |
|---|---|---|---|---|
| `_c2_disp_reg` on main `12e55076` | 20 | 308 | 76 | chain 384, headroom 64 |
| `_c2_disp_stk` on main `12e55076` | 4 | 292 | 92 | |
| `_c2_disp_reg` on te10-6b | 20 | 428 | -44 | FAIL |
| `_c2_disp_stk` on te10-6b | 4 | 412 | -28 | FAIL |

Also on te10-6b: `scr_epoch` (label l787) calls `_ipd_epoch` and `_ipd_epoch_of_screen` (not flat objects), and three
reachable frames are over 80 bytes (88, 96, 104).

## Violated invariant

FLAT-C3 (ledger row FLAT-C3, tests/test_flat_budget.py): the patched dispatcher entries run on the stack of whichever task
draws. Code reachable from them calls only functions of the flat objects (flat.s, flat_glyph.s, flat_core.s,
roundaa_core.s), externs named `_flat_*`, `jsr (aN)` callbacks and OS calls. Frame at most 80 bytes, chain at most 384,
headroom at least 64. The limits never move.

## Cause (three, one level: flat.c hook helpers)

1. `st_scr` / `st_rp` copy a whole `flat_state` (about 60 bytes) into a buffer on the hook's stack (`sb`, `kb`, `rsb`,
   `tsb`, `st` at the call sites).
2. `st_pens` holds a second whole `flat_state` (`o`) for `flat_state_for`.
3. `scr_epoch` calls `ipd_epoch()` / `ipd_epoch_of_screen()` from the hook. Both live in iconpen_dos.c and do
   `Forbid(); FindSemaphore(...); Permit();` on every draw.

## Fix

The main task of amarchy-flat owns a draw table. Hooks only look up a pointer. No `flat_state` copy and no iconpen_dos
call in any function reachable from the dispatchers.

### Pure part (amarchy/flat_core.h, flat_core.c; host tested)

```c
typedef struct flat_draw_slot {
    const void *screen;   /* 0 = free */
    unsigned epoch;       /* the pen record epoch of the screen at the last sync */
    flat_state st;        /* the look, with the four own pens of this screen (0 when it holds none) */
} flat_draw_slot;

typedef struct flat_draw_tab {
    const void *wb;       /* the Workbench screen at the last sync, 0 = unknown */
    unsigned wb_epoch;
    flat_state plain;     /* the look with the four own pens 0: a screen the table does not list */
    flat_draw_slot slot[FLAT_PENS_SLOTS];
} flat_draw_tab;

typedef struct flat_draw_scr { const void *screen; unsigned epoch; } flat_draw_scr;

void flat_draw_init(flat_draw_tab *d);
/* main task: plain = look with the four pens 0; one slot per listed screen, st = flat_state_for(look, pens, screen, epoch).
 * A screen already in the table keeps its slot (a hook may hold the pointer); a screen not listed frees its slot. */
void flat_draw_sync(flat_draw_tab *d, const flat_state *look, const flat_pens_tab *pens,
                    const flat_draw_scr *list, int n, const void *wb, unsigned wb_epoch);
/* hook: screen 0 or the Workbench screen = live (the shared state, as on main today: no lag on the Workbench);
 * a listed screen = its slot state; any other screen = plain. Pointer compares only, no copy. */
const flat_state *flat_draw_state(const flat_draw_tab *d, const flat_state *live, const void *screen);
/* hook, label path: the mirrored epoch (Workbench = wb_epoch, a listed screen = its epoch, any other = 0) */
unsigned flat_draw_epoch(const flat_draw_tab *d, const void *screen);
```

The table is one static in flat.c (BSS, about 2.8 KB), never on a stack.

### flat.c

- Workbench keeps the path main has today: hooks read `&r->st`; its four own pens are the Workbench's, written by
  `pens_run` into `r->st`. Check that `0ec78880` did not remove that write; if it did, put it back.
- Main task, new `draw_sync(r)`: under `LockPubScreenList` collect (screen, epoch) of every open public screen except
  the Workbench (`ipd_epoch_of_screen`; allowed here, this is the main task) into a static array, unlock, then
  `Forbid(); flat_draw_sync(...); Permit();` (hooks run in other tasks).
- Sync points: end of `pens_run` in every mode (start, the CTRL_D refresh, the 1 s watch; in the watch before
  `pens_reap`), and the CTRL_D branch also when the state is held.
- Hooks: `st_scr(r, scr)` / `st_rp(r, rp)` return `flat_draw_state(&g_draw, &r->st, scr)`. `st_pens` and the stack
  buffers go. The one draw copy that sets `side` (the call near flat.c:597) keeps the buffer main already has and takes
  the four pens from the pointer state. No function reachable from the dispatchers gains a `flat_state` local.
- Label hook (flat.c:865): `icl_tab_for(&icl_pc, w->WScreen, flat_draw_epoch(&g_draw, w->WScreen))`.
- `scr_is_open`, `pens_run`, `pens_reap`, `dim_refresh`, `dim_release` run on the main task and keep the live `ipd_*`
  calls. `scr_epoch` stays for them only; no hook path calls it.

### Behaviour

- Workbench: as on main, no lag.
- Other public screen: its pens and the look reach the hooks at the next sync: at once after CTRL_D, else within 1 s.
- A screen the table does not list (opened under 1 s ago, not public): look as set, the four own pens 0 (the stock
  fallback the hooks already have for pen 0).
- A screen closed and reopened at the same address is seen at the next 1 s watch (same as main today).

## Tests

Host, tests/test_flat_core.c, written first and seen failing:

1. `flat_draw_state` with screen 0 returns `live`.
2. With the Workbench screen returns `live`.
3. A listed screen with a pens slot: the four pens of that slot, every other field equal to the look.
4. A listed screen without pens: four pens 0, look equal.
5. An unknown screen: `&d->plain`.
6. Look change then sync: the new look in every slot and in plain.
7. Sync without a screen that was listed: that screen reads plain again.
8. A listed screen keeps the same slot address across syncs when other screens come and go.
9. `flat_draw_epoch`: Workbench = wb_epoch, listed = its epoch, unknown = 0.

Python guard: tests/test_flat_pens_per_screen.py follows the new shape (hooks call `flat_draw_state`, no hook path calls
`ipd_`). `make flat-budget` passes with the limits unchanged.

## Follow-up (TE-10.6c, not this fix)

The per-screen slot state is where the role pens of a themed foreign screen go later (plan pens by element instead of the
Workbench numbers), together with the screen filter for excluded screens and the menu / screen-bar paths.
