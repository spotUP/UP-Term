# Two-strike amarchy-flat crash marker - rig proof (s56, rig 70)

Date 2026-11-04. Build: `make amiga` from main 4c4d9d63 (contains 9286df30), VTC:amarchy-flat 127968 bytes = build/amiga/amarchy-flat. Rig: `UPTERM_RIG=70 rig.py start --max --os 32`, `stop --keep`, block `;BEGIN Amarchy-Flat / Run >NIL: VTC:amarchy-flat BOOT / ;END Amarchy-Flat` appended to build/rig70/os-32/S/User-Startup, every later boot `start --keep-sys`. Every STATUS read by `ami.py exec "VTC:amarchy-flat STATUS"`, marker read by `Type ENVARC:Amarchy/flat-starting` / `List ENVARC:Amarchy` before the reset; reset (`rig.py stop --keep`) 6 s after the last command, i.e. inside the 60 s window.

## Run A - two early deaths: PASS
- Boot 1 (11:38:55): `flat=on calls=0 chained=0`; List `flat-starting 16 ----rwed Today 11:38:54`; Type `flat-starting 1`. Reset 11:39:02.
- Boot 2 (11:56:00): `flat=on calls=0 chained=0`; Type `flat-starting 2`. Reset 11:56:30 (host file still "flat-starting 2").
- Boot 3 (11:57:08): `flat=held calls=0 chained=0 why=marker`; `List ENVARC:Amarchy` -> `Directory "ENVARC:Amarchy" is empty` (marker deleted by the held start).
- Procedure note: a first attempt at boots 2/3 (11:39-11:54) lost wasabid ("wasabid started" in boot.log, daemon unreachable, marker unchanged at 1). Cause: my own earlier `rig.py start` processes were still alive after each `stop --keep` and fought the next start. After killing them and starting one `start --keep-sys` at a time (stop only after the start process had exited), boots 2 and 3 above ran cleanly.

## Run B - forgiven once: PASS
Started from the empty ENVARC:Amarchy left by the held boot.
- B1 (11:58:10): `flat=on calls=49 chained=0`; Type `flat-starting 1`. Reset 11:58:29 (inside 60 s).
- B2 (11:59:24): `flat=on calls=58 chained=0` (marker 1 < 2, not held); Type `flat-starting 2` (N+1 written). After 95 s (12:01:03): `List ENVARC:Amarchy` -> `is empty`, host file absent (60 s timer deleted it). Reset 12:01:13.
- B3 (12:02:15): `flat=on calls=28 chained=0`; Type `flat-starting 1` (fresh start, no hold).

Verdict: CONFIRMED. N>=2 holds with why=marker and consumes the file; a good start >60 s clears the marker; one early death is forgiven.
Rig 70 stopped and cleared after the run.
