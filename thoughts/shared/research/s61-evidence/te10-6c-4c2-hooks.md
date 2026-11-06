# TE-10.6c brief 4c-2: screen-bar layer, ghost_menu and d4_flat read the per-screen state

## Changed
- flat_core.h/.c: `dri` in flat_draw_scr and flat_draw_slot, `wb_dri` in flat_draw_tab (compare keys only). `flat_draw_sync_dri(..., wb_dri)` copies them; `flat_draw_sync` is a wrapper with wb_dri 0 (existing callers and tests unchanged). New `flat_draw_state_dri(d, live, dri)`: NULL or wb_dri = live, a slot's dri = that slot, else &d->plain.
- flat.c st_rp: layer without Window -> key = `(char *)Layer->LayerInfo - offsetof(struct Screen, LayerInfo)` (never dereferenced, compared in the draw table only); rp or Layer NULL keeps screen NULL = live.
- ghost_menu: `ps = rp->Layer ? st_rp : st_scr(r, IntuitionBase->ActiveScreen)` (pointer read, no lock); returns 0 (stock) when `!ps->enabled`; `flat_gm_pen(ps, ...)`.
- d4_flat: `flat_dopus_decide(flat_draw_state_dri(&g_draw, &r->st, info), ...)`.
- draw_sync: `draw_dri_key()` (GetScreenDrawInfo + FreeScreenDrawInfo at once, pointer kept as key) for each listed screen in the existing second public-list-lock pass and for the Workbench screen (ipd_screen_in validated) ; passed via flat_draw_sync_dri. Nothing held, nothing to free on reap. ipd_screen_roles still only in draw_sync, outside the list lock.

## Tests
- Host failing first: `tests/test_flat_core.c:2504:45: error: no member named 'dri' in 'struct flat_draw_scr'` (compile failure). After: flat_core 2714 checks, 0 failed (new test_draw_state_dri: NULL, wb_dri, slot dri, unknown, after sync without screen, dri replaced on re-sync). flat_role_core 73, iconpen 2554: 0 failed.
- Guard failing first: G1 extended to ghost_menu and d4_flat -> `Regex matched: 'flat_gm_pen(&r->st'`. After: flat_pens_per_screen OK (11), flat_role_enum_names OK, ipd_no_dos_under_list OK.
- m68k build of the 7 targets: rc 0 (-warnings-as-errors), no warnings.

## Budget (FLAT-C3, own frame / chain / headroom)
Before: `_c2_disp_reg` 20 / 316 / 68, `_c2_disp_stk` 4 / 300 / 84. After: identical. PASS.

## Remaining `r->st` reads in hook paths
- flat_rf_c `flat_gh_match(&r->st, ...)`: LEFT. It reads `enabled` and `held` (held is live-only, by design); the gate before it already checks `ps->enabled` and `!r->st.held`.
- `r->st.calls`, `r->st.held`, `flat_clone_route(&r->st, ...)` in c2_draw; `r->st.held` in flat_rf_c; the `&r->st` live argument of st_scr / flat_draw_state(_dri): by design.

## UNVERIFIED at runtime (rig must check)
- Disabled screen-bar title on a themed foreign screen draws the dim ghost; on an unlisted screen stock (LayerInfo offset math lands on the listed Screen address).
- Disabled menu item (Layer NULL bitmap) ghosts on Workbench and on a themed non-Workbench active screen; stock on an unthemed one.
- Directory Opus on a themed foreign screen: d4 draw matches the slot; DrawInfo pointer from GetScreenDrawInfo equals the DrawInfo the DOpus patch receives (assumed: same per-screen DrawInfo). Workbench DOpus unchanged (wb_dri).
- A reopened screen with a new DrawInfo gets the new key at the next draw_sync (up to about 1 s lag, plain meanwhile).
