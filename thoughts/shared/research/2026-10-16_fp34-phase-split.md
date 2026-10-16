---
date: 2026-10-16
topic: FP3.4 / ASM1 step 1 - handler time split per conbench shape (waiting, vt_feed, drawing, rest)
commit: eb3473dd
---

# Phase split per conbench shape

Method: tools/rig/phase_rig.py (existing) with the conbench flags overridden to '--max' (68060, rig 3), because the script's default --stock --os32 needs the AmigaOS 3.2 tree, absent on this Mac. Handler built PROF=1 SERIAL=1 into BUILD=build/prof (deleted after), copied into build/rig3/vtc; normal build/amiga untouched and restored on the rig. Times in ms, from the handler's eclock counters (not 1/50 s ticks). wait = idle; non-wait = feed+draw+rest. Rig is not cycle-exact; the Mac load average was 44-116 during the runs.

Uptime: run 1 before: 3 days,  6:40, 6 users, load averages: 72,44 62,81 71,15; after: 3 days,  6:42, 6 users, load averages: 64,19 61,89 69,46
Uptime: run 2 before: 3 days,  6:42, 6 users, load averages: 71,06 63,36 69,93; after: 3 days,  6:44, 6 users, load averages: 116,10 84,17 77,49

| run | shape | total | wait | feed | draw | rest | feed % | draw % | rest % (of non-wait) |
|---|---|---|---|---|---|---|---|---|---|
| 1 | plain-lines | 1352 | 1138 | 18 | 5 | 191 | 8 | 2 | 89 |
| 1 | scroll-nl | 1352 | 1142 | 15 | 2 | 193 | 7 | 1 | 92 |
| 1 | clear-page | 1356 | 1135 | 21 | 6 | 193 | 10 | 3 | 88 |
| 1 | sync-line | 4038 | 1227 | 24 | 229 | 2558 | 1 | 8 | 91 |
| 1 | wrap-long | 1321 | 1130 | 9 | 1 | 182 | 5 | 1 | 95 |
| 1 | bytewise | 1458 | 1168 | 33 | 16 | 241 | 11 | 6 | 83 |
| 1 | sgr-colour | 1540 | 1171 | 58 | 79 | 232 | 16 | 21 | 63 |
| 1 | sgr-perchar | 1387 | 1142 | 17 | 44 | 183 | 7 | 18 | 75 |
| 2 | plain-lines | 1416 | 1153 | 40 | 21 | 202 | 15 | 8 | 77 |
| 2 | scroll-nl | 1390 | 1147 | 34 | 2 | 208 | 14 | 1 | 85 |
| 2 | clear-page | 1341 | 1134 | 13 | 5 | 189 | 6 | 2 | 91 |
| 2 | sync-line | 4088 | 1226 | 36 | 265 | 2561 | 1 | 9 | 89 |
| 2 | wrap-long | 1322 | 1130 | 4 | 6 | 182 | 2 | 3 | 95 |
| 2 | bytewise | 1392 | 1150 | 18 | 17 | 207 | 7 | 7 | 86 |
| 2 | sgr-colour | 1452 | 1152 | 38 | 52 | 211 | 13 | 17 | 70 |
| 2 | sgr-perchar | 1357 | 1142 | 7 | 26 | 182 | 3 | 12 | 85 |

## Reading

- Only sync-line (a line then WaitForChar barrier, 200 round trips) has real handler work: about 2.8 s non-wait, 91% of it rest (packets, loop), draw 8-9% (200 frames, about 1.2 ms each), feed about 1%.
- Every other shape finishes in about 1.3-1.5 s with wait ~1.13-1.17 s and a near-constant rest of 182-208 ms regardless of bytes or writes: that is a floor (startup or close), not per-shape work. Feed and draw are under 60 ms each (sgr-colour is the largest: feed 38-58, draw 52-79).
- On the 060 the throughput shapes are too short to rank paths: feed+draw is under 10% of the run. Rank by sync-line (rest = packet and loop path) first, then sgr-colour draw. The 68020/68000 rigs (--a1200, --a500) are needed to see feed/draw cost.
