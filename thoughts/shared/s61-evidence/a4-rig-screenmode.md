# A4 rig_screenmode 2 FAIL

Before (verbatim):
  FAIL: an --a1200 rig stays --a1200 after `rig.py aga`
  FAIL: every start argument is kept, values too
  rig_screenmode: FAIL (2)

Root cause: tools/rig/rig.py:2088 reboot_args -> tools/rig/paths.py:510 reboot_args read paths.RIG/'start.args'
itself, a second definition of the record that rig.START_ARGS (rig.py:101, what start() writes) names. Level: data
location (two sources of truth for one record). Code regressed, not the test: the test points rig.START_ARGS at a
temp file, the code ignored it.
Fix: paths.reboot_args(given, start_file=None); rig.reboot_args passes START_ARGS.
Reverted fix: both FAIL again. After: rig_screenmode: ok.
