# L7 pens-held rig check (rig 10, AmigaOS 3.2, 68060, build of rebased 5b02d3d8 on 24d72aec)

Host suites: iconpen 2008/0, flat_core 675/0, flat_on_hang OK, desk 1828/0, iconfile 125/0.
Regression: with the `h->screen = NULL; h->depth = 0;` lines removed from ip_holder_release, iconpen fails at tests/test_iconpen.c:155 (want 1, got 0).

Rig (STATUS of amarchy-flat; Amarchy process alive and AMARCHY port answered after every step):
1. boot with flat ON:            pens-held 16  PASS
2. flat OFF (theme pick):        pens-held 0   PASS
3. flat ON again (theme pick):   pens-held 14  FAIL (plan wants 16)
4. fresh start flat ON (later):  pens-held 16

Open: step 3 obtains only 14 of 16 after off->on on the same screen; the first obtain at boot gets 16. Likely 2 pens taken by another
user of the Workbench palette between release and re-obtain (the theme pick re-applies palette/pens), or the re-obtain path
asks pens that the pick itself holds. Not investigated further (budget). Flat ON hang fix stays effective: no hang in 3 steps.
Rig tick stays OPEN. Not pushed.

## Follow-up measurement (rig 10, 3.2) - cause partly found, NOT fixed, NOT landed
STATUS now prints `pens-held N missing <pens> dim=<pen>` (ip_status_line names the pens another user holds).
- Cause 1 (fixed in amarchy/flat.c): the dim pen (ObtainBestPen, shared) sat on an icon pen index. Measured: `pens-held 14 missing 43 47 dim=47`,
  `missing 43 46 dim=46`; flat OFF puts the dim pen on 47 (dim=47 after the OFF pick), ON then finds 47 taken. Boot order was dim_refresh THEN pens_sync;
  now pens_sync first; on Ctrl-D the dim pen is released before pens_sync (only when not held). After: dim=246/250, off the icon range.
- Cause 2 (OPEN): another holder of pens in 32..47 that is not amarchy-flat. Boot after the fix still gives `pens-held 15 missing 43`; a restart with
  Amarchy/amarchy-gui/amarchy-menu all ended gave `pens-held 3 missing 32..42 44 47`; CloseWorkBench is not in C: here, LoadWB did not change it.
  So not Amarchy; likely Workbench icon pens (shared) or leftovers of the failed theme pick. Count varies run to run (16, 15, 14, 13, 3), so the
  rig state, not the OFF->ON path alone, decides. Next: identify the holder (run the probe on a fresh rig boot with no picks; open/close drawers; compare).
- Rig picks became flaky ("open menu: no stable state") after the first pick in a run; probe at scratchpad probe.py (put_flat_prefs + pick + STATUS).
Host: iconpen 2008/0, flat_core 675/0 (test_iconpen.c checks the `missing 40` text).
