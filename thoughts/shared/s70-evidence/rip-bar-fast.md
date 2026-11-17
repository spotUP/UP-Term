# RIP bar fast path (branch rip-bar-fast, worktree /Users/spot/Code/UP-Term/vtcon-rip-bar-fast)

Commit: see `git -C /Users/spot/Code/UP-Term/vtcon-rip-bar-fast log -1` (branch rip-bar-fast).

Changed
- engine/rip_draw.c fill_span: copy mode clips once (viewport + canvas); solid row (pattern byte 0x00/0xFF) -> cv_hspan; patterned row -> 8-entry colour tile expanded into a 128 byte buffer, cv_hrow per piece.
- engine/canvas.c cv_hrow (new, next to cv_hspan; declared in engine/rip.h): clipped memcpy + one dirty_add.
- Sentinel: rip_gput_calls (rip_draw.c, rip.h), vt_rip_gput_calls (rip_vt.c, vtengine.h).
- Fast path: write mode COPY (W0), all fill styles 0..12. XOR (W1) stays per pixel through gput (needs old pixel).
- All fill_span users (B bar, polygon, ellipse fill, flood fill) use it; pixel-exact per tests.

Tests (tests/test_rip.c, suite rip, `make test-one SUITE=rip`; 756 checks, 0 failed)
- bars_row_path_is_pixel_exact: 23 cases, canvas FNV sums recorded from the old per-pixel code.
- bar_does_not_write_per_pixel: B0000HR9P solid / pattern / user pattern, gput delta < 1000 (was 224000; these 3 checks failed on the old code).
- buttons_fixture_canvas_is_unchanged: tests/rip/07-buttons.rip via vt_write, sum 621586454 identical before/after.

Numbers: fixture 07-buttons.rip gput calls 229573 -> 1730 (remaining are lines/text/outlines). Host time 6 ms -> 0 ms (host too fast to be meaningful). A500 timing (was 91-94 s): NOT measured; rig 68 hung, rig 2 not ours. OPEN.

Build: host `make test-one SUITE=rip` ok; `make amiga` exit 0 (68000 and 68020 engine objects rebuilt).
Note: new worktree needed build/amiga/menu_icons.inc copied from main (fontTools missing on host python).
