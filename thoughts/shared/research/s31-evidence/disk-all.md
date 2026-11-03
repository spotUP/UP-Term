# RL11 AM-ICONS-DISK-ALL measurement (rig 73, OS 3.2, 2026-11-02) - PARTIAL, no code fix yet

Volumes: Empty-12MB-FFS (SYS:), RAM Disk, VTCX, BOOTX, Work. No network/host volume on the rig.
- Empty-12MB-FFS: Disk.info written.
- RAM Disk: Disk.info is a SOFT LINK to ENVARC:Sys/def_RAM.info. list_dir treats fib_DirEntryType>0 (ST_SOFTLINK=3) as a directory, recurses into Disk.info/, Lock fails: note 'skip io (no lock)', counted skipped-io. Image still Amarchy through def_RAM.info (1328 bytes written). Fix (not done): list_dir skips soft links.
- VTCX, BOOTX, Work: no Disk.info, fall to ENV:Sys/def_disk.info (written by APPLY, 1214 bytes).
Counters: written=185 skipped-done=38 skipped-nodefault=15 skipped-io=7 fallback=126.

Pixels: screenshots before and after APPLY are identical (diff bbox None): Workbench did not redraw live. Post-restart colours UNMEASURED. Shots: ~/Desktop/upterm-shots/s31-rl11-before-apply.png, s31-rl11-after-apply.png.
Rig note: Assign UP-Term: LIST deleted the assign (re-added VTCX:Apps); lucide.dat needed make lucide-dat + put.
Open: network icon mechanism (no type:net entry, no classifier), softlink fix + test, restart-Workbench pixel proof.
