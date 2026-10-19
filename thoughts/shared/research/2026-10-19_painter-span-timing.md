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

## Follow-up: the Text runs and the rest of the rows pass (same day, same setup, rig 2 --a1200, 640x256x4 planar per wasabi screens)

PROF-only counters added (render/amiga_render.c flush_run, draw_rows_now, cell_style; `PROF text ...`, `PROF txr ...`, `PROF flushrun/cellstyle` lines). One run, `phaseprobe ONLY 8`, xterm-dialect XCON window, 20 writes.

Why a run goes to Text(): flush_run (render/amiga_render.c, `if (bg_apart(r, st)) {...} else if (painter_run(...) || cpu_refuses(r))`) falls through to Text() when painter_run refuses
(`!r->planar || !r->glyphs || st->attr || st->deco || st->font || RGB ink`, or planes_ok false) or when bg_apart() is true (dithered or unmatched RGB background: fill + Text in JAM1).

Text runs by reason (counted in the same order as the gate): 265 runs, 2,915 cells; dithered/unmatched background (bg_apart) 265; attr 0; deco 0; font 0; RGB ink 0; not planar 0; no glyphs 0; planes not ok/covered 0.
(The earlier note's 318 was a different run; this one gave 265 with 415 painter runs.)

| part (20 writes) | ticks | ms total | ms per write |
|---|---|---|---|
| rows pass (rows/scroll) | 7,829,322 | 11,037 | 552 |
| cell_style (all calls, rows pass and others) | 6,488,780 | 9,147 | 457 |
| flush_run, all of it | 933,744 | 1,316 | 66 |
|   of which fill() | 506,797 | 714 | 36 |
|   of which painter_run | 338,942 | 478 | 24 |
|   of which Text path (265 runs) | 124,582 | 176 | 8.8 |
| direct_row (24 calls) | 2,706 | 4 | 0.2 |
| row loop, asm scan, rest | | | about 29 |

Finding: the Text path is not the cost (8.8 ms of 552 per write). cell_style is: 457 ms per write, 83% of the rows pass (vt_resolve_colors, pen_for twice-plus, area_ink twice, vt_cell_underline_color, vt_cell_font per call).
Caveat: ReadEClock wraps every cell_style call, so the figure includes timer overhead; the number of calls is not counted yet. Next step: count the calls and split cell_style into pen_for / area_ink / vt_resolve_colors
(pen_for on a 16-pen AGA screen with 256-colour ncurses cells is the suspect: nearest-pen or dither lookup per call).

## Follow-up: is cell_style real? (same day, same setup, rig 2 --a1200, AGA 640x256x4, `phaseprobe ONLY 8`, 20 writes; host load very high, so ratios and counts count, not absolute ms)

PROF-only counters: cell_style calls, calls by call-site line (macro records __LINE__), 1000 empty ReadEClock pairs timed once at the first print, cell_style_in split (vt_resolve_colors / pen_for fg+bg / area_ink x2), and pen_for first-use entries (`!r->have[c]`) with failed obtains. Two runs, the second with the pen_for counters.

| figure | value |
|---|---|
| cell_style calls, 20 writes | 854 (43 per write) |
| empty ReadEClock pair | 18 ticks (25 us); 854 pairs = 15,400 ticks = 0.2% of cell_style's 6,558,041 |
| cell_style real | 6.54 M ticks = 9.2 s total = 460 ms per write (the 457 ms was real, not timer overhead) |
| split: vt_resolve_colors | 26,000 ticks (0.4%) |
| split: pen_for fg+bg | 6,444,000 ticks (98%) |
| split: area_ink x2 | 37,000 ticks (0.6%) |
| pen_for first-use path (obtain) | 126 entries, 0 failed; 6.44 M ticks / 126 = about 51,000 ticks (72 ms) each |
| call sites (lines in this commit) | 3336 (C loop, `st`): 559; 3299 (asm row-scan loop, `fs`): 280; 3065: 10; 3888 (cursor): 5 (2172, 2953, 3125, 3327: 0) |

Finding: cell_style is real, but it is not per-call work. 98% of it is pen_for, and that is the 126 first uses of a colour (obtain on the AGA screen: about 72 ms each). r->have[] is cleared when the render attaches to a window (amiga_render.c ~478, ~672), so these 126 obtains are the window's first paint of the ncurses 256-colour frame, paid inside the first writes of the 20, not a steady-state cost per call. The other 728 calls cost about 120 ticks each (cheap, as expected from reading the code). So the 552 ms per write average is dominated by a one-time pen-obtain burst; a steady-state write (second pass over the same colours) should not show it.
Next measurement: print per-write cell_style ticks (or run the 20 writes twice in one window) to confirm writes 2..20 are cheap, and time obtain()/few_obtain apart (ObtainBestPen on a 16-pen screen vs the dither path). If confirmed, the "PROF rows 552 ms per write" figures in the earlier sections are a cold-start average and must be re-taken warm.
Unverified: that the 126 first uses sit in the first writes (counted in total only, not per write).
