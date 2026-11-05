# amarchy-flat hunk 0 + 0x325E: stale ColorMap ReleasePen (vtcon-c1 subagent, 2026-11-05)

Question: the stuck Workbench stack (rig141-stuck-taskdump.txt, line 296 on) holds 1041d52a, inside amarchy-flat hunk 0
(1041a2cc-1043d350). Which function, which lock, who holds it, and is it a lock-order inversion?

## Verdict

Not a lock-order inversion. It is a use-after-free in the RL21 icon-label antialias path of the Text patch:
`ReleasePen(icl_cm, pen)` on the cached ColorMap of a Workbench screen that has since been closed (IPrefs reopened
the WB screen for a prefs change). ReleasePen takes that ColorMap's PalExtra semaphore; the memory is freed/reused, so
the "semaphore" has a junk owner and the Workbench task queues on it for ever (SIGF_SINGLE, wait 00000010). Workbench
holds the WB screen LayerInfo (nest 1) and its window layer 101048f0 (nest 2) from the Text call it is inside, so IPrefs
then blocks on LayerInfo 10104150, and Prefs/Font behind it.

## Map (proof)

- Rebuilt amarchy-flat from main b5458e99 (no source diff vs 6430b154 in amarchy/ render/ handler/be32.c device/ tty/),
  same vc command as the Makefile, vlink `-M` map added: md5 a6923d113be3392a203451801a5ea0fd, byte-identical to the
  rig 141 binary.
- Dump hunk ranges are BPTR address .. +size+8 (hunk 1 __MERGED 0x3c -> 0x44, hunk 5 BSS 0x18e30 -> 0x18e38;
  allocations 8-aligned at range start - 4). Code starts at range start + 4 = 1041a2d0, so 1041d52a = code offset 0x325A.
- Code at 0x3250..0x325A: `movea.l _GfxBase,a6` (reloc 0x3252 -> hunk 5 BSS +8 = _GfxBase), `jsr -948(a6)` at 0x3256
  = graphics ReleasePen. 0x325A is its return address. The map labels match the -S output exactly:
  l1355 0x3210, l1408 0x3238, l1371 0x325a (return label), l1364 0x3276.
- Function: the Text patch label branch (inlined into the code after `_c8_disp_stk`, before `_flat_tx_c` 0x33d4),
  source amarchy/flat.c:811-815:
  ```c
  if (icl_cm && icl_cm != cm) {             /* another screen's colour map: the cached pens are not ours */
      for (i = 0; i < ICL_CACHE; i++)
          if (icl_pc.e[i].used && icl_pc.e[i].obtained)
              ReleasePen(icl_cm, (ULONG)icl_pc.e[i].pen);      /* <- 0x3256, Workbench hangs here */
  ```
  `icl_cm` (flat.c:646) is a raw ColorMap pointer cached across calls; nothing clears it when its screen closes.

## Lock and holder

- Lock waited on: `icl_cm->PalExtra->pe_Semaphore` of the OLD screen (ReleasePen obtains it).
- Holder: none alive. The current WB screen's palextra 101066d8 is free (dump line 490: owner 0, queue -1), so Workbench
  is not waiting on the live colour map. Workbench's SemaphoreRequest on its stack (sp+0x2e: succ 00000008,
  pred 10104990, waiter 10581120) points into memory now holding the new screen's layer structures (layer 101048f0,
  lock 10104938); taskdump's pred walk ends at the junk "semaphore" 0008804c, owner 0, nest 0, queue 0. Freed, reused
  memory: nobody will ever ReleaseSemaphore it. (Address overlap with the new layer is inferred from the pred value,
  not proven; the old icl_cm value was not dumped.)
- icl_ss (flat.c:640) is held by Workbench via AttemptSemaphore; it is not the blocker.

## Why it fits the symptoms

- 640x256x8 only: the pen path (`depth <= 8`) is the only one that caches pens and icl_cm; at 800x600x24 the rtg path
  never touches icl_cm.
- Needs a WB screen close/reopen after a label was drawn (IPrefs palette/font USE at Amarchy start) and then a new label
  draw by Workbench: icl_cm != cm triggers the release on the dead map.
- A start by hand a minute later works: icl_cm already matches the new screen or no reopen follows.

## Proposed fix (not committed; no amarchy/ edit made)

Invariant: amarchy-flat must never call graphics on a ColorMap whose screen may be closed. Root cause level:
amarchy/flat.c icl cache lifetime (flat program, not the start gate).

- flat.c:811-816 (and icl_free, flat.c:652-655, called at unload line 1925): before `ReleasePen(icl_cm, ...)`, check
  under Forbid() that icl_cm is still the `ViewPort.ColorMap` of a screen in `IntuitionBase->FirstScreen ... NextScreen`.
  Alive: release as now. Gone: drop the cache without ReleasePen (the pens died with the map). Do NOT use LockIBase
  here: the Text patch runs with the caller's layer locks held, and IBase -> LayerInfo is Intuition's lock order.
- Optionally also keep `icl_scr` (the Screen pointer) and require both scr and cm to match, to shrink the ABA window of
  a new screen getting the same ColorMap address.
- Put the liveness decision in a pure helper in amarchy/iclabel_core.c (`icl_cm_alive(cm, const void *const *maps, n)`
  or a release-plan function returning RELEASE/DROP), host-tested.

Regression tests:
1. Host (tests/test_iclabel.c): the helper returns DROP for a colour map not in the open-screen list and RELEASE for one
   that is; a cache built on map A, then a draw on map B with A gone, issues zero ReleasePen calls (counted stub).
2. Source guard (python, like test_flat_on_hang_wild_call.py): every `ReleasePen(icl_cm` in flat.c sits behind the
   liveness helper. Fails on current main, passes after.
3. Rig (one run): rig 141, --os 32, 640x256x8, fresh-all, Amarchy start with flat on (the gate run 1 recipe); PASS =
   no hang and Workbench answers GETATTR after the start; fails on a6923d11.

## Rig A/B

Not run. Step 1 identified the call, lock and cause from the byte-identical binary; the A/B (flat not started) would
only show that flat is involved, which the stack word already proves. The useful rig run is test 3 above with the fix.
No test-register row added.
