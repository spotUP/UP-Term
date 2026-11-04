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
