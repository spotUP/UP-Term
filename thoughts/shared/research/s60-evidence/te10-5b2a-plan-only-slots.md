# TE-10.5b2a: non-Workbench pen-holder slots hold only the pen plan

## Invariant and level of the fix
A pen-holder slot >= 1 (non-Workbench screen) holds ONLY the pen plan's exclusive pens. It never holds the 16 icon pens (32-47) and never obtains role pens 4-7 on its own. Slot 0 (Workbench) keeps every current behaviour.
Level: the pure core (amarchy/iconpen_core.c), in `ip_screens_sync`, which now calls the new `ip_sync_plan` for slots >= 1 and `ip_sync` for slot 0 only. Not patched in the DOS layer.

## What was reused
`ip_holder_verify`, `ip_sync_decide`, `ip_sync_new_identity`, `ip_plan_sync` (already owns plan-pen obtain/release/recolour), `ip_holder_release`, the slot epoch handling in `ip_screens_apply`. New: `ip_sync_plan`, `ip_holder_plan_mask`, `ip_ops.plan`, `ip_screen_slot.owed`.

## Behaviour
- Unchanged tick: 0 obtain/release/set_rgb/plan/theme calls.
- `recheck = (pcount < number of exclusive plan entries)`: a refused pen is retried on the next tick; a full plan stops re-planning. A theme or rule change must set `recheck = 1` (b2b).
- A plan pen found free on the ColorMap (same-address screen reopen) is treated as lost: full forget, one epoch bump. If the theme is not ready (NEED_THEME), the bump is owed (`owed`) and paid once later.
- `ip_screens_held` counts `held > 0 || pcount > 0`.

## RED / GREEN (host, iconpen suite, ASan+UBSan)
RED (stubs, unchanged behaviour): `RUNTEST iconpen rc=2 [FAIL] iconpen: 2485 checks, 43 failed`
GREEN: `RUNTEST iconpen rc=0 [OK] iconpen: 2485 checks, 0 failed`
(One extra GREEN run was needed: the first failed 1 check because my test double `pm_theme` set flat=0 so slot 0 got no role pens. Test-double bug, not a core bug.)

## m68k
`make build/amiga/{Amarchy,amarchy-gui,amarchy-menu,amarchy-index,amarchy-icons,amarchy-flat,upprefs}` rc=0 (only pre-existing warnings in untouched files). `python3 tests/test_make_link_lists.py`: OK.

## Open points for the DOS half (b2b)
- Wire `ops.plan` in the DOS ops table (plan from the theme for that screen's depth).
- Use `ip_holder_plan_mask` to build the held-pen mask for the slot (so the theme code does not remap pens it does not own).
- Set `recheck = 1` on theme or rule change.
- Slot 0 callers are untouched.

## Later rig check
On a themed non-Workbench public screen, pens 32-47 stay unallocated by Amarchy (and pens 4-7 are not taken by Amarchy), while the plan's exclusive pens are held. Measure from the screen's ColorMap pen allocation, not from a screenshot.
