# TE-10.6a: per-screen pen tables (pure half)

Branch te10-6. No wiring into amarchy/flat.c (part b).

## Delivered API

flat_core.h / flat_core.c:
- `#define FLAT_PENS_SLOTS 40` (equals IP_SCREENS, compile-time checked in tests)
- `typedef struct flat_pens_tab { flat_pens slot[FLAT_PENS_SLOTS]; } flat_pens_tab;`
- `void flat_pens_tab_init(flat_pens_tab *t);`
- `flat_pens *flat_pens_tab_for(flat_pens_tab *t, const void *screen, unsigned epoch);` slot holding the identity, else a free slot, else 0 (table full; obtains nothing)
- `const flat_pens *flat_pens_tab_find(const flat_pens_tab *t, const void *screen, unsigned epoch);` 0 when absent
- `int flat_pens_tab_reap(flat_pens_tab *t, int (*is_open)(void *ctx, const void *screen, unsigned epoch), void *ctx);` forgets closed slots without ReleasePen (ColorMap went with the screen); returns count
- `void flat_pens_refresh_on(flat_pens *p, const flat_pen_ops *o, const flat_state *s, flat_state *store, const void *screen, unsigned epoch, ...);`
- `int flat_pens_follow_on(flat_pens *p, const flat_pen_ops *o, const flat_state *s, flat_state *store, int want, const void *screen, ...);`
- `int flat_ident_slot(const void *base, size_t stride, size_t scr_off, size_t epoch_off, int n, const void *screen, unsigned epoch, int allow_free);` one lookup rule for both tables; never releases or forgets

iclabel_core.h / iclabel_core.c:
- `#define ICL_TAB_SLOTS 40`, `icl_pentab`
- `void icl_tab_init(icl_pentab *t);`
- `icl_pencache *icl_tab_for(icl_pentab *t, const void *screen, unsigned epoch);` binds a free slot
- `const icl_pencache *icl_tab_find(const icl_pentab *t, const void *screen, unsigned epoch);`
- `int icl_tab_reap(icl_pentab *t, int (*is_open)(void *ctx, const void *screen, unsigned epoch), void *ctx);` reset without release

flat_pens_refresh / flat_pens_follow keep their signatures (forward to the _on variants with store == s).
pens_forget is null-safe, so release/verify accept a null flat_state.

## Null flat_state

The `store` argument of the _on variants. Null means: keep the pen numbers only in the flat_pens slot and do not write flat_state pen_*. The look block is still read through `s`. Non-Workbench slots pass store == 0 so a second screen never overwrites the Workbench flat_state pens (test_pens_non_workbench_slot_keeps_flat_state).

## Tests

RED (break: flat_ident_slot returns 0 for any non-null screen):
- RUNTEST flat_core rc=2 [FAIL] flat_core: 2658 checks, 10 failed
- iclabel (build/vttest_host iclabel directly): [FAIL] iclabel: 256 checks, 9 failed
GREEN (break removed):
- RUNTEST flat_core rc=0 [OK] flat_core: 2658 checks, 0 failed
- RUNTEST iclabel rc=0 [OK] iclabel: 256 checks, 0 failed
 m68k build (amarchy-flat, Amarchy, amarchy-gui) rc=0.

## Read-only question: does the flat draw hook filter by screen?

No. flat_rf_c (amarchy/flat.c:1095) decides by `r->st.enabled && !r->st.held` (1100), `rp->Layer->Window` (1103) and the caller return address in the Intuition ROM range `ret >= r->irom_lo && ret < r->irom_hi` (1165). w->WScreen is used only for side_px (309, 555, 1168) and raa_load (122, 128). The icon label hook is gated by icl_accept (flat.c:789) on task name and DrawMode. So the hook draws on any screen with the global pens; part b must select pens by w->WScreen.

## flat.c call sites part b must change

- 650 `static icl_pencache icl_pc`; 666 icl_cache_release; 695 icl_cache_clear; 730/734/740 icl_cache_find/has_room/put (pen callback); 800 icl_cache_bind(&icl_pc, w->WScreen, ipd_epoch()); 802 icl_cache_verify
- 1708 `static flat_pens g_pens`; 1753/1755/1756/1758 refresh/verify/follow/release (Workbench slot passes &r->st, others pass 0); 1889 flat_pens_init(&g_pens)
- tests/test_flat_core.c:1759-1760 greps flat.c for `flat_pens_refresh(&g_pens` and `flat_pens_follow(&g_pens`; update with part b.
