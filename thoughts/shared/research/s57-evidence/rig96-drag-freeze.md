---
date: 2026-11-05
topic: rig 96 "freeze" after dragging a VTC:vsh XCON window into the screen title bar
status: UNPROVEN (mechanism found, writer not identified; vtcon-handler direct-plane lead dead 2026-11-05)
rig: 96 (AmigaOS 3.2, main 7cf4ac17, flat mode, Amarchy + amarchy-flat at boot), started from ../vtcon-s57-rig96b
---

# Rig 96 drag freeze

## Verdict

Not a deadlock and not a hang. The system is alive. The display is blank because the
live copper list was overwritten with bitmap pixel data. Something drew rows at or
below y = 256, past the bottom of the 640x256 Workbench screen bitmap, straight into
chip RAM. The copper list sits 0x168 bytes after the end of that bitmap. The writer
is not identified (UNPROVEN).

## Evidence (read-only, no input except mouse moves; no Ctrl-C or Break tests run)

Tools: RAM:taskdump, RAM:vpdump, RAM:copdump (sources in the session scratchpad),
run over wasabi port 1329.

- Tasks: every task waits normally. input.device sw=C0000000. XCON, Amarchy,
  amarchy-flat and SolidWin (sw=00001000, Ctrl-C) are all in Wait. The only busy task
  is the Amarchy bar idle task (pri -128 busy counter, amarchy/bar.c:89-123). That task
  is why fs-uae shows 99% CPU. It is by design, not the fault.
- Semaphores: all free (own=-): IBase private semaphores IBase+822..+1708,
  LayerInfo.Lock, layers 0-3, ActiViewCpr.
- Intuition: alive. The mouse pointer moves through wasabi.
- Display: screen 'Workbench Screen' 640x256, mode id 00029000 (PAL hires AGA),
  depth 8, interleaved, bm Planes[0]=00003D40, pen0=1A1B26. ActiView=IBase.ViewLord,
  LOFCprList=10019D20, GfxBase->LOFlist=0002BEA8.
- Bitmap end: 0x3D40 + 256 rows * 640 bytes (80 bytes * 8 planes) = 0x2BD40.
  The copper list at 0x2BEA8 is 0x168 bytes past it.
- Copper dump: LOFlist starts with FFFF... and then stripes that repeat every 80 bytes
  (for example `EC66 01C18001 87EC67C0 060603C1 8000`). This is per-plane pixel data at
  the same column, which is the interleaved layout. The intact colour MOVEs
  (0180..01BE) come only later. The copper runs garbage and the display shows colour 0.
- Host capture of the emulator window: solid RGB 26,27,37 (= pen0). The wasabi bitmap
  grab still shows the full screen content: the bitmap is intact, the display is not.
  Shots: ~/Desktop/upterm-shots/rig96-drag-freeze-grab.png and
  ~/Desktop/upterm-shots/rig96-drag-freeze-emu-window.png.
- Geometry: the dragged window bottom sits exactly at row 256 (top 7, height 249).

## Candidates for the writer

1. Ruled out: amarchy/flat_core.c flat_gm_recolour (the flat.c ghost_menu direct-plane
   path for layerless RastPorts). It clamps x1 to width-1 and y1 to rows-1
   (flat_core.c:506-511), so it cannot write past the bitmap. No other direct
   Planes[] writes exist in amarchy/*.c.
2. Best candidate: a draw with no layer clipping during the drag, so a rectangle that
   ends at y >= 256 hits raw memory. Suspects: SolidWin (solid window drag, the only
   non-system drag drawer running; see tests/amiga/rawdrag.c), or an Amarchy/flat
   hook that draws into the screen RastPort (rp->Layer == NULL) with blitter calls
   that graphics does not clip on a layerless RastPort.
3. Second candidate: 3.2 Intuition/graphics behaviour when a window moves so its top
   is in the title bar area (top 7) and the frame redraw passes a rectangle one row
   past the screen bottom to a layerless call.

## Next step to prove it

Repro on a new rig (125 or above) with a fresh full install. Drag a VTC:vsh window to
top 7 with SolidWin stopped, then with amarchy-flat stopped. After each drag, run
copdump and check that LOFlist still starts with colour MOVEs. The run where the list
stays intact names the writer. A memory watchpoint on 0x2BD40-0x2BFFF (UAE debugger)
gives the writer's PC directly.

## Rig 96 state

Left up, not rebooted, nothing stopped. Its copper list is corrupt: the display stays
blank until a reboot. RAM:taskdump, RAM:vpdump and RAM:copdump remain on it (lost on
reboot).

## Hunt attempt 2026-11-05 (rig 125): writer UNPROVEN

- Rig 125 (3.2, 060 max) fresh-all install reached its last page, then `rig.py reboot` timed out:
  the wasabi daemon took the connection but did not answer for 15 s. Host CPU was about 610% at the time,
  so host starvation is not ruled out. No drag, no copper check, no bisect was run.
- Probe ready for the next run: a copcheck program (opens the Workbench pubscreen, computes the
  bitmap end with interleave detection, prints the first 16 LOFlist words, flags a MOVE whose register
  word is even and >= 0x200, exit 5 = CORRUPT). Source to re-add under tests/amiga when the run happens.
- Static review: flat_rf_c always ends in the layered or saved RectFill; ghost_menu recolour is clamped.
  Strongest unverified code suspect: vtcon-handler direct-plane writes in render/amiga_render.c
  (back_copy and target()) place rows from w->TopEdge/LeftEdge and check only the cell area against the
  single ClipRect, not each copy rect's rows. During a drag the window position can differ from the
  layer bounds. Invariant to test: every byte written lies inside the locked ClipRect bounds, with the
  position taken from the layer, not the Window.
- Next run: quiet host first, rig start, fresh-all, copcheck before and after the drag to top 7, then
  stop SolidWin / amarchy-flat / Amarchy in turn; memwatch with the debug FS-UAE (--no-jit --debugger).

## Host test 2026-11-05: vtcon-handler direct-plane lead DEAD

Test: `direct_plane_writes_stay_inside_the_bitmap_at_its_bottom_edge` in tests/test_render.c (run by
`make test ONLY=render`). gf_planar now allocates 4 guard rows (0xA5) after every plane of the fake
interleaved bitmap, and `gf_guard_hits()` counts changed guard bytes. Each case uses a 640-wide window
on an 8-plane screen, scrolls rows+4 coloured lines and runs synchronized-output frames that clear and
redraw the last row. It then asserts 0 guard hits and that back_copy ran (`rn.n_copies` grew).

Six cases pass with 0 guard hits:
- top 7, height 249 (the rig 96 geometry: bottom = row 256 = bitmap end)
- top 0, height 256
- top 7, height 249, with borders of 2
- top 7, height 249, with the Window TopEdge one row below the layer (stale geometry during a drag)
- the two bottom-edge geometries with a "race" hook: TopEdge moves down 1 row at a WaitBlit inside
  back_copy's locked section (Intuition's input task running while the handler holds the lock)

The test can fail. A mutation that writes back_copy rows at `TopEdge + y + 1` gives 560 guard hits
per case (32 failures).

Why the lead is dead:
- Every direct-plane path (planar text, direct cells, planar_begin plus back_copy) checks
  screen_planes_ok and writes under the same LockLayer, with the same w->TopEdge read.
- Rows are clipped to vis_rows, and the check requires the cell area to lie inside the single,
  unobscured ClipRect. The ClipRect lies inside the screen, so no row at y >= 256 passes the check.
- A stale TopEdge makes the check fail, so the handler falls back to the layered calls. Graphics clips
  those calls.
- Remaining caveat (unproven either way): a downward TopEdge change between the check and the write,
  inside the locked section. The race hook covered only the first WaitBlits of each pass. Intuition
  moves a window only after it locks the layers, so the handler's lock blocks this case on a real system.
- Side note (not this bug): planar_begin reads LeftEdge for the phase after its unlock. A LeftEdge
  change between planar_begin and back_copy can shift the copy by at most 4 bytes horizontally.
  This can not reach row 256.

Next candidates, in order:
1. SolidWin: a solid window drag that draws the window image into the screen RastPort with no layer
   (see tests/amiga/rawdrag.c). Graphics does not clip a rectangle that ends at y = 256 there.
2. A 3.2 Intuition frame redraw for a window whose top sits in the title bar area (top 7).

Prove on a rig: copcheck before and after the drag to top 7, with SolidWin stopped, then with it
running. A memory watchpoint on 0x2BD40-0x2BFFF gives the writer's PC.

## Hunt attempt 3, 2026-11-05 (rig 131): writer UNPROVEN, no drag run

- Build: worktree drag-copper-hunt at origin/main fdb5e99f (make worktree-ready, wasabi 959c33bd, amiga, dist via kit-main).
  `installer_rig.py ONLY fresh-all` with INSTALLER_KEEP=1: checks 1-33 ok, then FAIL 34 (AMARCHY port: no port) and
  [ABORT]: after the post-install reboot wasabid took the connection but did not answer for 15 s. This is the same stop
  as rig 125. Host at the time: rigs 96, 127, 129 plus a node pre-push hook, each about 85-100% CPU.
- `rig.py reboot --max --os 32 --no-jit --debugger` with UPTERM_FSUAE=fs-uae-debug: the console debugger stops at the
  Kickstart entry (PC 00F800D2) on start, so `rig.py reboot` times out until `rig.py dbg "g"` is sent. After "g" the
  machine booted fully (Amarchy, amarchy-flat, amarchy-menu, SolidWin, Workbench up).
- copcheck (now tests/amiga/copcheck.c, CONPROBES) on the clean boot, before any drag:
  `pl0=00003d40 pl1=00003d90 bpr=640 rows=256 depth=8 end=0002bd40 lof=0002cee0 off=4512 verdict=OK`.
  Same bitmap address and end as rig 96, but LOFlist sits 0x11A0 past the end here (rig 96: 0x168), so where the copper
  list lands depends on allocation order. A copcheck OK says nothing unless off is small; watch the bitmap end, not LOFlist.
- Memwatch armed `w 0 2BD40 50 W CPUDW BLTD` (plane 0 of row 256, the first bytes any row-256 writer hits; CPU data
  writes and blitter D). After the second dbgbreak + w + g the guest never answered again (wasabi: no answer, later
  connect timeout; fs-uae 110% CPU; debugger.log ends at the `>` prompt with no memwatch hit line). Unknown whether "g"
  was not taken from the FIFO or the no-JIT guest was starved. No drag, no SolidWin split was run.
- Next run: start the debug build with the memwatch already in place before "g" at the boot stop (one FIFO round, no
  second dbgbreak), open `NewShell WINDOW=XCON:0/7/640/249/DragTest`, then RAM:rawdrag on its title bar (dy negative)
  with SolidWin running, then with SolidWin stopped. A hit line names the PC (CPU) or the blit (BLTD).

## Hunt attempt 4, 2026-11-05 (rig 137)

Verdict: INCONCLUSIVE. No drag ran. The guest froze right after the DragTest shell opened, so the writer is still unknown.

- Build: origin/main 6b2aeb09 (wasabi made, dist via kit-main). fresh-all with INSTALLER_KEEP=1: checks 1-33 OK, step 34
  fails (AMARCHY port: no port), as expected.
- Debug reboot: fs-uae-debug `--max --os 32 --no-jit --debugger --idle-minutes 0`, purpose "vtcon-8c: drag freeze
  memwatch hunt". At the Kickstart stop (PC 00F800D2) two watches were armed in one FIFO round, then g (no second
  dbgbreak): `w 0 2bd40 80 W CPUDW` and `w 1 2bd40 80 W BLTD` (the mask is only parsed as the last token, so CPU and
  blitter need separate watches). Every hit breaks; scratchpad autog.py logged each hit and sent g.
- Boot completed. copcheck: `pl0=00003d40 bpr=640 rows=256 depth=8 end=0002bd40 lof=0002cee0 off=4512 verdict=OK`,
  windows `[- 0,11 640x245][- 0,10 640x246]`. The watched range starts at the bitmap end.
- Boot baseline (204 hits, all before the shell opened, last at 07:55:40; RAM below the bitmap end is free during boot):
  CPU 400B484A x68, 400B4900 x8, 00F8200C x20, 00F8200E x20, 00F81F24 x2, 00F81EE6/00F81EEE (exec clear);
  blitter D 00F86AA6 x34, 00F86AA8 x13, 00F86AE2 x7, 00F895D6 x10, 00F895DC x7. These are boot-time users of that
  memory, not the drag writer.
- Process list after boot: SYS:Prefs/Pointer still running (the S57 start hang from vtcon-c1), plus Amarchy,
  amarchy-flat, amarchy-gui, SolidWin, amarchy-menu, Workbench.
- `Run >NIL: NewShell WINDOW=XCON:0/7/640/249/DragTest` returned rc 0. The next exec (RAM:copcheck) never returned
  and wasabid stayed silent ("daemon did not answer for 150 s"), also after a dbgbreak + g. The debugger showed the CPU
  idle in STOP at PC 40061A8E (S=1, STP=1, IMASK=0): no task ready, all waiting. No memwatch hit in this phase, so
  the freeze is not a write past the bitmap end; it looks like a lock/wait deadlock, possibly tied to the still-running
  Prefs/Pointer start hang. The `T` (task list) command gave no output.
- Next run: first clear the Prefs/Pointer start hang (or boot with Pointer prefs not run) so the shell can open
  without the deadlock, then repeat the same watch and drag recipe. Alternative: open the DragTest shell before the
  debug reboot is idle (from S:User-Startup) to see whether the deadlock is the shell open itself.
