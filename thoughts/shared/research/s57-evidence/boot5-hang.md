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

## Run 3: lock-order (static, 2026-11-05, vtcon-c1 agent, main 800ea832)

Verdict: **no reverse-order pair found in amarchy code.** No fix written. No rig run.

Method: every LockPubScreenList / LockIBase / LockLayerInfo / LockLayer(s) / ObtainSemaphore acquire in amarchy/*.c,
with the calls made before its release (scan script, textual, per function), plus every ipd_* caller, every
ObtainPen / ObtainBestPen / ReleasePen. Intuition's documented rules from NDK 3.2R4 Autodocs: LockIBase (BUGS) must
not be called while holding layer or LayerInfo locks, and no Intuition / graphics / layers / DOS call while held;
LockPubScreenList: "the restrictions on LockIBase() apply here as well"; LockLayerRom: no Intuition call while held.
So Intuition's order is layers before IBase / pub list.

### Lock-order table (holder -> what it takes while holding)

| Who (task) | Holds | Then takes | Where |
|---|---|---|---|
| amarchy main / amarchy-flat / amarchy-icons (own process, no lock held by caller) | ipd record sem (r->ss) | DOS reads (pens_theme), then pub screen list, then PalExtra (ObtainPen, ReleasePen, is_free ObtainPen) + SetRGB32 | iconpen_dos.c:220/258 -> 170-188; amarchy.c:228-238, flat.c:1569, icons.c:493/507 |
| gthemes (theme apply) | r->ss | DOS reads, then pub list, then SetRGB32 / ReleasePen | iconpen_dos.c:274-292 via gthemes_dos.c:232 |
| amarchy-icons | its run sem | r->ss (above); then WB visitor lock -> LockDosList(READ) / Lock() | icons.c:424-511, 352-378 |
| any OpenWindow caller (strut hook) | pub list | LockPubScreen (same sem, nests) | strut_patch.c:122 -> iconpen_dos.c:102-107 |
| any drawing task (Text hook, maybe under the caller's layer / Intuition locks) | icl_ss (AttemptSemaphore, never blocks) | PalExtra (ObtainBestPen / ReleasePen) on the window's own live ColorMap | flat.c:705-724, 775-825 |
| flat, desk, bar, wbmode, gthemes | WB visitor lock (LockPubScreen) | PalExtra (ObtainBestPen) | flat.c:1409/1718, desk_dos.c, gthemes_dos.c |
| tile / win / strut / borderblank / flat / desk / shot | LockIBase | leaf (list walks) | see scan |
| win.c win_exec (hotkey / port WIN verb) | LockIBase | WindowToFront/Back, ZipWindow, ChangeWindowBox, **amconf_load (DOS Open/Read)** | win.c:253-319 (amconf_load at 273) |
| wbreset close_hook (CloseWorkBench caller) | nothing | waits for client signals, bounded (WBR_WAIT_TICKS) | wbreset_patch.c:55-70 |

### Why no cycle closes

- PalExtra is held only inside graphics pen calls, which call no Intuition and no amarchy code, so no PalExtra holder
  can wait on the pub list, IBase or r->ss. The pub-list -> PalExtra edge in pens_sync is a documented-rule breach
  (graphics calls under the list lock) but has no reverse taker.
- r->ss is taken only from amarchy's own processes at top level (amarchy.c, flat.c:1569, icons.c, gthemes_dos.c),
  never from a patch running in another task's context, so Intuition / input.device never waits on it.
- No amarchy code takes the pub list or IBase while holding a layer lock (bar.c:563 LockLayerInfo is a leaf).
- The only stale-ColorMap release path (the earlier s57 root cause, dead PalExtra) is fixed: icl drains per call
  (flat.c:817-822); pen_release in pens_sync releases only on the screen the pens are held on, under the list lock.

### Real rule breaches found (not shown to cause this hang)

1. win.c:253-319: DOS read (amconf_load, L273, WV_SNAP) and Intuition calls under LockIBase. A DOS requester or a
   handler that needs Intuition there freezes all input. Reached only by a WIN hotkey / port verb, not at boot.
2. iconpen_dos.c:170-188, 283-292: ObtainPen / ReleasePen / SetRGB32 under LockPubScreenList (documented: no graphics
   call under it). No reverse taker, so not a deadlock by itself; it does stall every LockPubScreen / OpenWindow on the
   system for the length of the sync.

### Step B not run

Step B's method needs the lock owners at the hang. The release FS-UAE has no SIGUSR1 debugger (rig.py dbgbreak needs a
debug build), wasabid is silent at the hang so taskdump cannot run, and the debug FS-UAE did not reproduce the hang on
rig 142. A release-emulator repro would only re-confirm the 2-in-9 rate already measured on rig 96. Needed first: a
release-speed FS-UAE build with the SIGUSR1 debugger handler (or a wasabid-independent dump such as an emulator-side
memory dump of ExecBase task lists), then the 10-boot loop.
