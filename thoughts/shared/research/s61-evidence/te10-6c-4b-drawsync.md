# TE-10.6c brief 4b: draw_sync fills role pens and side per screen

## Changed
- amarchy/flat.c `draw_sync` (and new statics above it): four passes. 1) list lock: collect screen + epoch. 2) no lock: `ipd_screen_roles` into static `g_roles_in[i]`. 3) list lock again: per screen still listed (`ipd_screen_in`): `side = side_px`; when themed: `draw_screen_colours` (ColorMap via existing `icl_rgb`, DrawInfo pens) then `flat_role_resolve` into static `g_roles_out[i]`; `roles` set only when `ok`. Gone: side 1, roles 0. 4) `Forbid(); flat_draw_sync(...); Permit();` unchanged. Stale comment replaced.
- Formats: `cmap` = `uc_u32[ncolours]` 0xRRGGBB (GetRGB32 top byte of each channel, same packing as `icl_rgb`, reused); ncolours = 1<<depth capped at 256 (DRAW_CMAP_MAX). `dri` = `short[FLAT_RP_DRI_PENS=8]` copied from dri_Pens (zero filled if fewer).
- tests/test_flat_pens_per_screen.py: guard G6 (`test_draw_sync_reads_roles_outside_list_lock`). Makefile: `flat_pens_per_screen` added to TEST_PY_ONLY and test-py (it was not registered).
- No hook function touched.

## Guard before / after
Before: `AssertionError: 'ipd_screen_roles(' not found in 'static void draw_sync(...` ; FAILED (failures=1). After: OK (6 tests).

## Checks
- flat-budget before and after identical: `_c2_disp_reg` 20/308/76, `_c2_disp_stk` 4/292/92.
- Suites: flat_core 2705 checks, flat_role_core 73, iconpen 2554, all 0 failed. flat_role_enum_names, ipd_no_dos_under_list OK. tests/test_flat_budget.py does not run standalone (needs build output; the make target covers it).
- m68k build of the 7 targets: exit 0, no warnings (warnings-as-errors on).

## UNVERIFIED
Runtime behaviour (no rig run): that a themed foreign screen's slot gets the right pens, that the ColorMap read and DrawInfo call under the list lock are safe in practice, depth > 8 screens (cmap capped at 256).

## State for the next brief
After 4b the slot state of a themed non-Workbench screen carries its role pens, while the gates in flat.c still read `r->st.enabled` (the next brief moves them).
