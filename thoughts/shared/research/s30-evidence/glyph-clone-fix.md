---
date: 2026-11-02
topic: OS 3.2 glyph clone (FLAT-GLYPH-OS-SHAPES) does not reproduce as broken on a fresh rig
status: no code fix; failure not reproduced, cause of earlier fallback=46 unproven
---

# Glyph clone on OS 3.2: measured, not reproduced

## Setup
- Rig 55, fs-uae, `--os 32`, intuition.library 47.53, Workbench screen (Amarchy desktop not running,
  no resident amarchy-flat at start).
- Build: origin/main 8aaaaecf plus diagnostic counters only (glyph-clone-diag.patch, this folder).
  The diag build's pass 0 is the unchanged main code path: direct call of the saved sysiclass
  dispatcher with a copied impDraw (marker DrawInfo, imp_RPort = offscreen cl_rp). Pass 1
  (DrawImageState with a per-task pass-through) only runs when pass 0 reads an all-zero grid.
- Prefs: flat-system-look = on, flat-window-gadgets = on. amarchy-flat started from RAM: with BOOT,
  then `NewShell "CON:40/40/420/160/glyph clone/CLOSE"`.

## Counters (STATUS)
- After BOOT: `clone draws=0 fallback=0 nest=0 empty=0 nzdirect=0 nzdis=0 rc=0`
- After the window opened: `clone draws=13 fallback=0 nest=0 empty=0 nzdirect=180 nzdis=0 rc=1`
  - draws=13, fallback=0: every clone succeeded.
  - nzdirect=180: the direct saved-dispatcher call put 180 nonzero pixels in the offscreen grid;
    pass 1 never ran (nzdis=0), no re-entry into our hook (nest=0), no empty grid (empty=0).

## Pixels
Shot ~/Desktop/upterm-shots/s30-glyph-clone-32.png (640x256). Close gadget zone x 40-57, y 40-49:
box in one pen, a 5x5 square of the glyph pen at x 47-51, y 43-47 (25 px), separator at x 58-59.
That is the OS 3.2 close-gadget shape, not a solid block.

## Conclusion
On a clean 3.2 rig the main code path clones correctly. The 3.2 failure recorded earlier
(`draws=0 fallback=46`, rig 8) came from a rig running the Amarchy desktop, and the same research
note (2026-11-02-flat-gadgets-invisible.md:95-120) shows that rig ran a stale resident amarchy-flat
(boot race: Amarchy starts UP-Term:bin/amarchy-flat before the boot shadow copy replaces it).
The earlier probe finding that 3.2 sysiclass ignores imp_DrInfo pens in IM_DRAW (probe B == E) does
not stop the clone here: nonzero pixels arrive and flat_clone_map finds glyph pixels.

## Open (not done)
- Reproduce under the Amarchy desktop on 3.2 with a fresh resident (quit + restart amarchy-flat,
  or reboot twice) before any code change. If fallback still rises there, the diag patch here
  discriminates direct-call vs DrawImageState vs re-entry vs empty grid in one run.
- No regression test added: no code change was made, so there is no fix to pin.
