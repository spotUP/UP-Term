# TE-10.6c brief 3: draw slot carries themed flag, role pens, side

Commit f9c87cbb on te10-6c-3 (worktree /Users/spot/Code/UP-Term/vtcon-te6c-3). Not pushed.

Files: amarchy/flat_core.h, amarchy/flat_core.c, amarchy/flat.c (one comment only), Makefile, tests/test_flat_role_core.c (new test), tests/test_flat_core.c (memset only).

## API as landed
- flat_draw_scr { screen; epoch; const struct flat_role_pens *roles; uint8_t side; } (forward declaration in flat_core.h).
- flat_draw_sync, every sync: plain.enabled = 0. Per listed non-Workbench screen: st = flat_state_for(...); roles usable (roles != 0 and roles->ok) -> flat_role_apply(&st, roles), else st.enabled = 0; then st.side = list[i].side. One code path. Slot address rule untouched; Workbench path untouched.
- flat_core.c now includes flat_role_core.h (so every binary linking flat_core.c needs flat_role_core.c).

## Meaning of ok (flat_role_core.h)
ok = 1: the ColorMap was given, every row chosen from colours. ok = 0: no ColorMap, rows without a held plan-pen match are 0. The header does not say "unusable"; I JUDGED ok = 0 as not usable (pen 0 for several roles would draw wrong colours, stock is safer). One test pins it. If you want ok = 0 drawn flat with the held pens, change the condition in flat_draw_sync and the test line "ok = 0 behaves as not themed".

## Test location deviation
D1a-D3 live in tests/test_flat_role_core.c (test_draw_slot_themed), NOT test_flat_core.c: flat_glyph.h (included by test_flat_core.c) and flat_role_core.h both define enumerators FLAT_ROLE_BACK/TEXT/LINE, so one TU cannot include both. (Production flat.c includes flat_glyph.h and must never include flat_role_core.h; brief 4 should keep role access via flat_core.h only.)
Failing first: `tests/test_flat_role_core.c:216:45: error: no member named 'roles' in 'struct flat_draw_scr'` (and 'side').
Changed existing tests: none asserted enabled/side. The four draw tests in test_flat_core.c (test_draw_state_lookup, test_draw_sync_look_and_drop, test_draw_slot_keeps_address, test_draw_epoch) got `memset(l, 0, sizeof l)` because the lists had uninitialised new fields (SEGV in flat_draw_sync otherwise).

## Checks
- make test-one SUITE=flat_role_core: [OK] flat_role_core: 73 checks, 0 failed
- make test-one SUITE=flat_core: [OK] flat_core: 2705 checks, 0 failed
- python3 -I tests/test_flat_pens_per_screen.py: OK
- m68k build of Amarchy amarchy-gui amarchy-menu amarchy-index amarchy-icons amarchy-flat upprefs: rc=0, no undefined symbols (flat_role_core.c needs nothing beyond gthemes_core, already linked everywhere flat_core.c is).
- make flat-budget rc=0:
  flat budget: _c2_disp_reg own frame 20, deepest chain 308, headroom 76
  flat budget: _c2_disp_stk own frame 4, deepest chain 292, headroom 92
- Makefile: flat_role_core.c added after flat_core.c at lines 958/960 (upprefs), 966 (AMARCHY_SRC), 1032 (MENU), 1046 (GUI), 1066/1071 (amarchy-flat); .h added to the two prerequisite lists (958, 1066). Brief 2 will add the same file to iconpen_dos.c rules: expect a duplicate-name merge conflict or a doubled source in the same variable lines (AMARCHY_MENU_SRC, AMARCHY_GUI_SRC, amarchy-flat line 1071); duplicates must be removed (double definition at link).

## Point 5: who reads enabled in flat.c
Every read is `r->st.enabled` (the live record): c2_draw (line 477), flat_tx_c (917 title accent, 970 icon label), flat_rf_c (1162, 1165, 1227). No hook reads `enabled` through the per-screen pointer (st_scr / st_rp results) today; grep -n enabled amarchy/flat.c shows nothing else.
Effect until brief 4: non-Workbench screens still draw flat exactly as before whenever the live record is enabled, because no hook consults the slot's new enabled = 0. The slot data is correct but unused; behaviour on screen is unchanged by this commit.

## Unverified
- No rig/emulator run. flat_role_apply not in a patched dispatcher: confirmed only by flat-budget numbers unchanged (it runs from flat_draw_sync via draw_sync on the main task).
