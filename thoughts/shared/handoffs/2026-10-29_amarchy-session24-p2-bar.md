---
date: 2026-10-29
session: Amarchy s24
topic: P2 status bar segments, W43 wait, R1.17 desk theme hang
status: in progress
---

# Amarchy session 24 handoff: P2 status bar, W43, R1.17

Plan: thoughts/shared/plans/2026-10-29-amarchy-p2-status-bar.md. Ledger: thoughts/spot/todos.md (s24 lines).

## Done (origin/main)

- R2.4a-c bar_core: centre group, screens cut with "+N", formatters without sprintf (8bccda94, e3e0dd3f, 5aace667).
- R2.1/R2.2 rig half: netprobe, idleprobe, research doc thoughts/shared/research/2026-10-29-amarchy-p2-probes.md (816bb673, 7eedbbef, b5162396). A1200 halves still open.
- R2.6 clock, R2.7 RAM, R2.11 screens list, R2.12a System entry, BAR TEXT verb (7f8d531d, 77589581).
- W43: Amarchy waits for the final Workbench mode before opening windows (d64bffe0). Rig 4: 3/3 boots reach RTG 800x600.
- R2.8 CPU (idle counter task at priority -128, 0.5 s calibration in bar_open) and R2.10 Network partial ("Network emulated" on UAE, "No network" on failure) (0432be4a, 6183d4e7).
- Bar overflow drops right segments by priority: Network, CPU, RAM, Agents. Clock, System and the first screen stay. BAR ON/OFF/TOGGLE during the W43 hold are remembered (bar_plan, g_bar_want) (8e381cbe).
- 640 px rig proof (14/14 steps), bar_hold rig step, revert proofs, very narrow bar clips the first screen, rig-helpers builds idleprobe and netprobe (3f34fb6d).

## Open, in order

1. R1.17 desk theme: all 5 rig FAILs come from one hang. desk_dos_apply (amarchy/desk_dos.c) runs Palette, WBPattern, Pointer, Font. SYS:Prefs/Pointer dies with "Software Failure" on RTG rig 4, so Font never runs and lf_gt_select (amarchy/leaf.c:346) never publishes AMARCHY_DESKS / AMARCHY_DESKMASK. The owner tests the Pointer editor by hand on the evening of 2026-10-29. Ask for the result. Do not pick a workaround before that. After the hang: re-check palette pen4[11]/pen8[11] and wbpattern ptrn1.depth expectations against the NDK 3.2 prefs headers. Evidence is in the session scratchpad desk-fix.md (may be gone).
2. R2.10 real network query: needs the official Roadshow SDK 1.4 .lha. The owner downloads it from https://www.amigafuture.de/app.php/dlext/details?df_id=3658 (curl gets 403). Unverified values from the AROS header: IFQ_BASE = TAG_USER+1900, IFQ_State = BASE+19, SM_Offline 0, SM_Online 1, SM_Down 2, SM_Up 3. Every ti_Data must be a pointer (LONG * for IFQ_State). Junk ti_Data caused the earlier UAE stack death. Licence: "Freely Distributable".
3. Bar layout: when the centre group plus the never-dropped right segments fill the bar, the left group still overlaps. Hiding the first screen broke test_bar checks at width 300.
4. P2 step 5: R2.13a/c (super+b), R2.5b, R2.12. Step 6: R2.16 per-segment revert proofs, R2.15 docs, R2.17 A1200 (with the A1200 halves of R2.1/R2.2).
5. W43 second PAL path: boot 3 in build/rig4/retry.log stayed PAL for 60 s with no requester. Unexplained.
6. p3_keys snap flake ("floats 0 -> 1", wrong box): cause unverified.
7. Rig 8 boot loop: likely stale worktree build/ state. RIG3-BOOT-GURU "00ff at 0001FFF8" unexplained.

## Gotchas

- `make test ONLY=bar` takes 5-10 min, because it rebuilds the host build.
- `rig.py aga` printed "Workbench stayed PAL (W43)" and "stopped" although the rig was up at 640 px.
- A fresh worktree tile rig needs `make rig-helpers`.
- The main checkout holds another session's uncommitted files. Work in detached worktrees only.
- Rig 9 belongs to another session.
