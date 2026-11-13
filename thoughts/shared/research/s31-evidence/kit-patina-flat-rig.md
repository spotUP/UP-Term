# KIT-PATINA-FLAT setup proof, rig 55, OS 3.2 fs-uae, commit e87853bb

Commands: make wasabi (rc 0); UPTERM_RIG=55 rig.py start --max --emu fsuae --os 32 --purpose ...; UPTERM_RIG=55 patina_gui_rig.py --setup (rc 1)

## Setup log lines
```
```

## Diagnosis (ami.py exec, rig 55, after the failure)
```
Status: Patina (Process 5), patina-gui, patina-menu, Workbench, wasabid running; NO patina-flat process
VTCX:Apps/bin/patina-flat STATUS -> rc 0, "flat=absent"
List: patina-flat 113144 bytes present in VTCX:Apps/bin (14:06:43)
```

md5 lines: none; rig has no md5sum (VTCX:Apps/bin/md5sum: Unknown command), so the script prints "not checked" for all four.
No requester text on screen (ami.py requesters empty).

## Verdict: FAIL
Check 1 (earlier resident unloaded on Break) ok. Check 2 (Patina started patina-flat) FAIL: Patina runs and answers, but no patina-flat resident is loaded after 15 s; STATUS from the file reports flat=absent. Cause not hunted.

## Run 2 (after d450f02e)

Rig 55 (fsuae, os 32, --max), build/amiga/patina-flat and Patina up to date. Logs: scratchpad runA.log, runB.log.

Run A (fresh rig, prefs off), `UPTERM_RIG=55 python3 tools/rig/patina_gui_rig.py --setup`, exit 0:
- ok setup: the earlier patina-flat resident unloaded on Break
- ok setup: Patina runs from VTCX:Apps/bin, port PATINA answers
- ok setup: flat prefs off, Patina loaded no patina-flat, STATUS: flat=absent
- (md5 not checked on rig: no md5sum)

Run B (ENV:Patina/prefs with flat-system-look = on and flat-window-gadgets = on via patina_desk_rig.put_flat_prefs(True), same Patina session still running from run A), same command, exit 1:
- ok setup: the earlier patina-flat resident unloaded on Break
- ok setup: Patina runs from VTCX:Apps/bin, port PATINA answers
- FAIL setup: Patina did not start patina-flat in 15 s

State after B: prefs file shows both switches on; Status lists Patina (VTCX:Apps/bin/Patina), patina-gui, patina-menu, no patina-flat process; VTCX:Apps/bin/patina-flat present, 113144 bytes. Not checked: size vs build/amiga/patina-flat, and flat=on proof (run stopped at the FAIL, no hunt).
Likely cause to check (unverified): setup reused the Patina started in run A with prefs off, so dk_flat_sync never ran after the prefs changed; run B needs a restart of Patina after the prefs are put.

## Run 3 (after c3c6654d)

Rig 55, FS-UAE 060, --os 32. Run A (prefs off), then Run B (prefs on via put_flat_prefs(True)).

Run A (exit 0):
- ok setup: Patina runs from VTCX:Apps/bin, port PATINA answers
- ok setup: flat prefs off, patina-flat BOOT left nothing resident, STATUS: flat=absent

Run B (exit 0):
- ok setup: Patina runs from VTCX:Apps/bin, port PATINA answers
- ok setup: patina-flat BOOT loaded VTCX:Apps/bin/patina-flat, STATUS: flat=on calls=0 chained=0

Resident check (Status): Process 8: Loaded as command: UP-Term:bin/patina-flat

Size: rig `List VTCX:Apps/bin/patina-flat` = 113168 bytes (UP-Term:bin/patina-flat the same); `stat -f %z build/amiga/patina-flat` = 113168. Equal. (rig start's 'refreshed patina-flat 113144 bytes' line is the start-time copy; --setup restaged the current build.)

No md5 check (no md5sum on the rig); no FAIL lines in either run. Prefs deleted, rig 55 stopped and cleared.
