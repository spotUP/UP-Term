---
date: 2026-10-29
author: spot
topic: Amarchy P2 status bar probes R2.1 (network query) and R2.2 (idle counter)
status: complete (rig 8, 060; A1200 2026-10-29, FPGA Replay with a real 68060, AmigaOS 3.2); the A1200 interface-down half open
---

# P2 probes R2.1 and R2.2, rig 8 (68060, --max), 2026-10-29

Plan: thoughts/shared/plans/2026-10-29-amarchy-p2-status-bar.md. Probes: tools/probes/p2/netprobe.c and idleprobe.c
(Makefile targets build/amiga/netprobe, build/amiga/idleprobe; both carry `__stack` and the `$STACK` cookie).

## R2.1 bsdsocket on the rig (UAE)

- `OpenLibrary("bsdsocket.library", 0)` succeeds at once (0 ticks). lib_Version.Revision 4.1, id string
  `UAE bsdsocket.library 4.1`. So the rig has a stack-less emulated library: it opens, it is not Roadshow.
- `QueryInterfaceTagList("eth0", <empty tag list>)` (LVO -468) returns 0, no crash.
- `QueryInterfaceTagList("eth0", {TAG_USER+1 -> &slot, TAG_DONE})` kills the rig: wasabid goes away (connection
  refused), FS-UAE stays up and `rig.py start` reports "already running" until `rig.py stop`. Reproduced 3 times
  (full scan, tag+1..24 on eth0, and the single tag TAG_USER+1). The Roadshow IFQ_* tag values are not in
  third_party/roadshow-netinclude or the NDK (grep IFQ_, QueryInterface: only the LVO stub), so the tag numbers
  used are a scan, not the real IFQ_State; but any tag in the list reaching UAE's -468 entry is enough to crash.
  Conclusion: UAE's bsdsocket does not implement the Roadshow query; calling it with tags is fatal.
- No-stack case: not reachable on the rig (UAE always has the library). The A1200 half of R2.1 (Roadshow answers,
  no-stack OpenLibrary failure and its timing) is OPEN; netprobe prints the OpenLibrary result with elapsed ticks
  for that run. Roadshow's real IFQ_* tag numbers must come from the Roadshow SDK before the A1200 run.

### Decision for R2.10 (fallback)
Never call QueryInterfaceTags without first checking the library id string: if `lib_IdString` begins with "UAE" (or
the library version/revision is not Roadshow's), skip the query and show "net: emulated" (or "no network"); only
on a non-UAE library call QueryInterfaceTags. OpenLibrary failure -> "no network". The rig therefore proves only the
OpenLibrary + UAE-detect paths; the up/down row is proved on the A1200 (matches the plan's Risks note).

## R2.2 idle counter (process at priority -128, CreateNewProcTags, 4 KB stack)

- Idle counts per second (3 samples, 1 s each by Delay(50)): 50.19M, 51.95M, 49.83M (spread 4 percent). Calibrate
  over one second at start; a bar needs only the ratio.
- Main task busy for 1 s: 0 counts (the -128 process gets no time while a priority-0 task is ready), so
  load = 1 - counts/calibration is well-defined.
- 50 percent duty (100 ms busy, 100 ms Delay, x10): 51.6M and 56.0M counts over 2 s against 100M for a fully idle
  2 s, i.e. 52-56 percent idle for a nominal 50 percent: usable, +2-6 points bias from the DateStamp polling in the
  busy loop and Delay granularity.
- Overhead on a fixed busy benchmark (60M iterations of a multiply-add, DateStamp ticks, 3 runs each):
  without idle 168 160 150 (sum 478); with idle 152 150 153 (sum 455). The difference is -5 percent, inside the
  run-to-run spread (150-168, 11 percent); no measurable overhead, target under 2 percent met within noise.
  (The first print of the percent line wrapped because of an unsigned subtraction; fixed in idleprobe.c, the
  numbers above are the raw tick lines.)
- Open: the same on the A1200 (68020 real speed, `--a1200` for a timing run) to see whether the count is stable
  enough there (plan Risk 2).

## A1200 halves, 2026-10-29 (192.168.0.74, Kickstart 47.111, Workbench 47.5, Roadshow 1.15)

R2.1 (network):
- bsdsocket.library 4.364 "Roadshow 1.15 68020+ version"; OpenLibrary OK after 0 ticks. The no-stack case is not
  reachable on the A1200 (the stack always runs; wasabi itself rides it).
- The interface is named `replayeth` (ShowNetStatus: "Local host address = 192.168.0.74 (on interface 'replayeth')",
  gateway 192.168.0.1), not `eth0`. netprobe guesses fixed names (eth0, ether0, en0, ppp0, lo0, uae0), so its scan
  printed ret=-1 for every tag: expected, none of those exist. The real proof is Amarchy's own query (R2.10 below):
  ObtainInterfaceList + QueryInterfaceTagList(IFQ_State) answers and the bar shows "replayeth up", net=4.
  Roadshow has no `ifconfig` in C:; the tools are ConfigureNetInterface, RemoveNetInterface, NetShutdown, ShowNetStatus.
- Taking `replayeth` down is not possible here: it is the only interface and wasabi's connection rides on it.

R2.2 (idle counter, A1200 = FPGA Replay with a real 68060 (owner correction 2026-10-29; an earlier note said Emu68 PiStorm, which was wrong)):
- Idle counts per second (3 samples): 11.88M, 11.91M, 11.94M (spread 0.5 percent): stable enough for calibration.
- Main task busy 1 s: 0 counts. 50 percent duty over 2 s: 13.75M (a fully idle 2 s is about 23.8M, so about 58
  percent idle for a nominal 50: same +bias as the rig).
- Overhead on the fixed benchmark (10M iterations, ticks): without idle 83 83 83 (249), with idle 82 82 82 (246);
  difference -1.2 percent (none measurable), under the 2 percent target.
- In use: the live bar showed "CPU 3%" on the idle desktop.
