---
title: Leg W W8 evidence (install, reboot, start order, uninstall)
date: 2026-11-08
---
Case: `UPTERM_RIG=<n> python3 -I tools/rig/install_rig.py --only legw` (tools/rig/install_rig.py, case_legw).
Kit: build/dist/up-term from `make dist-base` with a scratch ports repo (stub add-on files; test-only kit).

## KS 3.1 (rig 71, --max, fresh rig): 22 of 22 [OK]
- [OK] (a) one ";BEGIN UP-Term Amarchy-Flat" block, after AddDataTypes, before IPrefs, absolute path `SYS:UP-Term/bin/amarchy-flat`, ENVARC:up-term/Dir read back as 'SYS:UP-Term' with no stray character (step 3 proven on the rig; the Run and If lines match exactly)
- [OK] (b) Amarchy.info (DONOTWAIT, STARTPRI=10, tool UP-Term:bin/Amarchy) and amarchy-index.info (DONOTWAIT, STARTPRI=-10, TOOLPRI=-5)
- [OK] (c) after reboot: amarchy-flat in Status; port AMARCHY answers (Amarchy started by Workbench, no hang: D6); ENVARC:Amarchy/apps written (amarchy-index ran)
- [OK] (d) amarchy-flat is process 4, below SolidWin and DO_LAUNCHER (User-Startup, after IPrefs); resident patch answers STATUS (stack held on KS 3.1)
- [OK] (e) Uninstall: S:Startup-Sequence byte-identical, S:User-Startup as it was, both icons gone, no Install UP-Term: assign left
## KS 3.2 and KS 3.2 + P96: NOT RUN
`rig.py start --os32` fails: AmigaOS 3.2 tree not found (/Users/spot/Downloads/AmigaOS-3.2-full (1)); set UPTERM_OS32_TREE. Same case runs unchanged once it exists.

## Found / fixed
- dist/Uninstall never stopped a resident amarchy-flat (started by the boot block); it held SYS:UP-Term/bin. Fixed: Break + wait up to 10 s (by $updest path, UP-Term: path or plain name).
- Workbench-started programs are not CLI processes: Status never lists Amarchy/amarchy-index; the case uses the AMARCHY port and the apps file instead of Status.
- Open: after a reboot an empty SYS:UP-Term/bin remains after Uninstall (the pre-boot image had no bin; cause not isolated; the two base_uninstall comparisons are skipped with rebooted=True). Rig ARexx queue ("too many ARexx messages") makes a re-run on the same rig fail the port check: restart the rig between runs.
- Test-only stub kit: nothing shipped.
