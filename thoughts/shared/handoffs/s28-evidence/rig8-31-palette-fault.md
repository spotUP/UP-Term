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

## Cause hunt (s28, rig 8, 3.1, build md5 on rig = build/amiga for Amarchy 6e72d08b, gui 1a9554f2, menu 6ee7df23)
- MuForce NOT obtained: `rig.py start --max --mmu` on 3.1 never brought wasabid up (15+ min, fs-uae log ends at "68060 MMU: enabled=1 ... RTG head: illegal line count"; TCP 1241 SYN timeouts). Rebooted without --mmu. No fault task/PC/address: UNKNOWN.
- Reboot did NOT reproduce. Two fresh 3.1 boots (second with startup-launched daemons, pri 0, CLI 1/5/7, snoop on IPrefs): `Palette FROM T:flatpal.prefs USE` rc 0, IPrefs raised System Request Retry|Cancel (bar window open), RAM:bootmarker survived. Retry after Break 5 C: no reboot. Cancel with daemons running: no reboot, daemons alive (Amarchy stack FREE 16166). Run 1 of this file (reboot, no requester seen) is therefore not a deterministic property of "Palette USE + 3 daemons".
- Bisect (bar only / gui only / menu only) and Stack 262144 probe: NOT run, there is no reproducing baseline to bisect (budget). Amarchy itself is already CLI stack 16384; gui/menu 262144.
- Break C cause, MEASURED: on a fresh boot `Break 1 C` (menu) and `Break 7 C` (gui) leave both running; `Break 5 C` (Amarchy) ends Amarchy and the other daemons go with it. Cause: amarchy-gui and amarchy-menu use CTRL_C as the menu-toggle hotkey and deliberately clear it: amarchy/gui.c:1755-1756 and amarchy/menu.c:698-699 (`Wait(port|SIGBREAKF_CTRL_C); SetSignal(0, SIGBREAKF_CTRL_C);` "a stray toggle must not end the server"); also menu.c:187-188/219-220, gui.c:1175, menu_open.c:31/64 (Signal CTRL_C = toggle). Amarchy proper exits on CTRL_C (amarchy.c:497-498). So Break C on 3.1 works for Amarchy, never for gui/menu; the earlier "Break C fails" is the gui/menu being targeted (or a Break aimed at 7/1 first). Break ALL also sets D/E/F, which something (ixemul/signal path, not traced) turns into an exit.
- Related: with Break C only on gui/menu and the bar open, Palette USE gives the IPrefs requester (bar window "Amarchy bar" blocks WB reset), same mechanism as R1.17-WBRESET.
- Proposed fix level: stop path = a quit verb on the gui/menu port (or CTRL_D as quit) so scripts and `Break` have a defined end; reboot: needs the MuForce/Guru capture, MMU boot of the 3.1 rig must be fixed first (or use the real 3.2/3.1 A1200 / rig 2 mmu); the stock-Palette defence stays at the desk_dos_apply level (close bar + menu window before WB reset), not in a signal handler.
