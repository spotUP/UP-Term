---
date: 2026-10-16
topic: FP3.4 - phase split with the trace-free PROF instrument (PROF=1, no SERIAL=1), 68020 cycle-exact, rig 3 --exact
commit: 4a909c3e
---

# Phase split without the per-packet trace

Method: `make build/amiga/vtcon-handler PROF=1` (no SERIAL=1), copied to build/rig3/vtc, phase_rig.main() with conbench_rig.FLAGS=['--exact'], UPTERM_RIG=3, all 8 shapes in one boot (no hang). serial.log: 224 `v PROF` lines, 0 `v packet`, 0 `v reply`. Same mode as 2026-10-16_fp34-rest-split.md (--exact, rig 3). Default handler rebuilt afterwards (no PROF symbols) and reinstalled; rig stopped. ms.

| shape | total | waiting | vt_feed | drawing | rest | writes | rest / write (ms) | pkwrite (ms / pkts) | pkwait |
|---|---|---|---|---|---|---|---|---|---|
| plain-lines | 3260 | 1378 | 638 | 838 | 405 | 816 | 0.50 | 808 / 816 | 50 / 1 |
| scroll-nl | 3271 | 1526 | 689 | 449 | 607 | 1200 | 0.51 | 944 / 1200 | 7 / 1 |
| clear-page | 3677 | 1656 | 596 | 1001 | 425 | 840 | 0.51 | 773 / 840 | 47 / 1 |
| sync-line | 3552 | 1284 | 162 | 1818 | 288 | 200 | 1.44 | 205 / 200 | 1754 / 201 |
| wrap-long | 1766 | 1197 | 230 | 219 | 119 | 140 | 0.85 | 260 / 140 | 0 / 1 |
| bytewise | 4449 | 2138 | 698 | 433 | 1180 | 2400 | 0.49 | 1205 / 2400 | 5 / 1 |
| sgr-colour | 6516 | 2005 | 1119 | 2172 | 1220 | 2700 | 0.45 | 1683 / 2700 | 29 / 1 |
| sgr-perchar | 3214 | 1213 | 375 | 1547 | 78 | 60 | 1.30 | 388 / 60 | 189 / 1 |

## Rest, old (rest-split, trace + sub-counters) vs new

| shape | old rest | new rest | old total | new total |
|---|---|---|---|---|
| sync-line | 2931 | 288 | 6216 | 3552 |
| bytewise | 3162 | 1180 | 7132 | 4449 |
| sgr-colour | 3500 | 1220 | 10276 | 6516 |
| scroll-nl | 1655 | 607 | 4869 | 3271 |

## Reading

- sync-line: rest fell 2931 -> 288 ms; the serial trace was ~90 % of it (matches the 2174 ms estimate). Its total is now dominated by drawing (1818 ms, 200 frames) and waiting.
- Write-heavy shapes: rest is 0.45-0.51 ms a write (old: 1.3 ms). That is still about 7 ReadEClock calls a write at 41 us each (~0.29 ms, see rest-split caveat), so roughly 0.2 ms a write is real work outside vt_feed/draw/wait; ~1180/1220 ms in bytewise/sgr-colour remain, of which ~60 % is the timer's own cost. Not resolvable further with ReadEClock.
- Rest as a share of non-wait time: bytewise 51 %, sgr-colour 27 %, scroll-nl 35 %, sync-line 13 %.
- Caveat: pkwrite - vt_feed per write (bytewise 0.21 ms, sgr-colour 0.21 ms) is the packet path outside the engine.
- One run per shape; the Mac load was not recorded.

## Drawing split (same run, build/rig3/serial.log PROF lines, ms)

Parsed by the main session from the renderer's PROF lines (rows/scroll, cursor/paint, fill/fills, waitblit).
render = rows + scroll + cursor; paint and fill are inside rows; the rest of rows is row composition.

| shape | render | frames | rows | of it paint | of it fill | scroll | cursor | waitblit |
|---|---|---|---|---|---|---|---|---|
| plain-lines | 838 | 17 | 785 | 365 | 227 | 0 | 45 | 16 |
| scroll-nl | 449 | 70 | 227 | 0 | 83 | 0 | 187 | 0 |
| clear-page | 1001 | 24 | 890 | 384 | 284 | 30 | 65 | 17 |
| sync-line | 1818 | 200 | 772 | 436 | 162 | 401 | 543 | 20 |
| wrap-long | 219 | 11 | 185 | 122 | 16 | 0 | 29 | 3 |
| bytewise | 433 | 134 | 145 | 89 | 40 | 2 | 235 | 8 |
| sgr-colour | 2172 | 32 | 1550 | 1045 | 22 | 511 | 93 | 88 |
| sgr-perchar | 1547 | 4 | 1482 | 958 | 29 | 52 | 11 | 113 |

## Decision (2026-10-16 session 6)

- The session-5 call "ASM1 target #1 = the per-packet path" rested on trace-inflated rest. Clean rest is at most
  1.2 s and about 0.2 ms a write once the ReadEClock cost is taken out: not the target.
- ASM1 target #1: the painter on colour shapes (paint 1045 ms sgr-colour, 958 ms sgr-perchar, about two thirds of
  rows). Target #2: row composition (rows minus paint and fill: about 480-500 ms on both colour shapes).
- Cursor cost is out of line: 543 ms over 200 frames on sync-line (2.7 ms a frame), 235 ms over 134 frames on
  bytewise, 187 ms over 70 on scroll-nl. That is an algorithm question (what the cursor pass redraws), not asm.
- FP3.6 row 7 (one-plane pen-1-on-0 path) belongs to the painter target. Row 12 (ReplyPkt) sits in rest,
  which is now about 0.2 ms a write in all: no measurable gain is left for it.
