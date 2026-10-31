# Rig 8, AmigaOS 3.1: Palette USE fault (s28)

Verdict: the Amiga reboots (not just wasabid dying). amarchy-flat is NOT involved. The running Amarchy bar daemon (UP-Term:bin/Amarchy, started from S:User-Startup) is the trigger: with it (and its amarchy-gui / amarchy-menu children) stopped, the same Palette USE survives.

## Setup (fresh start, KS 40.68 / WB 40.42)
- Start [WARN]: only "Directory Opus 5.104 is not installed in this rig's VTC: drawer" (no drift line).
- Before Palette USE: processes Amarchy (CLI 5, stack 16384), amarchy-gui (CLI 7, 262144), amarchy-menu (CLI 10, 262144) running. amarchy-flat NOT running, never launched. `amarchy-flat STATUS` not applicable (not run, no patch installed).
- amarchy-flat copies on the rig: exactly one, VTC:amarchy-flat md5 9aebbeaa427e9b841ad5a99f153541d2 (equals build/amiga). None in UP-Term:bin, C:, RAM:.
- Marker RAM:bootmarker created before; wasabi debug + snoop streams armed.

## Run 1: `SYS:Prefs/Palette FROM T:flatpal.prefs USE` (palette-flat-pen8.prefs), Amarchy running
- 18:08:45 Palette returns rc 0 (writes ENV:Sys/palette.prefs); IPrefs reads it; DOpus busy. Last snoop line at 18:08:47.294. Then silence.
- wasabi: "connection closed by the Amiga", then Connection refused; both streams "reconnecting"; reconnected after 28 s at 18:09:15.
- After: RAM:bootmarker gone (RAM: wiped), `Assign` runs from startup at 18:09:17 in snoop, tasks have new addresses, T:lastguru absent. => a full reboot.
- fs-uae log (build/rig8/logs/fs-uae.log.txt.tail): `CPU reset PC=f80dc8 (Kickstart ROM)` then `CPU reset PC=f800d0` = the ROM's own ColdReboot/RESET sequence, not an emulator crash. No Guru text captured: T:lastguru lives in RAM: and the reboot wiped it; no on-screen read possible (box gone before polling). Guru code/address/task: UNKNOWN (a deadend alert is possible but unproven).
- Snoop right before the reboot: Palette -> IPrefs OpenLibrary workbench.library, DOpus tasks re-read wbpattern, no Amarchy line visible (but "5 snoop event(s) lost").

## Run 2: control after the reboot, Amarchy + amarchy-gui + amarchy-menu stopped (Break 5 C; 7 and 10 died with it)
- Same Palette USE: rc 0, Amiga stayed up (RAM:bootmarker2 still there 8 s later, wasabid answered). No fault.

## Not done (budget)
- Did not isolate which Amarchy piece (bar window, menu, screen lock) reacts to the IPrefs Workbench reset; did not retest with `Stack 262144` on Amarchy (its CLI stack is 16384, FREE 16174); did not test a stock (non-flat) prefs with Amarchy running. Note amarchy.c:157 and gui.c:1454 already special-case the Workbench reset IPrefs runs after a prefs USE.
- Next step: boot fresh, `Stack 262144` + start Amarchy, Palette USE; then bisect bar/menu; arm MuForce (`rig.py start --max --mmu`) to get the fault address.

Snoop log: rig8-31-palette-fault.snoop.log (366 lines, small). Rig 8 stopped at end.
