---
date: 2026-10-16
topic: FP3.4 / FP3.7 - handler time split per conbench shape on the cycle-exact 68020 (rig 3, --exact), RastPort path vs DIRECT=1
commit: eb3473dd
---

# Phase split on the 68020 cycle-exact, rastport vs direct

Method: as 2026-10-16_fp34-phase-split.md (phase_rig.py main() via a wrapper in the scratchpad with conbench_rig.FLAGS = ['--exact']; no tracked file edited). Rig 3, `rig.py start --exact` (68020 cycle-exact, no JIT: emulated time counts cycles, so Mac load does not distort). Handlers PROF=1 SERIAL=1 built into build/prof (default) and build/prof-direct (DIRECT=1); both deleted afterwards, normal handler restored into build/rig3/vtc (md5 c1e7e8e1 = build/amiga). All 8 shapes ran. ms from the handler's eclock counters.

Repeat check: the direct build was run twice (first boot attempt hung with "daemon did not answer for 150 s", rig stopped and restarted; the second boot is the data below). The rastport build ran once. Non-wait totals of the two direct attempts are not comparable (the first produced no data). Rastport vs direct differences below are therefore single runs each; the cycle-exact repeatability was not measured by a same-build repeat.

| shape | build | total | wait | feed | draw | rest |
|---|---|---|---|---|---|---|
| plain-lines | rastport | 3317 | 1401 | 634 | 837 | 445 |
| plain-lines | direct | 3289 | 1381 | 639 | 826 | 443 |
| scroll-nl | rastport | 3274 | 1499 | 683 | 447 | 645 |
| scroll-nl | direct | 3294 | 1513 | 681 | 450 | 650 |
| clear-page | rastport | 3680 | 1647 | 596 | 973 | 464 |
| clear-page | direct | 3701 | 1646 | 605 | 993 | 457 |
| sync-line | rastport | 5598 | 1265 | 172 | 1824 | 2337 |
| sync-line | direct | 5522 | 1282 | 174 | 1749 | 2317 |
| wrap-long | rastport | 1807 | 1205 | 230 | 221 | 151 |
| wrap-long | direct | 1866 | 1250 | 228 | 233 | 155 |
| bytewise | rastport | 4192 | 1905 | 681 | 385 | 1221 |
| bytewise | direct | 4192 | 1905 | 679 | 384 | 1224 |
| sgr-colour | rastport | 6958 | 2243 | 1124 | 2322 | 1269 |
| sgr-colour | direct | 6858 | 2231 | 1122 | 2223 | 1283 |
| sgr-perchar | rastport | 3114 | 1190 | 375 | 1436 | 113 |
| sgr-perchar | direct | 3055 | 1199 | 377 | 1365 | 114 |

## Draw breakdown (rows incl. painter / scroll / cursor+mask, ms)

| shape | rastport | direct |
|---|---|---|
| plain-lines | 784 (363) / 0 / 45 | 768 (350) / 0 / 48 |
| scroll-nl | 224 (0) / 0 / 188 | 227 (0) / 0 / 189 |
| clear-page | 889 (385) / 12 / 59 | 898 (377) / 24 / 57 |
| sync-line | 778 (440) / 406 / 540 | 893 (556) / 222 / 535 |
| wrap-long | 186 (122) / 0 / 29 | 194 (126) / 0 / 32 |
| bytewise | 132 (81) / 2 / 204 | 129 (78) / 2 / 205 |
| sgr-colour | 1619 (1090) / 552 / 127 | 1873 (1280) / 158 / 163 |
| sgr-perchar | 1375 (885) / 45 / 14 | 1318 (847) / 30 / 14 |

## Reading

- "0 Text runs" in both builds: the default build already draws every run through painter_68k.s (direct_wins picks it for the shapes here); DIRECT=1 changes only the always-direct choice and the scroll (BltBitMap instead of ScrollRaster). Draw total therefore differs by at most ~5% (sync-line -4%, sgr-colour -4%, sgr-perchar -5%, rest within 2%), near the single-run noise; the only large movement is the scroll component (sync-line 406 -> 222, sgr-colour 552 -> 158), offset by a larger rows component.
- Top non-wait costs on the 020 (both builds): 1) rest (packet/loop path; sync-line 2.3 s, bytewise 1.2 s, sgr-colour 1.3 s, scroll-nl 0.65 s), 2) draw on the colour shapes (sgr-colour 2.2-2.3 s, sgr-perchar 1.4 s) and feed on the throughput shapes (plain-lines 0.64 s, sgr-colour 1.12 s). Feed is no longer negligible as on the 060.
