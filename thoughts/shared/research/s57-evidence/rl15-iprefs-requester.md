# RL15 IPrefs "System Request" on flat toggle + theme pick (2026-11-05)

## Requester text

Rig 124 reads-4 had it on screen. uitree reads only the buttons (Retry/Cancel), so the text is not readable there.
The s31 research read the text on rig 56:
"Intuition is attempting to reset the Workbench screen. Please close all windows, except drawers."
On 3.2 it reads: "... Please close the following windows: Amarchy bar".
This run did NOT re-read the text on rig 128. See "Rig 128" below.

## Cause (code, desk_dos.c)

- Invariant (desk_dos.h:23): a Palette USE whose pen map differs from the live DrawInfo pens resets the Workbench screen.
  Amarchy's windows must be closed first, or IPrefs' CloseWorkBench fails and raises the requester.
- The menu pick runs gt_dos_select, then desk_dos_apply, then dk_apply, then dk_flat_pens.
  - dk_apply honours the invariant: dk_windows_close and dk_windows_open sit around its write, guarded by desk_pens_reset.
  - dk_flat_pens does not. After a flat toggle, desk_pens_sync is true, so dk_flat_pens writes palette.prefs with the bar and Amarchy windows open.
- IPrefs then waits on the requester. dk_wait_reset (6 s) and dk_wait_iprefs (6 s) run to their timeouts.
  The WBRESET close_hook adds its 3 s waits. This matches the 40-71 s per pick on rig 124.
- The third palette writer, desk_dos_start_apply, already brackets its write.

## Fix

dk_flat_pens computes desk_pens_reset(live, nlive, flat, depth) and puts dk_windows_close / dk_windows_open around dk_put_palette when it is true.
This is the same rule as dk_apply. A colour-only write (pens unchanged) resets nothing and keeps the windows open.

## Tests

- New tests/test_palette_write_closes_windows.py (make test ONLY=palette_closes), a structural test because desk_dos.c is Amiga-only.
  - It checks that every function in desk_dos.c that calls dk_put_palette has dk_windows_close before the call and dk_windows_open after it.
  - It checks that dk_flat_pens guards the bracket with desk_pens_reset.
  - With the fix reverted: 2 of 2 fail. With the fix: OK.
- test_flat_prefs_notify, test_wb_absent_restore, test_amarchy_editor_stack: OK.
- make test ONLY=desk: 2286 checks, 0 failed.
- make amarchy-flat amarchy-gui amarchy: rc 0.

## Rig 128 (3.2, base b8c7844f): NOT VERIFIED

- installer_rig.py ONLY fresh-all passed through step 33.
- Step 34 failed after the reboot: the AMARCHY port did not answer and the daemon froze for 15 s.
- After the next reboot, the screen showed the Workbench with only the RAM Disk icon and the busy pointer.
- wasabi ps showed process 1 "Background CLI (SYS:Prefs/Pointer)" in wait for minutes.
  Amarchy's start apply runs that editor through dk_editor (SystemTags, synchronous), so the Amarchy start blocks on the Pointer editor.
- No startup script calls Pointer.
- This hang is separate from RL15 and has not been hunted.
- So the no-requester toggle and the 5 s timing were not measured on a rig. The fix is proven by host tests only.
