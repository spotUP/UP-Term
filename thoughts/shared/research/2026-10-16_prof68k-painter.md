---
date: 2026-10-16
topic: prof68k painter profile on the colour shapes (ASM1, FP3.4 S1, FP3.6 row 7)
status: measured under vamos, instruction counts only (no timing)
---

# prof68k: the painter on the colour shapes

Command: `tools/prof68k.py build-paint`, then `tools/prof68k.py run-paint SHAPE PLANES [ROWS]`.
Driver: `tests/amiga/paintprof.c`. 8-pixel rows, bit phase 4, 80-byte rows, as paintbench.

Row composition is NOT included: `painter_run` (render/amiga_render.c:1678) needs graphics.library
(LockLayer, `planes_ok`, `target` from the RastPort/BitMap, SetWriteMask), none of which runs
under vamos. The painter is driven with the runs each shape gives it (shapes from
tests/amiga/phaseprobe.c:44-62): sgr-colour = 8 runs of 8 cells, pens j on 0; sgr-perchar = 40
runs of 1 cell, pens j%8 on 0; pen1on0 = one run of 77 cells, pen 1 on 0, one plane (the
`vp_asm_plane` row 7 path). Each shape ends its rows at the last run; blank cells are not painted.
Row counts differ (machine load 60-110 made 2000 rows impractical for two shapes); shares do not
depend on the row count. `vp_span_fast` includes the inlined `vp_span` body in painter.c.

## sgr-colour, 3 planes, 2000 rows (16000 runs)
15953993 instructions, 438272 in libraries.

| function | instr | share | footprint |
|---|---|---|---|
| asm:vp_asm_plane | 12144000 | 76.1% | 108 B |
| painter:vp_span_fast | 3360000 | 21.1% | 192 B |
| paintprof:main (driver) | 400942 | 2.5% | 270 B |
| painter:? | 48000 | 0.3% | 6 B |
| paintprof:? | 768 | 0.0% | 334 B |
| ? (startup) | 283 | 0.0% | 388 B |

Hottest painter lines: asm 12144000; painter.c:60 glyph pointers 1296000; :66 456000; :68 432000; :62 384000.

## sgr-perchar, 3 planes, 60 rows (2400 runs of 1 cell)
1414483 instructions, 50442 in libraries.

| function | instr | share | footprint |
|---|---|---|---|
| asm:vp_asm_plane | 986400 | 69.7% | 90 B |
| painter:vp_span_fast | 352800 | 24.9% | 192 B |
| paintprof:main (driver) | 67222 | 4.8% | 270 B |
| painter:? | 7200 | 0.5% | 6 B |
| paintprof:? | 598 | 0.0% | 334 B |
| ? (startup) | 263 | 0.0% | 388 B |

Hottest painter lines: asm 986400; painter.c:66 68400; :68 64800; :62 57600; :60 43200; :61 43200; :63 28800.
Per run (painter only): 558 instructions for 1 cell (asm 411, vp_span_fast 147); sgr-colour: 969 for 8 cells
(asm 759, vp_span_fast 210). A cell costs 558 here against 121 there: the per-call cost dominates short runs.

## pen1on0, 1 plane, 200 rows (77 cells a row)
688698 instructions, 20067 in libraries.

| function | instr | share | footprint |
|---|---|---|---|
| asm:vp_asm_plane | 518000 | 75.2% | 96 B |
| painter:vp_span_fast | 153000 | 22.2% | 162 B |
| paintprof:main (driver) | 16142 | 2.3% | 240 B |
| painter:? | 600 | 0.1% | 6 B |
| paintprof:? | 683 | 0.1% | 334 B |
| ? (startup) | 273 | 0.1% | 388 B |

Hottest lines: asm 518000; painter.c:60 gp[i] = glyphs + chars[i]*h 140400 (20% of the shape; the
glyph-pointer loop, 1825 instructions a 77-cell row); :68 3400.
Per row: 3443 instructions, of which vp_asm_plane 2590 (about 34 a cell, 8 pixel rows), glyph pointers 702.

## Reading
- The 68k plane loop is 70-76% of the painter's instructions in every shape; its footprint (90-108 B)
  fits the 256-byte cache. The C glue (vp_span_fast, 162-192 B) is 21-25%, of which the glyph-pointer
  line (painter.c:60) is the largest part when runs are long.
- Short runs (sgr-perchar) pay the per-call cost on every cell: 558 instructions a cell against 121 in sgr-colour.
- Not measured: row composition (graphics.library), WaitBlit/LockLayer, chip-RAM bus waits. These need the rig.
