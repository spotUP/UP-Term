---
date: 2026-11-06
session: vtcon-c1 (opus subagent, hunt 3)
build: te9-1 eec3bc56, worktree vtcon-te10-8, build/amiga/Amarchy (6 Nov 16:13)
verdict: UNPROVEN (both offsets mapped; faulting task and the exact race window not measured)
---

# Amarchy restart crash, hunt 3

## 1. Binary check
build/amiga/Amarchy hunk sizes: 0x55788 (code), 0x3c, 0x174, 0x8, 0x10, 0xa0e28 (BSS).
Hunk 0 size matches hunt 2 (0x55788). The offsets from hunt 2 apply to this binary.

## 2. Link map (probe link, scratchpad only, Makefile untouched)
Probe: build/vbcc-aos68k.cfg copied, `-x` on the -ld line replaced by `-M<scratch>/Amarchy.map`,
same source list as Makefile:970. Hunk sizes of the probe are identical, and the hunk 0 bytes are
byte-identical to build/amiga/Amarchy (cmp passed). The map offsets therefore transfer exactly.

| Offset | Object | Symbol | Kind |
|---|---|---|---|
| 0x28116 | prefs/settings_def.c (object 0x270e0-0x2aec0, rel 0x1036) | inside const table `_sd_settings` (0x279e0; next symbol `_sd_nsettings` 0x28c94) | DATA in the code hunk. The PC executed table data, so control arrived through a garbage function pointer. |
| 0x5392E | amarchy/strut_patch.c (object 0x52fdc-0x54688, rel 0x952) | static `tags_hook` (just before `_strut_patch_install` 0x53a9c) | CODE: `jsr (a2)`, with a2 = return of `ipatch_saved(&g_pt)` (`pea 0xa0dbc` = &g_pt in BSS; `bsr _ipatch_saved`). Source: amarchy/strut_patch.c:237 `w = ((openfn)ipatch_saved(&g_pt))(nw, mine ? mine : tags, ib);` |

The crash at 0x5392E is the strut OpenWindowTagList hook calling through a saved
pointer that is not the real OpenWindowTagList. ipatch_free() sets g_pt.gate = 0, so after
a free `ipatch_saved(&g_pt)` reads the long at address 18 (exec vector area): a garbage, odd
target, which gives Exception 3.

## 3. Exit path (amarchy/amarchy.c:680-712, on CTRL_C)

| Registration at start | Undone by | Case where the undo is skipped or unsafe |
|---|---|---|
| strut gates OpenWindow -204 / OpenWindowTagList -606, semaphore, g_pub (strut_patch.c:279) | strut_patch_remove strut_patch.c:321-346 | (1) busy still set after 300 x Delay(2): early return, gates + g_pub leak, but the code segment unloads. A caller blocked in the real OpenWindowTagList returns into freed code. (2) Race: tags_hook reads `p = g_pub` at :220 and does busy++ only at :226-228 under Forbid; a task preempted after the gate jumped into tags_hook but before busy++ is invisible to the busy wait. Remove then frees the g_pt gate and g_pub and the segment unloads. The task resumes, and `ipatch_saved(&g_pt)` at :237 reads a NULL gate (static, re-read late): the jsr at 0x5392E goes to garbage. |
| wbreset gates CloseWorkBench -78 / OpenWorkBench -210, semaphore (wbreset_patch.c:106-145) | wbr_patch_remove wbreset_patch.c:148-168 | Same busy early return (:164) with the code unloaded; close_hook can sit WBR_WAIT_TICKS inside its own code. Same pre-busy++ window at :61/:93. |
| borderblank OpenScreen / OpenScreenTagList gates | borderblank_remove borderblank_patch.c:~240-256 | Same pattern (:107/:121 busy++, :253 early return). |
| bar idle task "Amarchy bar idle" (bar.c:~108) | idle_stop bar.c:~124-137 via bar_close | No gap: CTRL_C + wait for done; failure path also calls idle_stop. |
| commodity broker, CxCustom objects (cx.c) | cx_close cx.c:141 | No gap. |
| clipboard change hook (clipwatch.c) | cw_stop | No gap (CBD_CHANGEHOOK removed via DoIO before CloseDevice). |
| timers, notify, ports, ARexx host, launch port | amarchy.c:690-712 | No gap found (AbortIO/WaitIO, RemPort before DeleteMsgPort, launches_out wait). |

Only the three ipatch users (strut, wbreset, borderblank) leave anything that can point into
the program after exit. They share one shape: the hook trusts statics (g_pub, g_pt) of a segment
that may be gone, and the busy counter does not cover the span from gate jump to busy++.

## 4. Runs
None in hunt 3. Rig 174 was not started: the tool-call budget (about 40) was spent on the map and
the exit-path read. Hunt 2 runs (test-register rows around 167) stay the measured base:
Break + restart crashed 2 of 4, fresh first start 0 of 3.

## 5. Verdict
UNPROVEN. Proven: the code at both offsets (above). Strongly indicated: the second crash is the
strut OpenWindowTagList hook running after its own instance freed g_pt (strut_patch.c:237 via
the :220-228 window, or the :343 busy leak with the code unloaded). Not measured: the faulting
task (most likely a window-opening task during the restart, for example Workbench or the rig's
setup tool), and the value of g_pt.gate / gate saved at the fault.

## 6. Fix shape (not applied)
Make each hook independent of the segment and its statics once it is removed:
- Put the hook body's pass-through decision in the AllocMem'd gate (already done: hook 0 means
  pass-through), and have the C hook read the saved target from the gate memory it was entered
  through, never from static g_pt; never free a gate that a task may still be inside.
- Count busy in the gate itself (gate code does addq #1 before the jump into C and the C hook
  decrements on exit), so the window between gate jump and C busy++ is closed.
- On a busy timeout do not let the segment unload under a live hook: keep the gate + g_pub and
  stay resident (Wait for busy == 0) instead of returning to exit.

## 7. Host regression test shape (fails today)
Fake ipatch + fake task layer, host C suite (ONLY=strut_patch style):
1. Install strut, enter tags_hook from a fake task and suspend it after the gate jump but before
   busy++ (seam on Forbid). Run strut_patch_remove. Resume the task. Assert: no read of a freed or
   NULL gate, and the call reaches the real OpenWindowTagList.
2. busy > 0 for longer than the wait: assert remove does not report "safe to unload" while any
   gate hook or saved slot, or any task PC, lies in the program code range.
3. Same two cases for wbreset close_hook/open_hook and borderblank.

## 8. Scratch
Probe files in scratchpad freeze-hunt3/ (hunks.py, probe.cfg, link.log, Amarchy.map 572 KB kept as
evidence); probe binary and raw hunk dump deleted.
