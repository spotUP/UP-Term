# TE-10.6c brief 4c-1: flat.c hook gates read the per-screen state pointer

## Changed (amarchy/flat.c, by function)
- c2_ring_refill: `ps = st_scr(r, rw ? rw->WScreen : NULL)`; gate `ps->pen_outline`; edge pen from `ps`.
- c2_draw: `ps = st_rp(r, d->imp_RPort)` once (after the rastport NULL check); `ps->enabled`; `r->st.calls++`, `r->st.held`, `flat_clone_route(&r->st, ...)` stay on the live record. The same `ps` is passed to c2_get_pens (new parameter, no second lookup).
- c2_get_pens: takes `ps`; `p->text = ps->pen_ptext`; its own `st_rp` local removed.
- c8_skip: `ps = st_rp(r, d->imp_RPort)`; `flat_frame_decide(ps, ...)`; the 48-byte `flat_state st = r->st` copy and the `st.side = side_px(...)` line are gone; `raa_frame_draw(ps, ...)` and `raa_knob_frame_draw(ps, ...)` use the pointer.
- flat_tx_c: `ps = st_rp(r, rp)` once; both `enabled` gates, `flat_title_decide`, `flat_tx_pen` use it.
- flat_rf_c: `ps = st_rp(r, rp)` once; the three `enabled` gates, `pen_outline`, `pen_back` (slider corner cuts) and the four pen functions use it.
- draw_screen_colours: `n` clamped to `cm->Count` (EHB, HAM).
- draw_sync: after the second list unlock, `r->st.side = side_px(r, g_wb_scr)` (main task).
- Comment "two GTE_COUNT arrays" reworded (G5 forbids the name in flat.c).

## Workbench side
Before: the live record `r->st.side` was never written in flat.c (flat_core.h says "set by amarchy-flat on its draw copy"); c8_skip computed it in the hook copy. With `ps` = `&r->st` for the Workbench the live record would have kept side 0 (1 px). Fix: draw_sync (main task, runs at every pens_run / watch / CTRL_D) writes it. UNVERIFIED: that `flat_draw_sync` does not overwrite `r->st.side` afterwards (it takes `&r->st` as the look source; tests flat_core pass).

## Guard output
Before (4 failed of 11): g1 (c2_ring_refill reads `r->st.pen_`), g2 (`flat_state st =` in c8_skip), g5 (`GTE_` comment in flat.c), g7 (no `cm->Count`). g3 passed already. After: OK (11 tests).
G4 not written: `ObtainBestPen` is only in icl_pen_fn, which takes the ColorMap as a parameter; which screen it is cannot be read from the source.

## Budget (FLAT-C3, own frame / chain / headroom)
Before: `_c2_disp_reg` 20 / 308 / 76, `_c2_disp_stk` 4 / 292 / 92. After: 20 / 316 / 68, 4 / 300 / 84. PASS (limits 80 / 384 / >= 64). Headroom is 4 above the floor; the +8 comes from the extra `ps` locals (flat_tx_c or c2_draw chain). The next brief that adds a local on the c2_draw chain must check the budget first.

## Checks
flat_core 2705, flat_role_core 73, iconpen 2554: 0 failed. flat_pens_per_screen, flat_role_enum_names, ipd_no_dos_under_list OK. m68k build of the 7 targets: rc 0, no warnings.

## Remaining `r->st` uses on hook paths (not `enabled`/`pen_` reads)
- flat_rf_c: `r->st.held`; `flat_gh_match(&r->st, ...)` (disabled-button dither match, look fields); `ghost_menu(...)` call. The screen-bar layer case (Layer without Window) now gets `ps` = st_rp = `st_scr(NULL)` = the live `&r->st`, so its behaviour is unchanged.
- ghost_menu: `flat_gm_pen(&r->st, ...)`.
- d4_flat: `flat_dopus_decide(&r->st, ...)`.
- c2_draw: `r->st.calls`, `r->st.held`, `flat_clone_route(&r->st, ...)`.
- c8_skip: none besides `flat_gh_frame(&r->gh ...)`.

## UNVERIFIED
No rig run: that a themed foreign screen draws flat through these gates, that an unlisted screen draws stock, Workbench side value at runtime.
