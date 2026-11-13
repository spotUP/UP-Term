# TE-10.6c brief 3 re-port onto main (branch te10-6c-4, worktree ../vtcon-te6c-4)

Source: te10-6c-3 (untouched). Method: git cherry-pick -X rename-threshold=20% (the amarchy -> patina rename defeats default rename detection).

| old | new | notes |
|---|---|---|
| f9c87cbb | d668dd8e | Makefile: took main, re-added patina/flat_role_core.c (and .h deps) to every link line that has patina/flat_core.c (upprefs, patina-flat; others already had it). flat_core.c: kept both includes (flat_glyph.h + flat_role_core.h). |
| c7e6182a | 1861927e | Makefile: TEST_PY_ONLY and test recipe list on main were restructured; kept main's, appended flat_role_enum_names. |
| 3abf31a2 | 5b882a46 | Makefile same pattern (flat_pens_per_screen). flat.c draw_sync: kept main's STG(FLAT_STG_DRAW_SYNC) + split lock/epoch init, added `i`, then the 4-pass body unchanged. |
| 81a5db97 | NOT PORTED | design conflict, see below |
| 722dfd77, 99db9a6d, 0bb305d3, 2d6d3ae4 | NOT PORTED | depend on 81a5db97 (hooks) or follow it; not attempted |

## Stop point: 81a5db97 (hook gates read per-screen state pointer)
main refactored the flat.c hooks into helpers (c2_ring_get/c2_ring_refill(drawn), c8_draw_on, tx_title/tx_armed, rf_gbox/rf_disabled/rf_scroller/rf_edge/rf_side, ghost_menu(r,rp,b), bb_*/dopus_fill). 5 conflict hunks (c2_ring_refill, c2_draw, c8_skip, flat_tx_c, flat_rf_c) where the old commit edited inline code that is now in helpers. Intent is clear (replace r->st.enabled/pen_*/held gates with `ps = st_scr(r,scr)` / `st_rp(r,rp)`), but every new helper also reads r->st internally, so the port means threading `ps` through ~10 helpers, a design/structure choice I did not guess.

Open design question: thread `const flat_state *ps` as a parameter into the new rf_*/tx_*/c8_*/c2_* helpers (matches old commit's c2_get_pens(r, ps, d, ...)), or have each helper call st_rp/st_scr itself? Recommended: parameter, as the old commit did.

## Build/test (3 commits applied)
Host tests were started (make test ONLY=flat_core, then flat_role_core, flat_pens_per_screen, flat_role_enum_names, make flat-budget) but the first one (builds every host test binary, -Werror ASAN) was still compiling when this agent ran out of budget. NO test or build result exists; make amiga not run. Logs: session scratchpad t1..t5.log.
