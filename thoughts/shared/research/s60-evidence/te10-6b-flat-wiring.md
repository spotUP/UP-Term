# TE-10.6b: flat.c uses per-screen pen numbers

Branch te10-6b. Build `build/amiga/amarchy-flat` rc 0 (-Werror). Host suites flat_core 2673/0, iclabel 256/0, iconpen 2529/0.
Python: tests/test_flat_icl_pens_persist.py (updated), tests/test_flat_pens_per_screen.py (new guard), tests/test_wb_lock_guard.py: 12 passed.
Rig run NOT done (unverified on the Amiga): needs an eye check with a second public screen.

## What changed
- flat_core.c: `flat_state_for(look, tab, screen, epoch, out)` copies the global look block, zeroes the four pens, overlays the slot's dim/knob/inbar/sel.
- iconpen_core.c: `ip_screens_epoch_of_screen`; iconpen_dos.c: `ipd_epoch_of_screen`, `ipd_screen_in` (list walk under the caller's list lock; no new LockPubScreen).
- flat.c: `g_pens` is a `flat_pens_tab`, `icl_pc` an `icl_pentab`. Hooks call `st_scr` / `st_rp` / `st_pens` (flat.c:301-335), identity = screen pointer + slot epoch (`scr_epoch`).
  Workbench slot is also stored into `r->st`, so Workbench output is unchanged; the look block stays global.
- `pens_run` (flat.c ~1819) handles the Workbench slot only; `pens_reap` (1 s step) reaps both tables under `LockPubScreenList` via `flat_pens_tab_reap` / `icl_tab_reap`.
- `icl_pens_release` (flat.c:713): per slot, ReleasePen on the map of the screen that obtained the pens, under the list lock; slots whose screen is gone or reopened are forgotten without ReleasePen.

## Behaviour for screens without pens
- Screen with no slot / no pens: the four pen fields are 0 and the existing fallbacks run (flat_dim_pen, flat_knob_pen, flat_inactive_pen, flat_edge_pen, pen_sel).
  Before this change such a window got the Workbench pen numbers (wrong ColorMap); now it never gets pens of another screen.
- Rastports without a window (menus, screen bar) and `d4_flat` (DrawInfo only) keep the Workbench pens from `r->st`, as before.
- icl label call on a full table: skipped, counted in lskip.

## Open point: does the frame hook draw on non-Workbench screens today?
Yes, no screen filter. `c8_skip` (flat.c:565-575) tests only MethodID, imp_RPort, imp_DrInfo, then `flat_frame_decide(&r->st, ...)` on the DrawInfo SHINE/BACKGROUND pens;
`c2_draw` (flat.c:478-497) tests only MethodID, enabled, held, RPort. Neither reads `WScreen`. The only per-screen input is the DrawInfo of the drawing screen.
