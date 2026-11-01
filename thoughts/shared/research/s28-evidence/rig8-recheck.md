# S28 rig 8 recheck (AmigaOS 3.2) - partial, A-E not measured

Branch s28-rig8-recheck, code head 15d5daaf.

## Host side (done)
- Flat off never deletes ENV:Sys/Workbench.prefs; it puts only the saved border word back (desk_core.c desk_wbborder, desk_dos.c dk_wbborder; DESK_WB_DELETE removed).
- Tests: test_desk_wbborder_absent updated, test_desk_wbborder_keeps_tweaks added.
- ONLY=desk: 1828 checks, 0 failed. ONLY=flat_core 675/0, ONLY=amconf 709/0.
- With the fix stubbed out (branch forced to the old path): 6 failed checks (tests/test_desk.c lines 303, 305, 306, 319, 320, 321).
- Emboss rect (-3 -3 3 3 to -1 -1 1 1) NOT written: the question is unanswered because no rig measurement completed.

## Rig side
- Rig 8 started twice (host load 40-57, first start took about 6 min). First run died during a hung md5sum on the rig.
- Second run: setup ok (Amarchy, amarchy-menu, amarchy-gui md5 match). Base step: DrawInfo stock SHINE 2 SHADOW 1 BACKGROUND 0, no Workbench.prefs in ENV or ENVARC, amarchy-flat md5 18d0667922958fbae7265ee377007c20.
- The "on" step (flat on, probe run) lost the connection; rig status then reported: crashed: Illegal instruction 7fff at 1000DDBC -> 00F80B86, 2 CPU resets. Not investigated (tool budget used up). Possible real crash in flat-on path on 3.2 or in the probe; needs a stack-first check (memory rule) on a fresh run.
- Checks A, B, C, D, E: not measured. No screenshot taken.
