---
date: 2026-11-03
topic: S32-STALE-GADGETS - why a 3.2 full install shows block glyphs, not cloned sysiclass gadgets
status: partial (cause 1 proven + already fixed on main; cause 2 located, not yet proven or fixed)
---

# Gadget clone hunt (rig 60, OS 3.2, worktree agent-ac56cf77b7ce8adf3 at 051add94)

## Cause 1 (proven, already fixed on main by 31ce61a5)

The rig 96 evidence (`flat=on calls=0`, `rectfill/text installed=no`, `clone draws=0`) came from main
952d2457, which is an ANCESTOR of 31ce61a5 (2026-11-03 01:48, "Apply the Amarchy start look once LoadWB
opens the Workbench screen"). Before 31ce61a5, `amarchy-flat BOOT` runs from S:User-Startup before
C:LoadWB; `class_slot()` (amarchy/flat.c, `ipd_lock_wb_screen` never opens a closed Workbench) finds no
screen, `hook_install` returns FLAT_ERR_NOCLASS and the install was never retried. No sysiclass hook =>
c2_draw never runs => calls=0, clone draws=0.

31ce61a5 retries from the 1 s screen watch (`flat_install_due`, amarchy/flat_core.c:307; call site
amarchy/flat.c ~line 1303). Host regression test already exists and was written to fail on the old
behaviour: tests/test_flat_core.c:1415 `test_boot_before_workbench_retries_install`.
So the "stale gadgets" on rig 96 is that rig running a pre-31ce61a5 build, not the resident-lag race.

Measured on rig 60 (OS 3.2, `installer_rig.py ONLY fresh-all`, INSTALLER_KEEP=1, every box ticked,
PASS, reboot), `UP-Term:bin/amarchy-flat STATUS` from the HEAD build:

    text installed=yes calls=988 hits=0
    clone draws=41 fallback=36
    frame=hooked calls=0 flat=0

The hook is in and the clone path runs (draws 41 > 0). Cause 1 is gone on main.

## Cause 2 (located, NOT proven): fallback=36

36 of 77 window-gadget draws still took the block-glyph fallback, so some gadgets on screen are still
the old block glyphs. `cfall++` happens only when flat_clone_draw returns FLAT_CLONE_FAIL
(amarchy/flat.c c2_draw, after flat_clone_route). The paths that give FAIL:

1. `AttemptSemaphore(&cl_ss)` in c2_draw fails: another task is inside the clone (Intuition draws
   window frames from several tasks - input.device, Workbench, Amarchy - and clone_render holds cl_ss
   across an IM_DRAW plus w*h ReadPixel calls, long enough to collide at boot when many windows open).
2. `clone_render` returns 0: `imp_DrInfo == NULL`, or `clone_ensure` fails (gfx < 39 / AllocBitMap).

Leading hypothesis: (1), contention on cl_ss. The fix at that level would be ObtainSemaphore instead
of AttemptSemaphore (the holder draws only into a layerless offscreen RastPort plus the caller's own,
already locked, real RastPort, so no lock-order inversion), or a per-call bitmap. NOT applied: the
cause is not yet proven and no commit was made.

Next step (one small agent): split `cfall` into three counters (busy / no DrawInfo / no bitmap) in
flatshared + STATUS, install on a 3.2 rig, read which one rises; then fix that path with a host test
in tests/test_flat_core.c and pixel proof of close/depth/zoom.

## Not done
- No screenshot / pixel measurement taken (budget reached).
- No commit. Rig 60 stopped and cleared.
- Rig 96 must be reinstalled from a main that contains 31ce61a5 (and the cause-2 fix once made).
