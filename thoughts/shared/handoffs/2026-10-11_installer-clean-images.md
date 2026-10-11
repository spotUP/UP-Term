---
date: 2026-10-11
topic: Installer, Uninstall and the clean-image rigs
tags: [installer, rig, uninstall, handoff]
status: draft
---
# Handoff: installer and clean-image rigs

## Task
Verify the kit (1.10 of the unix tool ports plan) on rig 1, fix what fails, tick port plan items 1.2-1.10 and 2.1-2.5, then copy the kit to ~/Desktop/UP-Term-<sha>.lha. The owner added: every rig boots a clean installed Workbench (rig.py --os), the Installer must not fill the 12 MB system partition, Uninstall keeps UP-Term:var.

## Critical references
- tools/rig/rig.py: --os, os_image_copy, boot_assigns_missing, Work: drive; paths.py (OS_IMAGES, ROM314, LIB68060/68040, INSTALLER; inputs in build/rig-inputs/)
- tools/rig/installer_rig.py (cases default dest update novice space; CLEAN -> Work:Apps/UP-Term), tools/rig/install_rig.py (--dest --move --var --show-check)
- dist/Install.installer room check (~line 305), dist/Uninstall (var question, .none markers), dist/install.dos (.none markers)

## Recent changes (all committed except the patch below)
cdcbec3 var + README, 19f4c95 / c4b7f54 room check and ixemul fix, cfdbc89 + 772b964 rig --os, 7bd94a0 SIGTERM fixtures, ed37b2c fixture. Uncommitted: thoughts/spot/notes/2026-10-11-installer-stop.patch (install_rig --var uses Work: on clean images; untested).

## Learnings
- Clean 3.1/3.2.2 hdf = plain FFS volume, 12 MB, no RDB; xdftool writes into it; hooks go in S/User-Startup (3.2.2's file ends without a newline).
- 68060 needs Phase5 68060.library and the 692-byte 68040.library stub, else Software Failure 8000 888B at boot.
- Clean 3.0/3.1 have no Installer; 43.3 is used (build/rig-inputs/Installer).
- Killed runs (SIGTERM) left an UP-Term block in S:User-Startup: now handled.
- Results on builds: ixemul 06baad5 build295, vtcon HEAD at each run (kit4 = c4b7f54 plus other agents' uncommitted claude/ files in the tree). install_rig pass 2 99/99 and pass 3 38/38 and installer default 37/37, update 45/45 ran on the old 3.1 system (kit before the Uninstall/room changes).

## Next steps
1. UPTERM_RIG=1 UPTERM_KIT=<kit> UPTERM_RIG_FLAGS="--os 322 --max" UPTERM_RIG_SHOW="..." python3 -I tools/rig/install_rig.py --var (apply the patch first; rig up via rig.py start --os 322 --max).
2. Same on --os 31. DV4 STANDARD 65608 vs 65536 on clean 3.1 (devverify_rig.py, rig 3): find the cause.
3. Tick the port plan (evidence: install_rig PORTS 16/16 in passes 2 and 3).
4. make test, make dist from a clean tree, copy kit to ~/Desktop/UP-Term-<sha>.lha.
