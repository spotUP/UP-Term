---
date: 2026-10-31
session: vtcon-05 s37
status: phase done
---
# vtcon-05 session 37: rig fixture fakes, up-zip rig case

## Done (ledger rows ticked with "s37 2026-10-31")
- RIG-FIXTURE-6FAIL (1b960b05): the six failures were stale test fakes, not rig code. Addon fake now answers
  install_rig.lock_exists (Echo >T:rig-exists.dos + Execute); RigStart fakes stub port_listeners and record
  sys_hdf_monitors / sys_hdf_dopus. tests/test_rig_fixtures.py 66/66.
- UPKG-ZIP-RIG (29cc0144): upkg_rig case `zipaddon`; app_addon() shared with `appaddon`. Rig 4: zipaddon 10/10,
  appaddon 11/11. Pushed.

## Next
- Open vtcon-05 rows: RIG-RECHECK-STALE (nvim SysBase hang first), RJ7-CANCEL-STILL-SLOW, RJ7-DRAWN-UNRUN,
  NVIM-PERF, AMARCHY-LAUNCH-FIELD7, RIG-AMITCP-UNSTAGED.
- Ports session item still open: upterm-ports mk/common.mk:123 `seq 1 0` on macOS.
