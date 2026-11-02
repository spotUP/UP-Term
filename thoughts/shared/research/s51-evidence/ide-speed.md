---
date: 2026-11-02
topic: IDE page-down speed (scidemo-lite), brief r8-ide-speed
status: measured 2026-11-02 rig 66 --a1200; no speed fix landed; handler regression found
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

## 2026-11-02 run (R8, rig 66 --a1200, kit-main terminfo, out/scidemo-lite of 19 Oct)

Bench unblocked: ide-spike d9cbcb3 makes bench_rig.py take terminfo from paths.ports()
(UPTERM_PORTS=upterm-ports/.claude/worktrees/kit-main).

### Correction of point 2 above
The "no ?2026" observation came from old captures (memo-backup) that predate ncurses patch 0008.
Patch 0008 (doupdate brackets a changing frame with Sync), 0001 (one write per frame), 0002/0003
(tparm memo) and 0004 (vid_puts memo per old/new attribute) are all in upterm-ports pkgs/ncurses
already; the vtcon terminfo declares Sync; the binary looks it up. New cap_scroll.bin (8869 B,
two pages): 2 x ESC[?2026h, 2 x ESC[?2026l, starts with ?2026h and ends with ?2026l.
vtwin_write renders once on the ?2026l and the frame tick holds while sync is open (render/vtwin.c
~539, ~1221), so colours and text arrive in one frame (text check of the stream + code path;
not eye-checked). Brief levers 2 and 3 (sync, vid_puts/tparm cache) were already shipped on 19 Oct:
results/scidemo-lite-a1200.hand-sync-*.txt measured 1007-1017 ms with them.

### Numbers (ms per page, scroll = 20 x PageDown)
| run | scroll | paint | tty (doupdate) | write per page |
|---|---|---|---|---|
| 19 Oct rig 3 (recorded) | 995 | 294 | 701 | 2 / 5713 B / 453 |
| 2 Nov rig 66 run 1 | 1233 | 237 | 996 | 2 / 4484 B / 775 |
| 2 Nov rig 66 run 2 | 1251 | 233 | 1018 | 2 / 4484 B / 778 |
| 2 Nov rig 66, PC sampler on (BENCH_FLAGS=p) | 1846 | 688 | 1158 | 2 / 4484 B / 901 |

Scintilla paint is down (294 -> 235) and the stream is 22% smaller, but the time blocked in
write() went from 453 to ~776 ms per page: the handler (XCON task) costs ~70% more per page
than on 19 Oct. Sampler task split (2 ms ticks, sampler run): XCON 60%, vsh/scidemo 29%, rest
other tasks. The goal (<= 500 ms) is out of reach from the app side: even a zero-cost doupdate
leaves ~776 ms of handler time per page.

### Next (not done, out of budget)
1. Find the handler regression since 19 Oct: same bench, handler builds bisected between the
   19 Oct vtcon commit and main (or rig 66 vs rig 3 screen mode: depth/planes change render cost;
   check the window's screen first). Handler PROF build (VTCON_PROF, vtwin_write PROF_IN/OUT
   phase 3 = vt_feed, phase 2 = render) splits feed vs render per page.
2. Then render cost per sync frame (compose/convert/blit) as in brief item 4.
Raw before files (scratch, small): r8b/pcs-before.txt, r8b/before-p.txt.
