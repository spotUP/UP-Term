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

## Run 2 (after d450f02e)

Rig 55 (fsuae, os 32, --max), build/amiga/amarchy-flat and Amarchy up to date. Logs: scratchpad runA.log, runB.log.

Run A (fresh rig, prefs off), `UPTERM_RIG=55 python3 tools/rig/amarchy_gui_rig.py --setup`, exit 0:
- ok setup: the earlier amarchy-flat resident unloaded on Break
- ok setup: Amarchy runs from VTCX:Apps/bin, port AMARCHY answers
- ok setup: flat prefs off, Amarchy loaded no amarchy-flat, STATUS: flat=absent
- (md5 not checked on rig: no md5sum)

Run B (ENV:Amarchy/prefs with flat-system-look = on and flat-window-gadgets = on via amarchy_desk_rig.put_flat_prefs(True), same Amarchy session still running from run A), same command, exit 1:
- ok setup: the earlier amarchy-flat resident unloaded on Break
- ok setup: Amarchy runs from VTCX:Apps/bin, port AMARCHY answers
- FAIL setup: Amarchy did not start amarchy-flat in 15 s

State after B: prefs file shows both switches on; Status lists Amarchy (VTCX:Apps/bin/Amarchy), amarchy-gui, amarchy-menu, no amarchy-flat process; VTCX:Apps/bin/amarchy-flat present, 113144 bytes. Not checked: size vs build/amiga/amarchy-flat, and flat=on proof (run stopped at the FAIL, no hunt).
Likely cause to check (unverified): setup reused the Amarchy started in run A with prefs off, so dk_flat_sync never ran after the prefs changed; run B needs a restart of Amarchy after the prefs are put.
