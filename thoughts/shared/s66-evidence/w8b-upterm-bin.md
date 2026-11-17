# W8b: empty SYS:UP-Term/bin after Uninstall following a reboot (s66)

Rig 77, KS 3.1 (`rig.py start --os 31 --max`). Kit: `make dist-base` with UPTERM_PORTS = scratch shim (real staged
userland copy + STAGED-FROM stamp + stub files/SOURCES rows for slim.txt paths the old staging lacks; test-only kit).

## Hypothesis (from code read)
- Leg W starts Amarchy from SYS:WBStartup (tool UP-Term:bin/Amarchy). A Workbench-started program is no CLI process:
  `Status COM` never lists it (the legw case itself notes this), so dist/Uninstall's `Status >ENV:upw COM ...Amarchy`
  + `Break $upw C` never reaches it after a reboot. Workbench holds the wa_Lock on the tool's drawer (UP-Term/bin)
  until the process ends, so `Delete "$updest" ALL` deletes every file (LoadSeg closed them) but not the drawer.
- Before a reboot Install started Amarchy by Run (a CLI process), so the pre-reboot Uninstall paths never showed it.

## Fix
- amarchy/amarchy.c quit_cli: `Amarchy QUIT` sends the ARexx QUIT via rexx_send (no RexxMast), waits up to 10 s for
  the AMARCHY port to go. dist/Uninstall runs `"$updest/bin/Amarchy" >NIL: QUIT` (UP-Term: assign already removed
  there) before the Status/Break path, and retries the drawer Delete up to 10 s while $updest/bin exists.
- tools/rig/install_rig.py: base_uninstall's rebooted=True skip removed (both compares run after the reboot).
- tests/test_amarchy_docs.py: test_uninstall_quits_a_workbench_started_amarchy_through_its_port (fails with fix stashed).

## Measured on rig 77 (KS 3.1), legw fail-before, 2026-11-09

legw with the rebooted skip removed: "passed 24 of 25", FAIL 21 after base Uninstall: SYS:UP-Term as it was
before Install (bin left). ixemul compare ok 22 (no skip needed, stock_libs re-assigns LIBS after reboot).

Status before Uninstall:
  Workbench 3.1:UP-Term/bin/amarchy-gui
  Workbench 3.1:UP-Term/bin/amarchy-menu
  SYS:UP-Term/bin/amarchy-flat
Amarchy itself is not in Status (started from SYS:WBStartup icon). After Uninstall all three still run;
`Delete SYS:UP-Term/bin` -> "object is in use".

Lock holders, root cause in dist/Uninstall stop block:
1. amarchy-flat loaded as "SYS:UP-Term/bin/amarchy-flat" (path from ENVARC:up-term/Dir). Uninstall's Status COM
   names ("$updest/bin/amarchy-flat" = "Workbench 3.1:UP-Term/...", "UP-Term:bin/..." after assign removed,
   bare name) never match. Proven: `Status COM "SYS:UP-Term/bin/amarchy-flat"` + Break C ends it.
2. amarchy-gui / amarchy-menu ignore Ctrl-C by design (menu.c:716-717, gui.c server loop): they end only on
   MS_QUIT from Amarchy's shutdown (act_menu_server_quit, amarchy.c:771). Break of them: no effect (measured).
3. Amarchy (Workbench-started) is unreachable by Break. Its AMARCHY port exists but does not answer ARexx:
   `Amarchy QUIT` (new quit_cli) returned rc 10; an AMARCHY STATUS query timed out. So Amarchy never runs its
   shutdown and gui/menu stay. Why Amarchy stops answering its ARexx port is OPEN (same symptom as
   "too many ARexx messages" on legw re-runs).

Fix still needed: (a) stop amarchy-flat by the ENVARC:up-term/Dir name too (read before line ~174 deletes it);
(b) make Amarchy quit reliably (find why AMARCHY port stops serving), then gui/menu end with it;
(c) keep the bingone retry loop. Then pass-after on a fresh rig.
