# DOpus boot hold check (rig 7, --os 32) -- RUN, result: UNKNOWN

Earlier attempt (rig never got past `rig.py start` under machine load): no data.

Verdict: UNKNOWN. The run could not exercise the hold: Patina was never running in the rig, so no PATINA_DESK_START_PLAN /
PATINA_DESK_START_WRITES sentinel was ever set. No "Intuition is attempting to reset" requester was seen either, but that is
meaningless without Patina having tried to apply.

Evidence
- Build: `buildq ... make amiga` finished; build/amiga/Patina 411780 bytes, md5 bc0b605db70850c357eb9647317bcef0 (19:18, newer than
  patina/desk_dos.c 18:53), installed VTC:Patina matched that md5.
- `rig.py dopus --os 32` + `rig.py start --os 32` worked (rig 7 up; DOpus5:DirectoryOpus running). `rig.py install` does NOT put
  Patina.info into SYS:WBStartup on an --os 32 host tree (only the sys.hdf clone path / kit does). I copied build/dist/up-term/Files/wbstartup/Patina.info
  (default tool UP-Term:bin/Patina) and the binary to SYS:WBStartup and UP-Term:bin, deleted ENV:Sys + ENVARC:Sys, rebooted.
- Poll (3 s) through boot: 31 s `['', 'Directory Opus']` splash seen (same as dopus-splash-window.md), 34 s a `Volume Request`
  window (402x56, active) appeared and stayed; final window list = Volume Request + DOpus lister + button bank + backdrop.
  `Status` showed no Patina process; ENV:Patina dir created, GetEnv of both sentinels returned nothing for the whole 200 s.
- Likely cause of the stall: the WBStartup icon's tool path `UP-Term:bin/Patina` needs the UP-Term: volume/assign the kit's
  User-Startup block makes; an --os 32 rig lacks it at WBStartup time (requester text not read; UNVERIFIED).

Next time
- Use a rig whose startup really launches Patina (a sys.hdf kit clone, `rig.py start --max`, not --os 32 host tree), or add the
  `Assign UP-Term:` line to S:User-Startup before the reboot, and read the Volume Request text via wasabi first.
- Then repeat: Delete ENV:Sys/#?, reboot, poll windows + GetEnv PATINA_DESK_START_PLAN / _WRITES.
- PASS = no "Intuition is attempting to reset" requester; PLAN nonzero and WRITES set only after window `Directory Opus` is gone.

## Try 3 (kit rig)

Verdict: FAIL on the "no requester" criterion; the hold ordering itself looks right. One run (not repeated: no PASS to confirm).

Setup: rig 7, `rig.py dopus --purpose ...` (installed 5.104 on the sys.hdf clone), `rig.py start --max`, then `Delete ENVARC:Sys/#? ENV:Sys/#?`, `rig.py reboot`.
Patina ran at boot from the kit: SYS:WBStartup/Patina.info (DONOTWAIT) -> UP-Term:bin/Patina, 411780 bytes (equals build/amiga/Patina; md5 not
checkable on the guest, `rig.py install` not run: UNVERIFIED that it is the bc0b605d build, size only). ENV:Patina/prefs present.

Poll (3 s, windows + screen title + GetEnv), seconds from reboot:
- 35 s  Workbench Screen, no windows; PLAN and WRITES unset
- 38 s  windows ['', 'Directory Opus'], screen title 'Directory Opus' (the splash)
- 41 s  splash gone; "System Request" (task IPrefs, buttons Retry/Cancel) open; PLAN=22, WRITES unset
- 85 s  Patina bar window appears; WRITES=3 (set after the splash was gone: ordering as intended)
Requester body text: NOT readable (read_ui exposes only the Retry/Cancel gadgets, the text is IntuiText). Identified as the Workbench-reset request by
its owner task IPrefs + the rig's own RIG-IPREFS-SYSREQ detection (rig.py start, "stayed after 3 Retry clicks; windows were open while IPrefs reset
the screen"). Exact wording UNVERIFIED. rig.py start's own boot logic clicked Retry 3 times before I took over (not me).
Owner correction applied: closed the DOpus Ram Disk lister (`RX "address DOPUS.1; lister close all"`; a click on its close gadget did nothing),
then clicked Retry once. Requester came back (new window address): still open on the screen are the Patina bar, the DOpus backdrop and the
button-bank window, which the requester also wants closed. Did not close those (would end the session under test).
Reading: requester appears right after the splash closes (not during the hold), at the same time PLAN is set; so the hold does not prevent it.
The apply (WRITES=3) ran 44 s later while the requester was pending. Whether the requester is caused by Patina's apply or by DOpus alone
(--no-dopus control, or Patina absent) was NOT tested: one level deeper cause UNKNOWN.
