# FLAT-BOOT-FREEZE: amarchy-flat from User-Startup froze AmigaOS 3.2 at boot

Rig 66, OS 3.2, `--max`. Follows flat-freezes-daemon.md (wasabid lost in 4 of 4 boots).

## Evidence

The probe build had a serial trace (exec RawPutChar) around each step of install_all, the 1 s pen loop and the
icon pen holder. The trace of a frozen boot:

    MHkQqABw  MHKRTt S Q q A B 1 2   (then nothing)

- First tick: the Workbench is not open (NOCLASS). The pen sync runs with no screen and reads no theme.
- Next tick: install OK (hook, RectFill, Text patches). Then pens_sync -> ipd_owner: the semaphore is obtained (Q q),
  LockPubScreenList is held (A B), ip_sync calls the theme op: amconf_load returns (1 2), gt_load's theme file
  Open/Read never returns.
- The sysiclass draw hook and its LockIBase (suspect 1) never ran in the frozen boots ('L' never seen).
  Opening a shell window on a settled Workbench drove 28 hook calls with no hang.

## Cause

amarchy/iconpen_dos.c pens_sync (and ipd_recolour) locked the public screen list, then let ip_sync call
`o.theme = pens_theme`, which reads ENV:/ENVARC: prefs and the theme file. One second after LoadWB opened the
Workbench, that read blocks in DOS (volume or handler not ready; a requester needs the public screen list to open
its window). LoadWB and wasabid both wait on the list lock too, so the whole system stops.

## Fix

Branch fix/flat-boot-freeze. pens_theme now runs before LockPubScreenList; the ops get pens_theme_cached, which only
copies the theme already read into pen_ctx. No DOS call runs while the list is locked. The colour map and
ObtainPen/SetRGB32 stay under the list lock, as before.

Not changed: amconf_load inside Forbid in flat.c main (suspect 3). It runs once at start before any Workbench window
work, and was not on the frozen path.

## Test

tests/test_ipd_no_dos_under_list.py (make test ONLY=ipd_no_dos_under_list): no DOS reader between
LockPubScreenList and UnlockPubScreenList in iconpen_dos.c, no `.theme = pens_theme` in a list-locking function,
and pens_theme_cached reads no file. 2 failures before the fix, OK after.

## The UP-Term: requester (coordinator report, owner saw it on rig 66)

Rig 66 is a plain rig, not a full install: UP-Term: is not assigned (rig.py only touches UP-Term: behind
`Assign EXISTS`). GT_SHIPPED_DIR is `UP-Term:Amarchy/Themes` (amarchy/gthemes_core.h:15), so the theme read in
pens_theme asks DOS for volume UP-Term:, and DOS puts up "Please insert volume UP-Term:". That is the blocked
gt_load in the trace: with the list locked the requester cannot open, so the system froze; with the first fix it
opened and sat on the screen (verify run 1: daemon answered 3 of 3 boots, but rig start rc 1 and every EXEC ended
on the open requester, so those boots were not counted).

The same freeze hits a real install whenever flat starts before the UP-Term: assign line in User-Startup.

Second commit: pens_theme sets pr_WindowPtr = -1 around its reads (same pattern as menu_font.c, proclaunch.c), so a
missing volume is a failed read (theme defaults), never a requester. The test also guards this.

## Verify (fixed build, flat in User-Startup, no Wait)

- 3 boots: rig start rc 0, daemon ping rc 0 each time, no requester error. STATUS was not captured
  (wrong ami.py subcommand in the script).
- Boot 4: ping rc 0, `ami.py requesters` empty, `ami.py exec "VTC:amarchy-flat STATUS"` prints
  `flat=on`, rectfill and text installed=yes, 16 pens held.
- Before the fix: 4 of 4 boots froze (flat-freezes-daemon.md) and the trace boot froze again.
