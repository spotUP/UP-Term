---
date: 2026-11-10
topic: m68k RIP drawing hot paths (68000 / A500)
branch: m68k-hotpaths (worktree /Users/spot/Code/UP-Term/vtcon-m68k-hotpaths), base 7365004f, head 9b890247
status: H4 + H3 done, H2 + H1 open
---

# m68k RIP hot paths

## Step 0
- The ">5 min on 7365004f" figure was WITHDRAWN per coordinator: it was an A500 reset during the feed
  (illegal instructions in user RAM on rig 2, `0088 at 002305E8`, `00fa at 0023C260`, chased by dctelnet-74).
  No rig booked, no rig timing taken.
- Static note: 7365004f's row path (fill_span via cv_hspan/cv_hrow) cannot be slower than the old
  per-pixel path: old ~1250 cy/px (gput + cv_pixel), new ~900 cy fixed per row + ~8/px solid or ~70/px
  patterned; even a 1-px span is cheaper. 07-buttons.rip (228 bytes) makes 1730 gput calls at 7365004f.
  Flood fill still does per-pixel cv_get + py*cv->w multiply and calloc(w*h) per flood (unverified hog).

## Analysis
- Invariant violated: a per-pixel primitive on the 68000 must cost compare + store, not a 32x32
  multiply (VBCC: 3 mulu.w, ~200 cy) + struct copy + call.
- Levels: compiler flags (VBCC has none that narrow y*w; probe: `(unsigned short)y*(unsigned short)w`
  still emits 3 mulu.w), canvas core (root: y*w per pixel), callers (per-pixel calls from blit/glyph/line).
- Chosen: row-pointer table in struct cv (row[y] = pix + y*w, CV_ROWS_MAX 1024), inline dirty widen,
  then callers batch work per row / per primitive.

## Results (cycle figures: static, from VBCC `-O3 -speed -S`; base cy from peer audit)
| Target | Commit | Before | After |
|---|---|---|---|
| H4 cv_pixel | 5f22df5f | 103 instr, 3 mulu, ~900-1000 cy | 52 instr, 0 mulu, ~400 cy est. |
| H4 cv_hspan | 5f22df5f | 149 instr, 3 mulu, ~900 cy/row fixed | 95 instr, 0 mulu |
| H4 cv_hrow | 5f22df5f | 158 instr, 3 mulu | 104 instr, 0 mulu |
| H3 cv_blit | 9b890247 | ~1550 cy/px (mul + 4 cmp + cv_get + cv_pixel) | XOR inner loop 8 instr ~66 cy/px; one mul per call |

Test: tests/test_rip.c `hot_paths_are_pixel_exact` - 19 RIP streams through the real RIP entry
point, canvas sum + dirty box vs gold recorded from 7365004f code. Rip suite: 775 checks, 0 failed
(756 old + 19 new). Wiring: old per-pixel paths deleted, so the only path is the new one.

## Open
- H2 glyph blit (rip_draw.c ~490-509): clip to viewport and canvas, step by s, write c->row[] (XOR when
  wmode==1), bbox locals, one cv_dirty per glyph. Not started.
- H1 line chain (rip_draw.c ~187-210 pen/gput): inline viewport test, row pointer, thick pen, one cv_dirty
  per line. Not started.
- Runner-ups: pattern buffer (rip_draw.c 153-161), __divs in fill_poly (311) and sin16/arc_points,
  rip_icon.c grab() per-px cv_get, flood fill multiply/calloc.
- No timed rig measurement of any target yet.

## H2/H1 (s70 agent 5)
Branch m68k-hotpaths. All counts and cycles are ESTIMATES from vbcc `-O3 -speed -cpu=68000` asm (`vc -S`, functions
un-static'd in a scratch copy with -inline-size=0 so each is counted alone); nothing was timed on a rig.
- Gold first: 35edc185 adds 7 streams to hot_paths_are_pixel_exact (cases 19-25: thick/patterned/XOR lines across
  every canvas edge, dotted axis lines, text scales 2/3/5 across and up, XOR text, clipped glyphs, viewport clip),
  gold recorded from the pre-change code.
- H2 f4dd77a3 glyph(): was inline in rip_gfx_exec (+92 instr there) with per-set-pixel gput 33 + cv_pixel 52 instr and
  3 emulated multiplies (x, y, k*s / r*s) per set bit: ~100 instr, 3 mulu.w ~ 800+ cy per set pixel (the 1700 cy
  figure predates H4). Now glyph() 316 instr total, 3 mulu.w per GLYPH (font row index, none per pixel), scale stepped by
  addition, blocks clipped once, row-table writes, one cv_dirty per glyph. Estimate per set pixel at scale 1: ~35 instr,
  ~300 cy (about 3x), better at larger scales (one clip per block, not per pixel).
- H1 1504db31 line(): old chain line 83 + pen 54 + gput 33 + cv_pixel 52 instr, 0 mulu.w, ~100 instr (~800 cy) per
  centre pixel, 3x that for thick. New line() 377 instr, 0 mulu.w, ~45 instr (~350 cy) per centre pixel, thick adds ~25
  instr per side pixel; one cv_dirty per line. pen() deleted (poly_line single point uses line()).
- Canvas height: cv_open and cv_open_pix are the only creators (grep + the two definitions in canvas.c); both refuse
  h > CV_MAX (= CV_ROWS_MAX 1024); production fixed 640x350. Test added (see commit "both canvas creators refuse").
- Open: fill_poly line 341 divide (per scanline per edge; an incremental DDA must reproduce the truncating rounding
  with the sign-dependent bias, not started), sin16/arc_points, pattern buffer, rip_icon grab(), flood fill.
