---
date: 2026-11-04
topic: rig 96 (OS 3.2, owner eye-check rig) lost the UP-Term: assign; AssignWedge "Volume Request" for UP-Term
status: UNPROVEN (read-only hunt, ~40 tool calls; rig 96 untouched)
---

# Rig 96: UP-Term: assign gone, Volume Request for UP-Term

Verdict: **UNPROVEN.** No fix committed (the brief allows a fix only for a proven cause).
Rig 96 was read only through `ami.py exec` / `ami.py requesters` from the main checkout. Not rebooted, no Assign
change, the requester left open.

## Timeline (host mtimes in build/rig96, main checkout)

| Time | Evidence | Meaning |
|---|---|---|
| 21:57:04 | vtc/runinstall, runinstallcon, rununinstall | install_rig.prepare (installer_rig run_base_case) |
| 21:57:06-21:57:21 | vtc/longwrap = `Execute VTC:rununinstall`, longdone = 0 | the LAST run_long of the run: the pre-install Uninstall. Its `Assign UP-Term:` (dist/Uninstall:177) is BEFORE the install and the reboot, so it does not explain the loss |
| 21:57:57 | ENVARC:up-term/Dir | Installer writes Dir |
| 21:59 | owner.json, boot/*, kit-hash 21:59:25 | post-install `rig.py reboot` (installer_rig.rig_reboot, :1635) |
| 22:00 | boot/boot.log | full boot sequence through "assigns done", "UP-Term shadows done", "wasabid started" |
| 22:00-22:02 | retry.log | rig.py retry_wb_reset (:2343) polls the UI tree; ends on "Welcome to Amarchy" + "Amarchy bar", no other requester |
| 22:02 | vtc/vshwin-f1host.script | reboot_amarchy_check -> tabstate.open_host('f1host') |
| 22:02:45 | every *.info / *.uaem on SYS: (os-32) and VTC: (Apps/UP-Term.info, distkit/*.info, kit-userland/upkg.info) | Amarchy icon apply at boot (amarchy-flat BOOT / icons.c APPLY) |
| 23:01:49 | last-command | later ami.py reads (this hunt's caller) |

## Rig state now (read-only)

- `Assign LIST`: no UP-Term:; `GG  Empty-12MB-FFS:UP-Term` (User-Startup line 52 `Assign GG: UP-Term:` ran at boot, so
  UP-Term: existed then); `LIBS  Empty-12MB-FFS:Libs + VTCX:amissl`; `AmiSSL  Work:AmiSSL`; "Denied volume requests:" empty.
- LIBS: is exactly what `install_rig.stock_libs()` (install_rig.py:490) leaves on this rig (SYS:Libs, the ncurses ADD and
  SYS:AmiSSL/Libs fail, VTC:amissl ADD). User-Startup line 71 `Assign LIBS: AmiSSL:Libs ADD` is gone from LIBS:. So
  stock_libs, the LAST step of installer_rig.reboot_amarchy_check (:1114), ran after the boot, and nothing reassigned
  LIBS: after it. install_rig.Fixtures.restore (:561) would have put rig_libs() back (VTC:ixp6 first): it did NOT run
  (consistent with INSTALLER_KEEP skipping it, installer_rig.py:1681).
- `Status FULL`: UP-Term:bin/Amarchy, UP-Term:bin/amarchy-flat (started through the assign), and
  Empty-12MB-FFS:UP-Term/bin/amarchy-menu, Empty-12MB-FFS:UP-Term/bin/amarchy-gui (started later, by resolved path).
  Two upgetty, SolidWin, Workbench, wasabid x2. No Shell running a command.
- `ami.py requesters`: "Volume Request" (task << AssignWedge >>, screen "Workbench Screen", Retry|Assign|Deny|Cancel).
- RAM:rig.history absent: the f1host vsh window never ran a command line.
- Host: the only process naming rig 96 is pid 6761 `rig.py logwatch` (started 21:54). logwatch (rig.py:2049) only
  calls paths.cap_fsuae_log every LOGWATCH_EVERY s while mine(): it never talks to the Amiga. Ruled out.

## Ruled out (read)

- BOOTX:go, wasabid-up, solidwin-up, shadows-up (build/rig96/boot): Assign lines are EXISTS checks or adds
  (go:74 `Assign UP-Term: VTC:Apps` only when UP-Term: is missing). No remove form.
- config/upassign.h upassign_ensure: adds only, and only when UP-Term: is missing; it would HEAL the loss for any
  caller (vsh, handler, upconsole) since ENV:up-term/Dir is valid.
- amarchy/*.c: no Assign call or string outside comments; no RemAssignList / AssignLate / AssignPath / RemDosEntry in
  amarchy, handler, shell, device, config, claude, render, prefs, proc, tty.
- installer_rig reboot_amarchy_check post-boot steps: STATUS over ARexx, tabstate.open_host (script = Set HISTFILE /
  VTC:vsh / EndCLI), amarchy_window_rig.drawn_menu_prefs/restore_prefs (prefs files + RELOAD), LAmiga+Space, Esc,
  stock_libs (LIBS: only). None touches UP-Term:.
- rig.py logwatch, retry_wb_reset (exits on any other requester), boot_assigns_missing (reads `Assign LIST`).

## Candidates

1. **A host-side rig script run against rig 96 after 22:02 that removes UP-Term: on its way out** (not visible now:
   no such process left). The removers that do NOT touch LIBS: (so they fit the LIBS: evidence) are the remaining
   suspects: amarchy_menu_rig.py:497 `c.run('Assign UP-Term: "%s"' % upterm if upterm else 'Assign UP-Term: REMOVE')`
   (removes when its saved read of the assign came back empty), histfile_rig, claude_datadir_rig, standalone_rig,
   amigapi_rig, installer_rig :949/:1429. Against: no runner log or verdict row naming rig 96 was found in this hunt
   (not searched: other sessions' scratchpads, verdict stores per script). Next check: `grep -l "rig 96\|RIG=96"` in
   build/*/verdicts and thoughts/shared/research/test-register.md rows after 22:02, and `ls -lt build/rig96/vtc` for
   the X9 / histfile / claude scratch names those scripts leave in VTC:.
2. **Something on the Amiga in the 22:02 window** (Amarchy's boot icon apply, or amarchy-menu / amarchy-gui, which
   are the two processes started after the boot by resolved path, i.e. after the menu key at 22:02). Against: no
   assign-removing code exists in amarchy/ (grep above), so this needs a DOS-level effect (e.g. an Execute of a
   kit script line) that was not found.

## Late check

- No file in build/rig96 (SYS:, VTC:, vtc/ scratch) changed after 22:03 except last-command and the FS-UAE log:
  no rig script left a VTC: scratch file after the check. A script that only sends commands over wasabi (as
  amarchy_menu_rig does) leaves none, so candidate 1 is weakened, not ruled out.
- test-register.md: the only rig 96 row is the s56 owner eye check (WAITING on owner); no script run on rig 96 is
  recorded after it.

## Not done

No fix, no regression test (cause not proven). Rig 96: untouched, requester still open.
