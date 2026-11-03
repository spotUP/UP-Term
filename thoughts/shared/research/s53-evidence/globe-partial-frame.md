---
date: 2026-11-03
topic: UPDEMO-GLOBE-PARTIAL-FRAME root-cause hunt
status: no data defect found; owner run not reproduced on rig
---

# UPDEMO-GLOBE-PARTIAL-FRAME

Owner report: rig 91, `VTCX:UPDemo 15`, coded-globe kitty animation, "one of the frames was not drawn completely".

## Verdict

No frame-data or blit defect found on host or rig. Most likely cause: tearing (a frame
seen mid-draw; kit_anim_show copies the frame into the placement and marks rows, with no
double buffer), or the moment during the initial a=T/a=f upload before all 8 frames exist.
Not proven: the owner's exact command was not reproduced on the rig (see below).

## Host measurements (engine/vtengine.c + demo/updemo.c harness)

- Coded globe (demo/updemo.c sc_kitty_anim ~1924, globe_frame ~1855; GLOBE 48, 8 frames, 120 ms gap):
  all 8 composed frames have 0 unset pixels, RGB and indexed mode.
- Same with the stream fed whole, in 7-byte and in 13-byte split writes (chunk boundary test of
  the a=f path, engine/kitty.c ~1337, kitty_run only after the last m=0 chunk).
- After every vt_anim_tick switch, the shown placement equals its frame: 0 px off.
- PNG path (earth.anim): 44 frames whole, every switch 0 px off.
- Frames 5-8 equal frames 1-4: the checker repeats every 4 phases. By design, not a defect.

## Rig 91 measurements (24-bit RTG WB 800x600)

- Coded-globe stream (74767 bytes, cut at s=3 so the exit clear does not remove it) typed into
  an 800x320 XCON window; globe at screen 44,82.
- 40 grabs ~81 ms apart: every shown frame matched one of the 4 expected phases, 0 of 2304 px
  mismatched. Transitions at grabs 9, 21, 37 each changed 894-895 px. No partial or mixed frame caught.
- Rig 91 lacks UP-Term:share/updemo/earth.anim (start [WARN]), so UPDemo takes the coded path there too.

## Not reproduced

- `VTCX:UPDemo 15` typed in the XCON window drew nothing (640x240 and 800x320 windows); no UPDemo
  process seen. Through wasabi run it exits 20 with "UPDemo: start it in a console window".
  Cause of the typed run drawing nothing: unknown, not hunted (budget).

## Side finding (unverified)

- Frame switches on the rig came every ~0.65-1.0 s (12-16 grabs) instead of 120 ms. Possibly the
  screen grabs stall the emulator; not measured without grabs. Worth a separate row if the owner
  sees a slow globe.
