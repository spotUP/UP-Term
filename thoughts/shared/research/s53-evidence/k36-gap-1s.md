---
title: K36-GAP-1S, kitty animation with a 1000 ms frame gap on a rig
date: 2026-11-03
session: vtcon-ee s53, rig 93, branch s53-k36-gap (worktree ../vtcon-s53-gap)
---
# K36-GAP-1S

Verdict: PASS on RTG and AGA. The frame clock keeps pace at a 1 s gap: one switch per second, none lost or doubled.

Spec number: the K36 plan states no number for a 1 s gap, only the rule "at most one frame switch per wake, no catch-up,
armed for max(due, pace)". Derived expectation: switches = window life in seconds / 1.0 (+/- 1), and a neighbour-shot
pair changes in about span/gap cases.

Tool: tools/rig/kitty_anim_rig.py got an optional third argument gap-ms (default 100, old behaviour unchanged). Above
100 ms it takes 17 shots 0.5 s apart (about 9.3 s) and counts changed neighbour pairs.
Command (from the worktree, PROF=1 handler, under paths.long_run()):
  PROF=1 UPTERM_RIG=93 python3 tools/rig/rig.py start --max --purpose "..."
  kitty_anim_rig.py <prefix> on 1000        (RTG 800x600, then `rig.py aga`, restart, again: AGA 640x256)

| screen | span | changed pairs of 16 | expected | PROF switches (hex, window life) | expected life |
|---|---|---|---|---|---|
| RTG | 9.3 s | 9 | 9.3 | 0xf = 15, then 0x10 = 16 | about 15.3 s from stream start to shots end, +close |
| AGA | 9.4 s | 9 | 9.4 | 0xf = 15, then 0x10 = 16 | same |

Every changed pair differs by exactly 4096 = 64*64 pixels (the picture only). The 0.5 s sampling makes unchanged
pairs fall where two shots land in one 1 s frame; the pattern is irregular because the shots are not phase locked.
Compare the 100 ms proof (562f588e): AGA 68/69, RTG 79/80 over the same window life: the 1 s run is 1/4-1/5 of that
rate times 10 gap ratio, consistent with 1 switch per second.
PROF "switches/ticks": ticks column is render time (prof_anim), not a tick count.
Unverified: no tick-rate (idle wake) measure of "no CPU between frames"; PROF has no wake counter.
