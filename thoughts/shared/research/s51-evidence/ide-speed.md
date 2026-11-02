---
date: 2026-11-02
topic: IDE page-down speed (scidemo-lite), brief r8-ide-speed
status: blocked - no new measurement taken, no code changed
---

# IDE page-down speed: findings, no fix yet

## Baseline numbers (existing ide-spike results, not re-measured)

- The ~995 ms/page baseline is a1200 mode (68020 at real speed): results/scidemo-lite-a1200.txt
  has avail_start_kb=68793, and bench_rig.py machine_ok treats under 100000 KB as a1200.
  scroll_ms=995 = scroll_paint_ms 294 + scroll_tty_ms 701; 2 writes, 5713 bytes, 453 ms of write per page.
- 060 --max mode (host speed): results/scidemo-lite-060.txt shows scroll_ms=60. The goal of 500 ms/page
  only makes sense in a1200 mode; every after-number must come from `--a1200` on the same rig.

## Bench blocked on rig 66

`UPTERM_RIG=66 BENCH_FLAGS=p python3 bench_rig.py --machine 060 --variants scidemo-lite --runs 1 --force`
fails before the run: bench_rig.py copies TERMINFO = upterm-ports/build/tools/terminfo, and that directory
does not exist (upterm-ports/build/tools is gone). Rebuild the upterm-ports tools terminfo, or point
TERMINFO at another compiled vtcon entry, before any bench.

## Code facts that change the plan

1. Brief item 2 (lex the visible range before paint) is already in ScintillaCurses::NoutRefresh
   (ide-spike src/scinterm/scinterm-scinterm_6.0/ScintillaCurses.cxx about line 505): the invalidated
   area is styled with StyleAreaBounded before Paint, and an abandoned paint repaints the full area in a
   second pass. Not yet measured: how many paint passes a page-down takes (add a pass counter sentinel).
2. scidemo output carries no ?2026 synchronized-output markers. Old page-down captures
   (ide-spike build/memo-backup/cap_scroll_*.bin, 8.8 KB): sync_h 0, sync_l 0, about 1240 SGR and 47 CUP
   per page. The page leaves ncurses in 2 writes (5713 bytes, over a 4 KB buffer), so the handler's frame
   tick can render between them: the top of the page with its text, the rest (and colours) one frame later.
   vtwin_write already renders at once when a ?2026 frame ends (render/vtwin.c vtwin_write), so wrapping
   doupdate output in ?2026h/?2026l (ncurses patch in upterm-ports, or the app) gives one render per page
   with colours in the same frame. Unverified on the rig.
3. The handler render runs inside ACTION_WRITE (vtwin_write calls vtwin_render on sync end and on layout
   change) and on the frame tick; on one CPU the per-page cost is total CPU, so pipelining the reply does
   not help. The cost split still needs the PC sampler (BENCH_FLAGS=p) mapped with pcmap.py.
4. Earlier probes (results/redraw.md): vid_puts about 115 ms/page and mvcur 60 ms/page on the 020, tparm
   re-analysis per call; caching formatted setaf/setab per colour in ncurses is the next ncurses candidate.

## Order for the next run

1. Restore the terminfo source; bench a1200 mode with BENCH_FLAGS=p twice; map pcs.txt.
2. Fix the biggest measured cost. Likely: ?2026 around doupdate (one render per page, colours same frame),
   then ncurses vid_puts/tparm caching.
3. Re-bench twice on the same rig and mode; host test per change.

No commits in ide-spike (branch s51-ide-speed exists, unchanged).
