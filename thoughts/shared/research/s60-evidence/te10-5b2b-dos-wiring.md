# TE-10.5b2b: DOS wiring of the pen plan for themed non-Workbench screens

Invariant: a public screen that the screen rules theme gets the pens of its theme's pen plan.
Level of the fix: the DOS wiring in `pens_sync` (amarchy/iconpen_dos.c); the pure sync is unchanged.

## What changed
- `pens_sync`: `ip_ops` zeroed, `o.plan = pens_plan`; per-slot Take Over Palette kept in `pen_ctx.take[]` (slot 0 none);
  slots >= 1 load with `gt_load_model` (new `model` parameter of `pens_theme_named`, no second loader);
  models are cleared (not freed) at entry, `gt_free`d before a second read and at the end of the call, so no model pointer stays in the shared record.
- `pens_plan` (plan op, runs under the list lock): `ip_plan_free_mask` then `gt_screen_apply_plan(c->t.model, ...)`; no DOS call, allocation or wait.
  Counts `screen_applies` for slots >= 1 when it returns > 0 entries.
- core: `ip_plan_free_mask` (is_free pens plus the holder's own plan pens, clears the mask, nothing >= 1<<depth), `ip_screens_recheck_plans` (recheck=1 on slots >= 1).
- `rules_dirty` reread calls `ip_screens_recheck_plans`; `ipd_recolour` sets rules_dirty, so a main-theme change re-plans at the next sync. `ipd_recolour` zeroes `ip_ops` and `pen_ctx`, plan stays 0.
- `ipd_rec.screen_applies` at the end; status line `... screens=<n> applies=<n>`; `IPD_SEM_NAME` is "Amarchy.IconPens.5" (also tools/rig/amarchy_iconpens_rig.py docstring). No parser of the status line exists in tools/rig, tests/*.py, tests/amiga (grep `screens=`). No other use of the name under docs/ or tools/.
- recheck on a slot that holds nothing: `ip_sync_decide` gives OBTAIN for hold=1 (stale), so it would obtain anyway; hold=0 and nothing held gives NONE, flag stays harmlessly; a slot whose screen is not open is skipped by `ip_screens_sync` and keeps the flag until it opens.

## Searched / reused
grep of `gt_load_model`, `gt_screen_apply_plan`, `ip_holder_plan_mask`, `pens_theme_named`, `ip_sync_plan`, `recheck`, `IconPens.`, `screens=`. Reused pens_theme_named, pen_is_free, ip_holder_plan_mask, pm_* fakes of the b2a tests.

## Results
- RED (core stubbed, DOS files at HEAD): `RUNTEST iconpen rc=2 [FAIL] iconpen: 2513 checks, 17 failed`
- GREEN: `RUNTEST iconpen rc=0 [OK] iconpen: 2524 checks, 0 failed`
- m68k build of the 7 targets rc=0 (warnings shown are pre-existing: atiming.h, win_core.c; none in iconpen_*). test_make_link_lists.py OK, test_wb_lock_guard.py OK.

## Deviations
None. Free mask in `pens_plan` uses a local zeroed `ip_ops` (is_free only) because the plan op gets only ctx.

## Open / rig check later (not run)
Public screen NAME with rule `NAME = othertheme` in ENV:Amarchy/screen-themes, screen-themes switch on. After the next sync: status line shows `applies` risen and `screens=2`;
a pixel of a theme element on that screen has the theme colour; pens 32-47 of that screen are not allocated by Amarchy.
With an Exclude rule the pixel returns to stock and `applies` stays. Also try Take Over Palette on/off, and a main-theme switch (applies rises again).
