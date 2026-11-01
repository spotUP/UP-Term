---
date: 2026-10-31
session: vtcon-05 s39
status: handoff
---

# vtcon-05 session 39: screennotify parking, reset-loop triggers, nvim watch retarget

## Done (all pushed, origin/main = acd33353)
- acd33353 WB-RESET-SCREENNOTIFY UP-Term half: the handler registers with screennotify.library when present (LVO -54 AddWorkbenchClient / -60 RemWorkbenchClient), closes the console window on the Workbench-close message and reopens it on Workbench-open (handler/wbpark.h decision, tests/test_wbpark.c). Every rig stages the library by default (rig.py stage_screennotify, pinned Aminet archive; the library is (c) Stefan Becker and never committed or shipped).
- Rig 2 (3.1, 060, RTG) result: an IPrefs ScreenMode reset still shows the System Request. On 3.1 IPrefs does not go through screennotify's CloseWorkBench patch: the library's own screennotifytest client blocks the same reset. With UP-Term closed the reset succeeds; DOpus windows do not block it.
- bb9a17aa RIG31-RESET-LOOP triggers: ScreenMode Use, reboot cycle and own-screen close give no loop; a plain `crashme illegal` loops the 3.1 060 rig.
- a3c72c77: `crashme illegal` on a 3.1 68020 rig gives 2 resets and no loop, so the loop needs the 060 configuration.
- 69e5a447 NVIM-HEAP-CORRUPT watch retarget: watch never hit; slot address is per boot (BSS base + 4 + 0x1fb88); FS-UAE exited after setting the watch.

## Open, in order
1. WB-RESET-SCREENNOTIFY: find how IPrefs 3.1 resets the Workbench without the patched vector. Compare with Amarchy WBRESET-EXT (d3a5346c, amarchy/wbreset_core.c), which patches CloseWorkBench itself and sees Palette USE: ScreenMode vs Palette path, and patch order (screennotify README: start clients after IPrefs). Then retest on a 3.2 rig (`--os 32`). Gaps: tabbed consoles not parked; DISK_INFO returns no window while parked.
2. RIG31-RESET-LOOP: add a rig.py flag for a 060 rig without 68060.library, rerun `crashme illegal`.
3. NVIM-HEAP-CORRUPT: start the rig as a background task, poll debugger.log from a second process; if FS-UAE dies on the first hit, make fs-uae-debug memwatch output unbuffered.
4. Backlog for the second slot: RIG-BATCH (upkg 0053), NVIM-PERF, RIG-RECHECK-STALE.

## Unverified
- `rig.py rtg` left the boot on PAL; ScreenMode Use after reboot gave depth 16 not 24.
- UP-Term `exit` may not close its window on 3.1.
- build/wasabi vs tools/nas/wasabi.rev 83acd194 pin mismatch.

## Rigs
None of this session's rigs run (2, 3, 4, 6 stopped and deleted). Rigs 8 (vtcon-32) and 9 (unknown owner) were running: leave them. Never rig 1.
