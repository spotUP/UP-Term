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
Results (3 ported commits + 1 test-fix commit): flat_core 4550 checks 0 failed; flat_role_core 73 checks 0 failed; flat_pens_per_screen OK; flat_role_enum_names OK; make flat-budget PASS.
Two ported tests needed fixes for main: tests/test_flat_role_core.c called desk_flat_state with 3 args (main takes 4; added aa=1); tests/test_flat_role_enum_names.py used amarchy/ paths (now patina/). make amiga not run.

## Update (S80): stop point resolved, branch still not mergeable
81a5db97 and its follow-ups were ported after this report: f35ed690 (hook gates read the per-screen state pointer), 8e0c2747 (screen-bar layer, ghost_menu, Directory Opus patch), 892363a1, 422e6cd2 (pubscreen helper SHAREPENS). Design choice taken: helpers compute `ps` themselves where that keeps the stack frame small; `ps` is a parameter only where the old commit already had one and the budget allows.

Flat-budget regression and fix (712e36f8): after the port, c2_draw saved d4 (frame 48 instead of 44), so the _c2_disp_reg chain reached 324 of 384 (headroom 60 < 64, FAIL). Fix: new `c2_off(r, d)` holds the RPort/enabled/held guard; c2_get_pens and c2_ring_get call `st_rp(r, d->imp_RPort)` themselves instead of taking `ps`; c2_ring_refill is back to `(r, d, im, drawn)`. Result: headroom 64 (chain 320), make flat-budget PASS, make amiga rc=0, flat_core 4559/0, flat_role_core 73/0. Tried and rejected: inlining ps in c2_glyph (frame grew to 56).

Judgement calls to review before merge:
- flat_gr_match, flat_gh_match and pen_back read `ps` instead of `&r->st`.
- d4_flat_pens keys the DrawInfo pen array via offsetof(struct DrawInfo, dri_Pens).
- ghost_menu returns 0 when `!ps->enabled`.

Blocker: rig check 2 (thoughts/shared/research/s61-evidence/te10-6c-rig-check-2.md) shows the plan is counted but no pens are written and flat frames are not drawn on a non-Workbench screen. Owner moved brief 3 behind the release. Do not merge.
