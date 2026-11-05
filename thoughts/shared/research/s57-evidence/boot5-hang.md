---
date: 2026-11-05
topic: Boot 5 hang hunt after the icl pen fix (92b29ff8), rig 142
verdict: NOT REPRODUCED, cause UNVERIFIED, no fix
---

# Boot 5 hang hunt on rig 142 (3.2, 060 max, --idle-minutes 0)

Build: main 53d89817 (contains 92b29ff8). make worktree-ready, make amiga, make dist with
UPTERM_PORTS = kit-main worktree, make wasabi build/amiga/taskdump. All rc 0.
Install: INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all: PASS (step 34 STATUS answers, step 35 menu opens).

## What ran

| Run | Emulator | Result |
|-----|----------|--------|
| try 1 | fs-uae-debug, --debugger | stuck at the Kickstart-entry stop (PC 00F800D2): the loop script read debugger.log from the worktree build/, but rigs live in the main checkout build/rig142. Script bug, not a product hang. |
| try 2 | fs-uae-debug, --debugger | boot came up (AMARCHY STATUS answered, ticks=20), but `rig.py reboot` was still in its post-start steps after 420 s ("LIBS: assigned before Amarchy" was the last line). The loop counted that as a hang and sent SIGUSR1, which stopped the emulator in the debugger; `dbg g` resumed it and wasabid answered again. Not a product hang. |
| Palette lead | same rig, live | `SYS:Prefs/Palette USE` once, `Palette FROM ENV:Sys/palette.prefs USE` 3 times, then `Run SYS:Prefs/Locale`: wasabid still answers, no Guru, 0 "CPU reset"/"Exception" lines in fs-uae.log.txt. |

The reboot loop under the release emulator never ran: the rig kept using the debug FS-UAE after the
first debug start (start.args / reboot args carry it; dropping UPTERM_FSUAE from the env did not change it).

## Findings

- Post-start on rig 142 is slow under the debug emulator: `rig.py reboot` took over 420 s while the Amiga
  was already up. The proof run's boot 5 died at the same post-start Assign step. Whether that was a real
  freeze or a very slow host step is still open (boot 5 had fs-uae at 79-100% CPU for 20 min, which points
  to a real spin, not a wait).
- Palette USE + Locale did NOT crash this rig. The gadget agent's "Exception 3 (10031f6a)" lead is not
  reproduced here. Differences: debug FS-UAE build, mode not confirmed as 640x256x8 (start warned
  "the Workbench stayed PAL (W43)"), Palette run from the CLI, not the editor's Use gadget.
- Unreviewed lock order: iconpen_dos.c calls ObtainPen/ReleasePen (PalExtra semaphore) while holding
  LockPubScreenList (lines 170-188, 283-292). Safe only if no task takes PalExtra and then the pub screen
  list. Not verified either way.

## Next

1. Start rig with the release emulator explicitly (check `ps` shows Emulators/fs-uae/fs-uae, not fs-uae-debug).
2. Confirm 640x256x8 on the WB before the loop.
3. Loop timeout 900 s per boot, and treat a hang as "ping fails for 120 s", not "reboot slow".
4. Palette lead: click the Palette editor's Use gadget (not the CLI USE) at 640x256x8, then start Locale.
