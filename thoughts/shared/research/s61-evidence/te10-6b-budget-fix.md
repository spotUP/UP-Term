# TE-10.6b budget fix (branch te10-6b, worktree vtcon-te10-5b2b)

Commits: 577bc3a9 (fix + tests), then a one-line follow-up for tests/test_flat_icl_pens_persist.py.

## Changed
- amarchy/flat_core.h/.c: flat_draw_slot/tab/scr, flat_draw_init/sync/state/epoch. Slot lookup reuses flat_ident_slot; pens via flat_state_for.
- amarchy/flat.c: static g_draw; st_scr/st_rp return flat_draw_state pointer (no buffers, st_pens removed); draw_sync(r) (collect under
  LockPubScreenList, write under Forbid/Permit) at end of pens_run (every mode, before pens_reap), startup and CTRL_D held branches;
  label hook uses flat_draw_epoch. The one `flat_state st = r->st` copy with side stays (as on main), pens taken from the pointer state.
- tests/test_flat_core.c: 4 test functions covering the 9 cases; tests/test_flat_pens_per_screen.py, test_flat_icl_pens_persist.py adapted.
- Workbench pen write: EXISTED (pens_run uses flat_pens_refresh_on/follow_on with store = &r->st); not removed by 0ec78880. Guarded now by the python test.

## Red then green
Red: tests/test_flat_core.c compile error "unknown type name 'flat_draw_tab'", "implicit declaration of flat_draw_init" (rc 2).
Green: `[OK] flat_core: 2705 checks, 0 failed`.

## flat-budget (limits unchanged)
flat budget: _c2_disp_reg own frame 20, deepest chain 308, headroom 76
flat budget: _c2_disp_stk own frame 4, deepest chain 292, headroom 92
largest reachable frame 80 (limit 80). PASS. (Same as origin/main baseline.)

## Suites
flat_core, iclabel (256), iconpen (2529), gthemes (6934) green; m68k build rc 0.
test_flat_prefs_notify.py fails (dk_flat_sync_with regex vs desk_dos.c, not touched by this fix; came in with origin/main merge).
