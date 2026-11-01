# Tutorial rig check (rig 5, 2026-10-30)
L-C1 rig half: FAIL. L-R: FAIL. Cause is not the tutorial: on rig 5 every external command started from vsh
(UP-Term:bin/mkdir, ls, cp, ...) returns rc 161 and does nothing; builtins (true) are fine. Same binaries run
from the AmigaDOS shell (rc 0, dirs created). vsh -c and a script file both show it; Stack 262144 does not change it.
Tutorial enter_sandbox does `mkdir -p T:tutorial || exit 1`, so selftest exits rc 1 silently ("+ exit 1" under vsh -x).
Lesson 1 typed: "BOOTX: % ls" (no output) ... "vsh: cd: docs".
Driver: UPTERM_RIG=5 tools/rig/tutorial_rig.py; log tut.log; verdicts build/rig5/tutorial.verdicts.json.
Selftest 01-14 FAIL (rc 1, empty), hint-before PASS, typed PASS, menu/slash FAIL (no T:tutorial, "active title 'tabwin'"), lesson12/lesson1 FAIL.
