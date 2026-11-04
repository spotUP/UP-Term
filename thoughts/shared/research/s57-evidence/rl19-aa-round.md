# RL19 SELECTA-AA-ROUND: anti-aliased round corners (frames, buttons, scroller knob)

Status: code and host tests done; rig pixel proof NOT obtained (rig screen was 4 colours).

## Code
- amarchy/roundaa_core.c/.h: raa_radius, raa_round_inset, raa_cover (4x4 samples, 1/8 pixel), raa_nearest (pm_aim/pm_dist), raa_frame_draw (stock corners first via flat_frame_draw_round, then blend corners with ce_mix), raa_knob_frame_draw (knob box pen from flat_knob_state_pen, same path).
- amarchy/flat.c: frameiclass path and smart-knob path call them; palette read by raa_load (GetRGB32, 0 when depth > 8 or unreadable -> stock corners).
- Makefile: amarchy-flat links render/emoji.c and render/unifont.c; flat-budget also checks roundaa_core.s.
- render/pen_match.h: PM_MAYBE_UNUSED on the static helpers (host -Werror).

## Host tests (tests/test_flat_core.c, test_round_aa), make test-one SUITE=flat_core under nice 19
- flat_core: OK, 2407 checks, 0 failed.
- Covers raa_cover, raa_radius (11 -> 2, 12 -> 4), raa_nearest (5 known picks), blend (grey pens in corner, blended > 0), fallbacks (npens 0, 300, unfilled, secondary role, unreadable pen) = stock corners and blended == 0, knob via raa_knob_frame_draw.
- Revert proof: blend replaced by (void)pen -> 3 checks failed (lines 561, 564, 565); restored -> 0 failed.

## Builds
- make flat-budget PASS (_c2_disp_reg chain 308 headroom 76; _c2_disp_stk chain 292 headroom 92).
- make amiga rc=0; amarchy-flat keeps long __stack = 16384 and $STACK: 16384.

## Rig pixel check (rig 121, 3.2, --max, tools/rig/amarchy_roundaa_rig.py BINARY TAG)
Recipe: rig.py start --max --os 32; rig.py aga256 (log: "Workbench stayed PAL (W43)", depth stayed 2); amarchy_gui_rig.py --setup; the script installs the binary, picks a theme, draws btnbgprobe with and without the 80x14 button, compares 12 corner pixels.
Result: new and stock both 4 of 12 corner pixels at b6b6b6 (identical), right-side corners hard. Workbench depth 2 = 4 pens, no intermediate pens, so raa falls back to stock corners. This run does NOT prove the blend on screen.
Open: get a 256-colour Workbench on the rig (aga256 did not change the screen mode), rerun with new and stock, expect corner pixels between fill and backdrop for new only. Locale scroller knob not measured (script has a --knob stub that only saves a grab).
Screenshots: ~/Desktop/upterm-shots/rl19-{new,stock}-button.ppm
