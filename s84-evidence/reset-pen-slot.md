# reset pen slot dump (S85-RESET-BARTRIM)

Result (measured later, see sections below): slot 11 BARTRIM only; Palette 40.2 USE writes 2 to ENV:Sys/palette.prefs while the work copy holds 0. Fixed in desk_pens_reset; rig 84 Keep 0.6-1.8 s.

## First attempt: NOT MEASURED (rig 84 unreachable)

Rig 84 never answered wasabid, so no instrumentation was built or deployed and patina/ is untouched.

- Background `rig.py start` (pid 17678) exited after a wasabid socket.timeout in ami.py _watch (log: scratchpad r84restart.log).
- One fresh `rig.py start --max --purpose "vtcon S85n session: reset pen slot dump"` printed "already running" (fs-uae pid 25970 was up).
- fs-uae at 60-70% CPU; boot.log ends at "wasabid started"; serial.log at the BOOTX shell prompt.
- Port 1317 (rig 84's fs-uae forward): TCP connect times out (not refused), checked at about 4, 6 and 8 min of uptime (`nc -vz -G 8 127.0.0.1 1317`; condev_rig.run raises TimeoutError).

Static reading only (unverified): in desk_pens_reset, wanted pen = desk_pen_fold(want[i], ncolors), then for depth>3 and pen 4..7 remapped via desk_palette_pen (pen - 8 + 2^depth). Slots compared except DK_BLOCK. Candidates for a persistent mismatch: any slot whose desk_pens value is 4-7 (accent / surface / text roles) if IPrefs does not apply the last-four-pens remap to that slot; or at flat, slots DK_SHINE/DK_SHADOW (surface), DK_BARTRIM/DK_EXTRA (= barblock) which OS 3.2 may store differently. Needs the live dump to say which.

---
# MEASURED (rig 84, OS 3.2, S85n session; log scratchpad rp_run2.log; instrumentation in dk_wait_reset, reverted)

depth 4, flat 1, dri_NumPens ncur = 12 (DK_SLOTS is 13: slot 12 DK_EXTRA is never read, desk_pens_reset skips it via i < ncur).
Values = live / wanted (wanted as desk_pens_reset computes it; DK_BLOCK slot 4 skipped by the test).

| slot | name | press 1 (starship) final | press 2 (nord) first-iter | press 2 final (ITER=24, capped) |
|------|------|------|------|------|
| 0 | DETAIL | 0/0 | 0 | 0/0 |
| 1 | BLOCK (skipped) | 1/13 .. see note | 1 | 1/13 |
| 2 | | 2/2 | 2 | 2/2 |
| 3,4,6,7 | | 0/0 | 0 | 0/0 |
| 5 | FILL | 3/3 | 3 | 3/3 |
| 8,9 | | 2/2 | 2 | 2/2 |
| 10 | BARBLOCK | 0/0 | 0 | 0/0 |
| 11 | BARTRIM | 0/0 (ITER=7, done) | 2 | **2/0 (never converges)** |

Note slot 1 is DK_BLOCK (live 1, wanted 13): skipped by desk_pens_reset (BLOCKPEN-IGNORED), so it is not the blocker.
Press 3 (starship again): counters unchanged (ITER=24 before and after), so the wait either capped again or did not run; not distinguishable.

Mismatching slot: 11 = DK_BARTRIM (BARTRIMPEN). Live 2 (the stock / Palette-editor value), wanted 0 (flat: desk_pens_flat sets
BARTRIM = BARBLOCK = surface = pen 0). It never changes once it sits at 2. Slot 12 (DK_EXTRA, BARCONTOUR) is not even readable on 3.2
here (ncur 12).

Why (reading, not yet measured further): desk_core.c:453-476 (pal_list_31) rewrites the palette.prefs word 11 to equal word 10 because
Palette 40.2 writes 2 there. In press 1 live slot 11 reached 0; at the start of press 2's wait it was back at 2, so something between
the presses (the Palette editor USE writing 2, then Patina's ENV fix-up rewriting the file without IPrefs re-reading it, or the
S82-SKIP-UNCHANGED equal-bytes skip) leaves IPrefs holding the editor's word 11 = 2. Next step: log word 11 of ENV:Sys/palette.prefs
right after the editor and after the fix-up for the nord press.
