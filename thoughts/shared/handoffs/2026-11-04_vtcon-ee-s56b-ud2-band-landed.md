---
date: 2026-11-04
session: vtcon-ee s56 (second half)
status: handoff, week limit approaching
---

# vtcon-ee s56b: UD2 globe band fix landed, release rows open

## Landed
- main 230f94bf (pushed): UD2 globe band fix.
  - d51aaa79 guard test `s56_planar_globe_switch_reaches_the_window_in_one_copy` (tests/test_render.c).
  - b864decf fix: engine/kitty.c kit_anim_show bumps sync_frames on a frame switch outside ?2026;
    render/amiga_render.c adds back_chunky (planar back-bitmap writes), img_write_idx uses it when r->cpu,
    draw_images refuses CPU compose only for RGB images (fmt != 1), under_text skips when cpu_refuses.
  - Root cause: on AGA planar the globe drew straight to the window (no compose), or a compose was
    abandoned and replayed row by row; the gap between the row fill and 1-row WriteChunkyPixels is the band.
  - Host: render 739/0, kitty 470/0, updemo 8695/0. Full `make test` NOT run.
  - Rig grabs (~1 frame/s) cannot see the band; verdict = owner eye.
  - Evidence: thoughts/shared/research/s56-evidence/ud2-globe-band.md (Rig hunt 3).
- Old branch fix/ud2-globe-band (ab9cc6b9) superseded but NOT deleted: the main checkout is still on it. Switch the checkout to main (uncommitted evidence comes along), then delete it.
- Worktree ud2-band and branch fix/ud2-band-root removed (verified).

## Open, in order
1. Owner eye check UD2: fresh-all 3.2 rig from main 230f94bf (rig 111 is ours, owner's eye-check rig;
   restart with --keep --idle-minutes 0 --purpose "vtcon-ee: UPDemo 15 band check"). Owner runs `updemo 15`,
   PASS = no horizontal band over the planet. Also UPDemo 8 eye check still open.
2. DONE: strut helper fix on main ef015f25, branch deleted. (was: merge fix/strut-off-helper
   if checks green; delete branch.
3. Commit uncommitted evidence in the main checkout, by name (no -A):
   s56-evidence/release-rig-batch.md, ud2-globe-band.md, ud2-band-sentinel.patch, release-ledger-audit.md,
   other s56-evidence files, research/test-register.md.
4. Ledger (thoughts/spot/todos.md, edit by pattern, rows move):
   - STRUT-REFLOW-RESTORE: strut prebar PASS, strut_off PASS after helper fix (release-rig-batch.md).
   - FLAT-BUTTON-FILL: depth 8 idle 8d8d8d (pen 3), held bebebe = window background. Owner chose
     "Lighter tint". Code in amarchy/flat_core.c ~560 belongs to the Amarchy session: ledger only.
   - R1.17-WBRESET / AM-FLAT-WB: NOT verified. `amarchy_gui_rig.py --setup` on rig 115 failed:
     "flat prefs off, amarchy-flat BOOT left nothing resident, STATUS: flat=on calls=16 chained=0". Undiagnosed.
   - FLAT-PENS-NOT-LIVE visual check: not run.
   - 3.1 rows: wait for the batched 3.1 run.
   - New row: UD2 band fix landed 230f94bf, owner eye check open.
5. Rig rules: never touch 1, 66, 68, 70, 91, 96; avoid 71, 74, 84. Check test-register.md before any run.
6. Agent briefs stay small (last agents ran 56-118 calls).
