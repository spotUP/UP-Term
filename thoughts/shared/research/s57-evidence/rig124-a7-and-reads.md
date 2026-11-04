# Rig 124: A7 flat-boot hunt + release reads (s57, 2026-11-04, vtcon-c1 agent)

Build: main 7cf4ac17, `make amarchy-flat Amarchy amiga` rc 0. md5 rig SYS:UP-Term/bin vs build/amiga equal:
amarchy-flat a7f57fa0cbbeeb1442dce508d66a5de3, Amarchy 8535ce7ff3bcda063accc04eda519eed, amarchy-gui 3991070d913f4bb33999504f6e9ae24b.
Rig 124: OS 3.2 full install (UP-Term: = Empty-12MB-FFS:UP-Term, no Rushhours:), rebooted once (ami.py reboot, back after 20 s).

## A7 verdict: no product bug; two rig-helper bugs

1. "flat=0 calls=0" was a misread. After the reboot, the User-Startup block `Run >NIL: UP-Term:bin/amarchy-flat BOOT`
   leaves `UP-Term:bin/amarchy-flat` resident and STATUS line 1 is `flat=on calls=145 chained=0`, rectfill installed
   calls=406, text installed labels=15, clone draws=43. The quoted `frame=hooked calls=0 flat=0` is the LAST STATUS line,
   the frameiclass hook (flat_core.c flat_frame_status_line: flat = draws skipped); calls=0 = no frameiclass image drawn
   yet. Break on the boot copy unloads it (STATUS flat=absent). No crash/hang, so no Stack 262144 probe needed.
2. `amarchy_gui_rig.py --setup` "earlier amarchy-flat resident unloaded on Break": the setup Broke only processes loaded
   from `UP-Term:bin/` or VTCX:Apps/bin. amarchy_btnbg_rig.py leaves `RAM:amarchy-flat` resident (Process 13 in the
   earlier Status listing), never Broken, so the check failed after the kit Amarchy was already Broken: hence the next
   helper's "the Amarchy commodity is not running". Fix: setup_break_targets() Breaks every SETUP_NAMES program from any path.
3. Same setup copied the kit from a fixed `Rushhours:Tools/UP-Term` (3.1 kit rigs only); on a 3.2 install that Copy
   did nothing and `Assign UP-Term: VTCX:Apps` + `ADD Rushhours:...` left UP-Term: = VTCX:Apps alone. Fix:
   kit_upterm_dir() reads the installed target from `Assign UP-Term: EXISTS`.
Regression test tests/test_gui_rig_setup_breaks_any_copy.py (ONLY=gui_rig_setup_breaks): fails on 7cf4ac17, passes with the fix.
Live: RAM:amarchy-flat started, fixed `--setup` rc 0: "Amarchy runs from VTCX:Apps/bin, port AMARCHY answers",
"amarchy-flat BOOT loaded VTCX:Apps/bin/amarchy-flat, STATUS: flat=on calls=36"; UP-Term: = VTCX:Apps + Empty-12MB-FFS:UP-Term.

Operator slip (mine): `Assign UP-Term: LIST` and `Assign VTC: LIST` removed both assigns (known rule); restored at once to
Empty-12MB-FFS:UP-Term and VTCX:, verified with EXISTS before the reboot.

## Reads

| ID | Measured | Verdict |
|---|---|---|
| RL21 icon label AA | amarchy_iclabel_rig.py: drawer window None (ARexx WINDOW SYS: OPEN still opens nothing on 3.2), labels=0 lskip=0 in that run; boot STATUS had labels=15 lskip=5; WB alive; Break unloads | NOT RUN as pixel read (harness: drawer open). Sentinel only: labels counter rises at boot |
| RL3, RL15, RL16, S32-GREEN-LINE-96, S32-SCROLLBAR-PROP, FLAT-BUTTON-FILL, RL19, RL8 | - | NOT RUN (budget spent on A7) |

Rig 124 left up: setup state (UP-Term: = VTCX:Apps + SYS:UP-Term, Amarchy + amarchy-flat from VTCX:Apps/bin), theme vantablack, flat on.
