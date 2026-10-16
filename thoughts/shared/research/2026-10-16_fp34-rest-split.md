---
date: 2026-10-16
topic: FP3.4 - the "rest" phase split into its parts, cycle-exact 68020, rig 3 --exact, rastport build
commit: eb3473dd
---


# Where "rest" goes (68020 cycle-exact, PROF=1 SERIAL=1)

Method: PROF-only sub-counters in handler/vtcon_handler.c (enum PS_*, PROF_T/PROF_ADD, REPLY_T; printed as `v s-<name> <ticks> <count>` at stream close), run through phase_rig.main() with conbench_rig.FLAGS=['--exact'] on rig 3 (one run per shape, all 8 shapes in one boot; first boots hung with "daemon did not answer", rebooted). Same EClock source as the old phases (709379 Hz). ms below.

Caveat that bounds everything: ReadEClock costs 41 us per call here (200 back-to-back calls = 8.18 ms, printed as `s-cal200`). A counter of n events carries about 2 x 41 us x n of its own cost, so any part under ~100 us per call (GetMsg, ReplyPkt, the loop checks) reads as instrumentation, not as work. The old phases (idle, packet, out, render) carry the same cost: about 7 ReadEClock calls per write = 0.29 ms a write = ~700 ms of bytewise's "rest".

| shape | rest | write pkt (-feed) | wait pkt (-render) | s-pkdbg | s-rpdbg | reply | getmsg | looppre | loop tail (-tick) | wrpre | wrsvc | tailsvc |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| sync-line (200 wr, 201 wc) | 2931 | 297-185=112 | 4119-1866=2253 | 1102 / 403 | 1072 / 201 | 35 / 401 | 51 / 985 | 59 / 582 | 161-25=136 / 582 | 9 / 200 | 15 / 200 | 43 / 582 |
| bytewise (2400 wr) | 3162 | 2071-733=1338 | - | 124 / 2403 | 2 | 200 / 2401 | 260 / 5010 | 271 / 2607 | 1642-1151=491 / 2607 | 112 / 2400 | 176 / 2400 | 193 / 2607 |
| sgr-colour (2700 wr) | 3500 | 2647-1128=1519 | - | 136 / 2703 | 5 | 229 / 2701 | 292 / 5559 | 275 / 2856 | 3972-3447=525 / 2856 | 127 / 2700 | 199 / 2700 | 212 / 2856 |
| scroll-nl (1200 wr) | 1655 | 1382-709=673 | - | 69 / 1203 | 5 | 100 / 1201 | 131 / 2520 | 128 / 1317 | 1012-765=247 / 1317 | 57 / 1200 | 88 / 1200 | 97 / 1317 |

(ms / count. "tick" = vtwin_tick, whose time is the draw phase.) Totals this run: sync-line 6216 ms, bytewise 7132, sgr-colour 10276, scroll-nl 4869 (the previous run without these counters: 6072, 6872, 9896, 4734: the added counters cost 2-4 %).

## Findings

1. sync-line: 2174 of its 2931 ms "rest" is the SERIAL trace itself. packet() prints `v packet ...` for every packet that is not WRITE/READ, and reply() prints `v reply ...` the same way, so each WaitForChar packet pays two serial lines at about 5.4 ms each (s-pkdbg 1102 ms in 201 wait packets, s-rpdbg 1072 ms in 201 replies). A normal build has neither (DBG is empty). The real sync-line rest is about 750 ms; ReplyPkt itself is 35 ms for 401 replies (0.09 ms each, at the 2 x ReadEClock floor), the loop around it ~250 ms, ahead_feed 18 ms. The 200 WaitForChar waits cost the handler nothing but the vtwin_render the packet forces (1866 ms, draw phase, one frame per packet: 9.3 ms a frame) -- that is draw, not rest.
2. Write-heavy shapes (bytewise, sgr-colour, scroll-nl): "rest" is 0.56-1.3 ms a write and is spread thin, with no single part above ~0.1 ms once the ReadEClock floor (~0.08 ms per counter pair) is removed:
   - the write packet outside vt_feed: 0.28 ms a write in bytewise (1338 ms / 2400 = 0.56 ms, of which reply 0.08, service_reads 0.07, entry 0.05 are counters' own cost plus ~7 ReadEClock calls in packet()+output()+the loop that are the existing phase timers);
   - the main loop per iteration: GetMsg (two calls an iteration), the mask build to Wait (looppre), the tail checks and service_reads: 0.27 + 0.10 + 0.19 + 0.07 = ~0.6 ms per iteration in bytewise, every count within 2x of the floor.
   So the cycle-exact "rest" in bytewise/sgr-colour is mostly per-packet overhead that is the same size as the timer reads themselves; it cannot be split further with ReadEClock. A cycle count from the emulator (FS-UAE profiler / wasabi snoop) or a build that times 100 packets with one pair is needed to resolve below 100 us.
3. Per packet (rest / writes): bytewise 1.32 ms, sgr-colour 1.30 ms, scroll-nl 1.38 ms, sync-line 14.7 ms (trace) -> 3.7 ms without the two serial lines.

## Top two sub-parts

1. handler/vtcon_handler.c:6065 (`DBG("packet", ...)` in packet()) and :486 (`DBG("reply", ...)` in reply()): 2174 ms of sync-line's rest, present only in SERIAL builds. The earlier "sync-line rest 2.3 s" was this; the PROF=1 SERIAL=1 split overstates WAIT_CHAR by ~11 ms a packet. Fix for the measurement: make the packet/reply trace skip ACTION_WAIT_CHAR under VTCON_PROF.
2. The ACTION_WRITE path outside vt_feed plus the loop around it, handler/vtcon_handler.c:6229 (reply) / :6233 (service_reads after the reply) and the loop tail (:6793-6856): ~1.3 ms a write in bytewise together, but each piece is <= 0.1 ms and within the 0.08 ms ReadEClock floor, so a fix target (e.g. skipping the second service_reads, which runs at :6233 and again in the loop tail for the same packet) is a guess until measured with a cheaper clock.
