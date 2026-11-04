# UPDemo scene 15 (Kitty animation) skips itself on the rig: cause

Symptom (rig-recheck-2.md, rig 102): `UPDemo 14`, one Space, lands on "Demo 16/35 Kitty truecolour" with scene 15's
heading left on row 2; B/Left from 16 do nothing; no auto-advance for 30+ s.

## Cause 1 (shown on the host, fixed): scene time is counted from before the first frame is drawn

- demo/updemo.c play(): `t0 = io->ticks()` is taken before `sc->fn(t, fresh)` runs the first frame; the loop
  `while (!moved && (t = ticks - t0) < dur)` then ends the scene as soon as the first frame's draw took >= dur.
- The console's Write is synchronous: handler/vtcon_handler.c:8287 ACTION_WRITE runs `output()` (kitty parse,
  base64, PNG decode) and only then `reply()`s. So the program's clock runs while the terminal digests the data.
- sc_kitty_anim (demo/updemo.c:1955, table 2585, 450 ticks = 9 s) sends fjord.png (33 kB) + earth.anim
  (984 kB: 44 PNG frames) in its first frame, ~1.35 MB of base64 escape data. On a 68060 that takes the handler
  longer than 9 s, so after the frame `t >= 450` and scene 16 starts at once: scene 15 "starts to draw and is
  left at once". Not a stray key: all kitty commands are q=2 (no OK/ERR reply), and read_event never turns a
  reply into ACT_NEXT.
- Missing media (coordinator's lead) does NOT cause this: with io.load == 0 the scene draws the coded globe
  (8 small raw frames) and stays its 9 s. Media is in the kit: Makefile:1879 copies dist/updemo into
  Files/share/updemo, dist/install.dos:611-615 copies all four files to UP-Term:share/updemo, and
  build/rig96/os-32/UP-Term/share/updemo holds earth.anim + fjord.png. The "updemo files missing in rig tree"
  text is not in tools/ or dist/ of this repo (grep): source of that warning unverified.

Host test (fails before, passes after): tests/test_updemo.c
`a_scene_whose_first_frame_draws_slowly_still_gets_its_whole_time`: an io whose write costs 1 tick per 1000 bytes
(50 kB/s) runs the kitty animation scene alone. Before the fix: 2 failures (scene left after 1 frame, total time
= draw time only). After: the scene stays 450 ticks after its first frame.

Fix (branch `fix/updemo-scene-time-after-draw`, worktree .claude/worktrees/updemo-scene15): after the first
frame's flush, `late = ticks - t0 - t; if (late > 2) t0 += late;` so the scene's time starts once it is on screen;
`stat_ticks` uses the unshifted start, so BENCH numbers are unchanged.

## Cause 2 (NOT confirmed, needs a rig): scene 16 frozen, B/Left dead, no auto-advance

Cause 1 alone does not explain 30+ s on scene 16 with keys ignored: scene 16's loop polls keys every 40 ms and
ends after 9 s. Likeliest: the handler is still busy (or kept busy) with scene 15's data and its animation:
- the demo never deletes kitty images i=1 (earth, 44 frames, looping s=3) or i=6 (fjord) when it leaves
  scene 15 (scene 16 deletes only i=2,3), and no ED-clears-placements handling was found in engine/kitty.c;
  the handler's frame clock (vt_anim_tick, image-animation default on) keeps switching 44 big frames;
- while the handler is busy, the demo blocks in Write/WaitForChar packets, so its keys and its clock-driven
  advance both stall, and the screen shows scene 16's status over half-redrawn scene 15 text.
Rig check to confirm: on a fresh window run `UPDemo 15` with the fix, time how long scene 15's first frame
takes (status line appears), then Space and check B works on 16; repeat with `image-animation off` in the
profile. If 16 recovers with animation off, the fix is: delete i=1 and i=6 at scene 15's end (a scene leave hook
like tabs_leave/splits_leave), plus look at the per-frame cost of vt_anim_tick on the 060.

## Result (final)

- Commit 5a0b15bc on branch `fix/updemo-scene-time-after-draw` (worktree `.claude/worktrees/updemo-scene15`), not merged.
- Test `a_scene_whose_first_frame_draws_slowly_still_gets_its_whole_time` (tests/test_updemo.c): with fix `[OK] updemo: 8689 checks, 0 failed`; fix reverted: FAIL at 1071 (total time) and 1072 (`frames > 1`).
- Cause 2 (scene 16 frozen, B/Left dead, kitty images i=1/i=6 never deleted) stays unconfirmed; rig steps above.
