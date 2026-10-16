---
date: 2026-10-16
topic: RT1/W8 retro32-term (DCTelnet Built-in renderer) against ibmcon.device, same input, rig 3
status: partial -- whole-run totals measured; no per-phase split exists in the tooling (see Gaps)
---

# RT1 / W8: retro32-term against ibmcon

## Tooling found (reused, nothing new written)
- tools/rig/phase_rig.py and conbench_rig.py profile UP-Term's own handler (feed / draw / rest via PROF=1 serial lines). They do not drive DCTelnet.
- tools/rig/cc_rig.py named by the plan does not exist on main (branch feature/cc1-cc2 only).
- DCTelnet itself ships the benchmark: `DCTelnet --test-speed` (~/Code/DCTelnet/dctelnet-vtcon, src/DCTelnet.c:2447 SpeedTest, :2814 entry). It pipes PROGDIR:speedtest.ans (tests/test-speed.ans, 37 KB) through ConWrite() and prints `renderer_duration_seconds` (CurrentTime(), 0.1 s steps). The renderer is the one in PROGDIR:DCTelnet.Prefs (State bit 9 Built-in, bit 12 ibmcon.device).

## Method
Rig 3 (`UPTERM_RIG=3 rig.py start --max`, 060, KS 3.1.4 40.63), existing build/vbcc-68020-release/DCTelnet.68020 (built 2026-10-04 16:35, after the last src commit f94d59d, so current). Two drawers in build/rig3/vtc: dct_bi (State 0x02000300, Built-in) and dct_ibm (State 0x02001100, ibmcon.device, Devs/ibmcon.device bundled), same binary, same fonts, IBM.font 8x8. The 37 KB file ran in 0.1 s on both (below the timer's resolution), so speedtest.ans was the same file concatenated 100 times (3.72 MB). Command: `ami.py exec "VTC:dct_<v>/DCTelnet --test-speed"`, alternating, two runs each. Logs: scratchpad/fp32-timed.log.

## Table (seconds for 3.72 MB of ANSI art, lower is faster)
| Phase | retro32-term (Built-in) | ibmcon.device | Ratio |
|---|---|---|---|
| whole run: parse + draw + scroll + blit wait (not separable, see Gaps) | 8.0, 10.1 (mean 9.05) | 7.7, 8.6 (mean 8.15) | 1.11 (Built-in slower) |
| parse / draw / blit / scroll separately | not measured | not measured | - |

Noise: Built-in varies 8.0 to 10.1 between runs (emulated bus load), ibmcon 7.7 to 8.6; the ratio is 1.04 best-to-best and 1.17 worst-to-worst. Built-in is not faster than ibmcon on this input, so the RT1 "done" condition (faster than ibmcon) is NOT met. Unverified: which Built-in path ran (direct 4-plane planes or the RastPort/Text path); the screen is the prefs' 640x256 own screen and the depth was not read back.

## Gaps
- No existing tool splits a DCTelnet run into phases. The plan's phase profile (feed / draw / rest) needs timers in DCTelnet around the vtcon engine feed and the term-engine.c draw calls (as vtcon-handler's PROF=1 does), plus an E-clock stamp. Not built within this run's budget.
- No instruction counts (tools/prof68k.py is for the vtcon engine only).

## Slowest phase and likely causes (from code reading, not measured)
The slowest part is the draw/blit side, not the parse, because the engine is the same one UP-Term uses. Two most likely causes:
1. Per-run Text() plus a forced WaitBlit before the next CPU touch: src/third_party/retro32-term/term-engine.c:166-183 (term_run_flush: SetAPen, SetBPen, SetDrMd, Move, Text per colour run, then blit_pending = 1) and :185-190 (term_blit_sync WaitBlit). ANSI art changes colour every few cells, so each run pays the graphics.library calls and blitter wait (RT1 "no overlap" suspect, plan CC1).
2. Scroll as BltBitMap plus a rectangle fill every line feed, with no jump scroll batching: term-engine.c:428-441 (term_scroll_up: full-height BltBitMap, blit_pending, then term_rect_fill per line). Art that scrolls on every row costs one full-window blit per line.
