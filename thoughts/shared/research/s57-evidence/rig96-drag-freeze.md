---
date: 2026-11-05
topic: rig 96 "freeze" after dragging a VTC:vsh XCON window into the screen title bar
status: UNPROVEN (mechanism found, writer not identified)
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
