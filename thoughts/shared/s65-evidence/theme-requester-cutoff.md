# Theme confirm requester cut-off (s65) - NO VERDICT, repro not achieved

Rig 67 (KS 3.2, --max, 640x256 depth 8, ENV/ENVARC font.prefs deleted + reboot = topaz 8), main 905cdf5d, `amarchy_gui_rig.py --setup --no-build`, flat prefs on, `amarchy_desk_rig.pick(keep=False)`.

## What happened (twice, rig 67 and its reboot)
- pick returned; screenshots 0 s and ~2 s after showed the themed desktop with NO requester (wasabi windows: only the backdrop).
- The next wasabi call (read_ui / env sentinel / ami shot) hung: "daemon did not answer for 150 s"; `rig.py status` = down. FS-UAE process alive; a host capture of the FS-UAE window showed the themed Workbench, pointer, no requester, no guru.
- Requester absent at 0-2 s is consistent with test-register (appears 9-10 s after pick). The guest daemon stopped answering around when the requester should open (or after the 15 s timeout revert). Which is unproven; possible (c): rig/daemon wedged by the requester open or by the revert. Re-repro should poll with host window capture (`screencapture -l <CGWindowID>`; id via swift CGWindowListCopyWindowInfo), not wasabi.

## Code read (bottom border)
- Outer-edge strips: amarchy/flat_glyph.c:541-552 outer_strip; bottom = (1,h-1)-(w-1,h-1), covered by design. Painter amarchy/flat.c rf_edge (~1625) via flat_outer_edge_fill; rf_side uses w->BorderBottom (flat.c:1644). No skip of the bottom edge found. Whether the requester's BorderBottom is mishandled is UNVERIFIED (no pixel measure).
- req_open: amarchy/desk_confirm_dos.c:32-45 (5-line body, 2 buttons; Intuition sizes it from the screen font). rreq_open: :75.

## Not measured
Requester Left/Top/Width/Height, bottom-edge pixels, rreq_open window.

## Repro
rig.py start --os 32 --max; delete ENV:Sys/font.prefs and ENVARC:Sys/font.prefs; rig.py reboot; amarchy_gui_rig.py --setup --no-build; d.put_flat_prefs(True); d.pick(keep=False); then avoid wasabi for ~20 s.

Shots: none kept. Rig 67 stopped.
