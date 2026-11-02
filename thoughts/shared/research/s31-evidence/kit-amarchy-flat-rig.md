# KIT-AMARCHY-FLAT setup proof, rig 55, OS 3.2 fs-uae, commit e87853bb

Commands: make wasabi (rc 0); UPTERM_RIG=55 rig.py start --max --emu fsuae --os 32 --purpose ...; UPTERM_RIG=55 amarchy_gui_rig.py --setup (rc 1)

## Setup log lines
```
```

## Diagnosis (ami.py exec, rig 55, after the failure)
```
Status: Amarchy (Process 5), amarchy-gui, amarchy-menu, Workbench, wasabid running; NO amarchy-flat process
VTCX:Apps/bin/amarchy-flat STATUS -> rc 0, "flat=absent"
List: amarchy-flat 113144 bytes present in VTCX:Apps/bin (14:06:43)
```

md5 lines: none; rig has no md5sum (VTCX:Apps/bin/md5sum: Unknown command), so the script prints "not checked" for all four.
No requester text on screen (ami.py requesters empty).

## Verdict: FAIL
Check 1 (earlier resident unloaded on Break) ok. Check 2 (Amarchy started amarchy-flat) FAIL: Amarchy runs and answers, but no amarchy-flat resident is loaded after 15 s; STATUS from the file reports flat=absent. Cause not hunted.
