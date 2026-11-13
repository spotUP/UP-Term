# FP-3 marker recovery on AmigaOS 3.2 (rig 24), s29

## Why Patina does not start on a 3.2 rig after reboot

Not a Patina bug: nothing launches it. `rig.py start --os 32` boots the clean 3.2 image tree
(build/rig24/os-32) with only OS_HOOKS appended to S:User-Startup:

    Assign >NIL: VTC: VTCX:
    Run >NIL: C:Execute BOOTX:go

No kit is installed there, so there is no `Run >NIL: UP-Term:bin/Patina` line (the 3.1 sys.hdf kit block has it),
SYS:WBStartup has no Patina, and BOOTX:go only shadows/copies Patina binaries, never runs them.
patina_desk_rig.py's header already says "the 3.2 kit does not auto-start it"; Patina runs on a 3.2 rig only
after `patina_gui_rig.py --setup` (Run >NIL: VTCX:Apps/bin/Patina), which a reboot undoes.

Live evidence, rig 24 fresh boot from ac0c8561 build:
- Status: wasabid, Status, Workbench, wasabid. No Patina.
- Search S:User-Startup Patina: no hit. List SYS:WBStartup: RAWBInfo, MenuTools, DefIcons, AutoArrangeIcons, AsyncWB, AssignWedge.
- Assign UP-Term: EXISTS -> VTCX:Apps (from go). Assign LIBS: EXISTS fine.
- After --setup: md5 of VTCX:Apps/bin/{Patina,patina-menu,patina-gui} == build/amiga; port PATINA answers.
  So the binary starts and runs normally; no crash, Stack probe not needed.

Earlier run: phase b waited 120 s for a Patina process that no boot path starts; the "a3" retry ran phase a
after a reboot without --setup, so d.pick() found no menu ("open menu: no stable state within 60s").

## Fix (rig-script gap), branch s29-fp3-os32, worktree ../vtcon-wt-fp3os32
- patina_gui_rig.py: setup()'s tail (Run Patina + wait for port) is start_patina(tag), reused.
- patina_marker_rig.py: header names the --setup prerequisite; phase b waits 30 s for a boot-started Patina,
  else starts VTCX:Apps/bin/Patina itself (as a kit User-Startup would) and then checks the marker recovery.

## Rerun result
(see below)

## Rerun 2 (worktree build e9f11171, fixed exists())
- a: [FAIL] marker ENVARC:Patina/pending-revert written at confirm start; [OK] apply changed ENV:Sys (pointer.prefs md5 changed; palette 4db04406.. and wbpattern 94455cd1.. identical pre/applied).
- After reboot, before Patina start: List ENVARC:Patina ALL = desk-original/, prefs.orig, prefs only. No pending-revert, no pending-revert.d.
- So the marker is never written on 3.2: dk_marker_write runs only from dk_confirm_ask, called from desk_dos_apply only when r > 0 (desk_dos.c:981). Likely r <= 0 for vantablack->white on 3.2 (palette/wbpattern unchanged; pointer change comes from dk_orig_off/flat path, which does not count in r). UNVERIFIED: r value not measured; next probe = log r / confirm_asks sentinel during phase a.
- Phase b not run (no marker to recover). Rig 24 stopped.
- Branch s29-fp3-os32 commit e9f11171 (unpushed): sentinel moved into desk_dos_marker_recover, exists() uses rc, b starts Patina itself.
