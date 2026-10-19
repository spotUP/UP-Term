# Painter span timing on AGA, 68020 (2026-10-19)

Question: of the painter render per write, how much is vp_span_fast (vp_span_long) and how much the rest of painter_run.

Setup: rig 2 `--a1200` (68020), native AGA 640x256 depth 4 (wasabi screen list: 640 x 256 x 4, 1.9 MB graphics mem; planar painter active:
`long/spans` PROF counter moved), PROF=1 handler, EClock 709,379 Hz, `VTC:phaseprobe ONLY 8` (ncurses-frame, 20 writes of 4,268 bytes) in an
`XCON:0/0/640/210/pp` window (xterm dialect, `NewShell FROM RAM:pp.s`), the same recipe as 2026-10-19_vtcon-feed-perf.md. Wall 613 ticks (earlier 605).
New PROF line `span/painter`: ticks inside the vp_span_fast call of painter_run, ticks in the whole painter_run (vr_prof[0]).

| part (one run, 20 writes) | ticks | ms total | ms per write | % of painter_run | % of render |
|---|---|---|---|---|---|
| vp_span_fast (vp_span_long) | 353,986 | 499 | 24.9 | 87.4 | 4.4 |
| rest of painter_run (lock, planes_ok, WaitBlit, target, loop) | 51,236 | 72 | 3.6 | 12.6 | 0.6 |
| painter_run total | 405,222 | 571 | 28.6 | 100 | 5.1 |
| render total (render/frames, 6 renders) | 7,987,123 | 11,259 | 563 | | 100 |
| of which rows (rows/scroll) | 7,880,680 | 11,109 | 555 | | 98.7 |
| fill() (fill/fills, 458 fills) | 614,342 | 866 | 43 | | 7.7 |

Counts: 415 painter runs (one span each), 318 Text runs. Span cost 1.2 ms a run; rest of painter_run 0.12 ms a run.

Finding: painter_run is about 5% of the render in this workload, not the whole cost. 87% of painter_run is the span blitter, so the span is 4.4% of the render
(25 of 563 ms a write). About 94% of the render (about 530 ms a write) is outside painter_run: the 318 Text runs, the fills, the row loop and the
cursor pass. Porting more of the painter (pre-shifted glyph store, single-plane path, asm row loops) can win at most 25 ms of 563 here, which agrees with
the long-store result (no visible change). The next hunt is the Text-run path and whatever else runs under rows/scroll.

Not re-derived: the handoff's "~390 ms" render per write; this build gives 563 ms (the feed-perf doc's long-painter row gives 554 ms). One run only on the xterm-dialect
workload. ReadEClock itself costs a little per call (two per painter_run before this patch, four now), so the rest-of-painter_run figure is an upper bound.

Discarded first run: window `XCON:0/0/640/180/UP-Term/AMIGA` (Amiga dialect, phase_rig.py's window) ran the same workload in 87 ticks with 501 painter runs and 0
Text runs: spans 413,281 of painter_run 475,811 ticks (87%), render 792 ms for 5 renders. That is not the workload of the earlier measurements; do not compare.

Patch: render/amiga_render.c (vr_span_ticks around vp_span_fast, VTCON_PROF only) and handler/vtcon_handler.c (`PROF span/painter` line). No drawing change.
