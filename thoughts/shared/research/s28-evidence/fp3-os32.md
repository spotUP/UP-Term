# FP-3 marker recovery on AmigaOS 3.2 (rig 24), s29

## Why Amarchy does not start on a 3.2 rig after reboot

Not an Amarchy bug: nothing launches it. `rig.py start --os 32` boots the clean 3.2 image tree
(build/rig24/os-32) with only OS_HOOKS appended to S:User-Startup:

    Assign >NIL: VTC: VTCX:
    Run >NIL: C:Execute BOOTX:go

No kit is installed there, so there is no `Run >NIL: UP-Term:bin/Amarchy` line (the 3.1 sys.hdf kit block has it),
SYS:WBStartup has no Amarchy, and BOOTX:go only shadows/copies Amarchy binaries, never runs them.
amarchy_desk_rig.py's header already says "the 3.2 kit does not auto-start it"; Amarchy runs on a 3.2 rig only
after `amarchy_gui_rig.py --setup` (Run >NIL: VTCX:Apps/bin/Amarchy), which a reboot undoes.

Live evidence, rig 24 fresh boot from ac0c8561 build:
- Status: wasabid, Status, Workbench, wasabid. No Amarchy.
- Search S:User-Startup Amarchy: no hit. List SYS:WBStartup: RAWBInfo, MenuTools, DefIcons, AutoArrangeIcons, AsyncWB, AssignWedge.
- Assign UP-Term: EXISTS -> VTCX:Apps (from go). Assign LIBS: EXISTS fine.
- After --setup: md5 of VTCX:Apps/bin/{Amarchy,amarchy-menu,amarchy-gui} == build/amiga; port AMARCHY answers.
  So the binary starts and runs normally; no crash, Stack probe not needed.

Earlier run: phase b waited 120 s for an Amarchy process that no boot path starts; the "a3" retry ran phase a
after a reboot without --setup, so d.pick() found no menu ("open menu: no stable state within 60s").

## Fix (rig-script gap), branch s29-fp3-os32, worktree ../vtcon-wt-fp3os32
- amarchy_gui_rig.py: setup()'s tail (Run Amarchy + wait for port) is start_amarchy(tag), reused.
- amarchy_marker_rig.py: header names the --setup prerequisite; phase b waits 30 s for a boot-started Amarchy,
  else starts VTCX:Apps/bin/Amarchy itself (as a kit User-Startup would) and then checks the marker recovery.

## Rerun result
(see below)

## Rerun 2 (worktree build e9f11171, fixed exists())
- a: [FAIL] marker ENVARC:Amarchy/pending-revert written at confirm start; [OK] apply changed ENV:Sys (pointer.prefs md5 changed; palette 4db04406.. and wbpattern 94455cd1.. identical pre/applied).
- After reboot, before Amarchy start: List ENVARC:Amarchy ALL = desk-original/, prefs.orig, prefs only. No pending-revert, no pending-revert.d.
- So the marker is never written on 3.2: dk_marker_write runs only from dk_confirm_ask, called from desk_dos_apply only when r > 0 (desk_dos.c:981). Likely r <= 0 for vantablack->white on 3.2 (palette/wbpattern unchanged; pointer change comes from dk_orig_off/flat path, which does not count in r). UNVERIFIED: r value not measured; next probe = log r / confirm_asks sentinel during phase a.
- Phase b not run (no marker to recover). Rig 24 stopped.
- Branch s29-fp3-os32 commit e9f11171 (unpushed): sentinel moved into desk_dos_marker_recover, exists() uses rc, b starts Amarchy itself.
