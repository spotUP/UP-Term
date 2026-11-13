# s76 rig smoke (branch patina-rename)

## A: make test, "boot console was not captured"
- Source: tools/test_rig_fsuae_log.py (Makefile row rig_fsuae_log, after rig_screenmode/rig_dopus_os_default/rig_wasabi). It calls the real rig.start() with fakes (status, go_assigns_done, sys_hdf steps) in a temp dir; the check at rig.py:2467 (boot_console_captured, added by 52ed369a, 2026-11-05) reads RIG/boot/boot.console, which the fake never creates.
- Verdict: NOT rename-caused, NOT environment. Stale host test: 52ed369a is an ancestor of the rename base 98c58e32, the base rig.py has the same check (line 2466), the base test has no boot.console reference, and the rename diff of rig.py touches no console/bootcap line (only a docstring).
- Real rig confirms bootcap works: rig 9 boot.console starts with "boot console open"; rig.py start returned rc 0.
- Not fixed (not rename damage). Fix would be: the test writes "boot console open\n" into d/boot/boot.console before rig.start(). Rows after rig_fsuae_log in `make test` did not run.
- Other rig rows run singly and passing: dopus_os_default, wasabi, iprefs_sysreq, boot_console_blind, stale_base, reports_not_in_rig_dir, screennotify, clone_new.

## B: rig 9 smoke (purpose "s76 session: Patina rename smoke + screenshot retake")
- Boot: PASS (up, boot.console captured).
- Finding: the rig's DH0 master image still has the OLD installed kit: at boot, the pre-rename main program, menu and gui binaries under UP-Term:bin ran. Only VTC: has the new patina-* binaries. A real check of the renamed kit needs the new kit installed (install_rig.py / installer_rig.py on build/dist/up-term) or a refreshed master image.
- Patina running: PASS after killing the pre-rename tasks and starting VTC:Patina, patina-menu, patina-gui by hand (Status lists VTC:Patina).
- ARexx PATINA: PASS. `RX "options results; address PATINA; 'STATUS'"` -> rc=0 result=`hits=0 rows=71 enabled=1 active=0 wins=0 snaps=0 bar=1 wbpens=0 wbrefused=0 shots=0 notes=0 ticks=66`. A nonexistent port gives rc 10/13, so the PATINA port exists.
- Screenshots: NOT retaken (out of tool budget). Rig screen is 800x600 like the originals. DOpus.1 opened a lister (RX DOPUS.1 "lister new" returned a handle); setting its path needs the right ARexx syntax (see tools/rig/patina_dopus_rig.py).

## C: screenshot retake (rig 9, purpose "s76 session: Patina rename smoke + screenshot retake")
- Rig 9 started fresh; VTC: carries the renamed kit. The rig's DH0 copy still had the pre-rename files, so on rig 9's own copy only: Break of the old tasks, Delete of the pre-rename drawer and bin files, VTC:Patina started, `Assign UP-Term:` re-added (it was unassigned). Master image untouched.
- dopus-lister.png: PASS. DOpus lister of Rushhours:Tools/UP-Term (`lister read <h> <path>` and `lister set <h> position 120 70 560 440` through DOPUS.1) shows a Patina drawer, no old-name entries.
- patina-menu.png: PASS. Drawn menu opened with Left Amiga + Space on the Workbench; title reads "Patina".
- terminal-unix.png: PASS with a caveat. UP-Term vsh window shows uname and the first rows of `ls bin`; the window is the default 600x200 (size-gadget drag did not resize it), smaller than the old shot. No old-name text visible.
