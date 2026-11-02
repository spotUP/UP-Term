# RL10: filled Phosphor icon set, plan F11, on OS 3.1 (rig 90)

Question: does the filled icon set APPLY and RESTORE correctly on OS 3.1, as on the 3.2 run (filled-f11-32.md)?

Verdict: APPLY and RESTORE PASS (byte-identical 533 of 533, differing 0). Pens-held 16 and the pen 33..47 pixel check are UNVERIFIED (not measured, see below). One coverage limit found: the 512-entry manifest cap.

## Method
Rig 90, fs-uae, 060 max, OS 3.1. Volumes walked: SYS:, VTCX:, BOOTX:, RAM:. `tools/rig/amarchy_icons_restore_rig.py --vols SYS:,VTCX:,BOOTX:,RAM:`.
Rig also has Rushhours: (DH1, 1.4 GB HDF, more than 1000 icons). UP-Term: assigns to Rushhours:Tools/UP-Term, which had no Amarchy/Icons, so `MakeDir UP-Term:Amarchy/Icons` was needed before the script put lucide.dat (121792 bytes, newer than the 119436 bytes of the 3.2 run).

## Counters
- 533 .info files listed and md5-summed before.
- APPLY rc 0: written=512 skipped-done=35 skipped-bad=108 skipped-nodat=0 skipped-nodefault=18 skipped-io=1015 skipped-readonly/locked/uaem/deep=0 fallback=1486 net=0. 30 of the 533 listed files changed.
- RESTORE rc 0: restored=512 restore-failed=0.
- PASS: byte-identical 533 of 533, differing 0.
- A rerun of APPLY under `Stack 262144`: no Guru, no crash, same counters; restored clean (512).
- Disk.info: Rushhours written; System present, not written; Ram Disk, VTCX:, BOOTX: missing, show def_disk. ENVARC:Sys def_drawer.info (1716 bytes) and def_disk.info (1740 bytes) written.

## Finding: manifest cap limits coverage
skipped-io=1015 equals the manifest limit `IA_MAXENT 512` (amarchy/iconapply_core.h line 27; iconapply_core.c line 253: no manifest line, no write, so RESTORE covers every write). Rushhours (volume order Ram, VTCX, Rushhours, System, BOOTX) fills the manifest, so most System/VTCX/BOOTX icons are skipped on this rig. Correct by design (no unrestorable write), but the filled set reaches only 512 files per run. The 3.2 rig had no Rushhours, so its 254 writes fit. Not fixed: a manifest capacity change is a design item for the owner.
skipped-bad=108 are Rushhours *.nail.info style files; skipped-nodefault=18 are the same table-path rows as on 3.2.

## Not measured (UNVERIFIED)
- `amarchy-flat STATUS` pens-held 16: an earlier plain `RAM:amarchy-icons STATUS` printed "pens-held 0 missing 8..15 refs=2" (flat not running). The flat/flatwin flow was not completed.
- Pen 33..47 pixel check: the screenshot ~/Desktop/upterm-shots/s31-rl10-31.png (800x600) was taken on a 24-bit Directory Opus screen with flat not running, so palette pens 33..47 do not apply. Not analysed.
- `rig.py aga256` failed (it re-invoked `rig.py start`, exit 1) and the rig then died (connection refused; build/rig90 gone) before the final flat flow and a last RESTORE. The rig was in APPLIED state, but its folder is gone, so nothing persists.

## Tooling changes
- tools/rig/amarchy_icons_restore_rig.py: RUN_SECS = 1500 for the APPLY/RESTORE runs. The 300 s limit killed the first run: APPLY walks Rushhours for over five minutes.
- `make dist` failed in the worktree with "upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports" (sibling repo, not touched). md5sum was copied from the main checkout's build/dist.
