# merge-all integration (2026-11-09)

Branch merge-all (head 3603bd7e) from main 1eb68dd6. NOT fast-forwarded into main: step 5 stopped. Main checkout has uncommitted
changes to thoughts/shared/research/test-register.md, which the merge also changes (s57-title, s57-pen5, s56-deskrig append rows).
Other dirty files (amarchy/amarchy.c, dist/Uninstall, tests/test_amarchy_docs.py, 2026-11-08 research note, tools/rig/install_rig.py) do not overlap.
Main remains 1eb68dd6. Worktrees and branches of the merged branches are kept (not yet in main).

## Per branch
- s57-title: merged. Conflict in test-register.md (both sides append rows): kept both.
- s57-pen5: merged. Same register conflict, kept both. Touches amarchy/desk_core.c, tests/test_desk.c.
- s56-deskrig: merged. Conflicts: register (kept both); tools/rig/amarchy_gui_rig.py flat_prefs_on: kept main's flat_prefs_text_on version (newer, same intent), dropped the branch's inline variant.
- te9-1: merged clean.
- te10-6c-3: ABORTED. Conflicts in Makefile (TEST_PY_ONLY list, upprefs build lines: main has AMARCHY_NOBREAK, new tests dopus_toolbar/flat_single_instance/flat_clone_stack) and 6 hunks in amarchy/flat.c (main refactored c8_skip frame fill via flat_frame_fill/push_gadget_of, d4_flat_pens, flat_tx_c; branch uses the old inline gadget loop and flat_draw_state_dri). Design call: re-port the FLAT_RP_* rename and hook gates onto main's flat.c. git cherry shows 8 unique commits of 11 still not in main.
- s48-amiberry-default: cherry-picked c14dd384 only (Amiberry notes, 1 file). c4eea240 and 55157152 not taken. Branch and worktree kept.

## Fix made on merge-all
tests/test_amarchy_desk_rig_pick_waits.py failed after s56-deskrig (amarchy_desk_rig now imports amarchy_rig and calls menu_state in pick). Stubbed amarchy_rig and t.menu_state in the test; passes.

## Build and tests (worktree, host, no rigs)
- make build/amiga/Amarchy amarchy-flat: rc 0, no errors (log scratchpad/mergeall-build.log).
- ONLY=desk: 2354 checks, 0 failed. ONLY=flat_core: 2838 checks, 0 failed. ONLY=palette_closes OK. ONLY=rig_themes_live OK.
- ONLY=desk_rig_pick_waits: failed before the fix above, OK after.
- python3 -I tests/test_amarchy_edit_applies_sentinel.py: OK.
- Not run: flat_role_core, theme editor rig tests beyond the above (te10-6c-3 aborted).

## To finish
Owner: commit or move aside the test-register.md edits in the main checkout, then git merge --ff-only merge-all. Then delete branches s57-title, s57-pen5, s56-deskrig, te9-1 and their worktrees (check git cherry first). Final main hash: 1eb68dd6 (unchanged).
