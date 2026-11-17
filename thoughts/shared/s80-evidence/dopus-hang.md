# DOpus boot hang (Opus 5 loading stops at 55%, IPrefs requester) -- s81 hunt, NOT reproduced

Status: cause by code reading only. Rig repro NOT done (rig 7: `rig.py dopus --os 32` spent ~10 min cloning/booting the install rig on a loaded machine; I stopped it at the call budget). Everything below marked [read] is from source, not measured.

## Boot order [read]
1. S:Startup-Sequence -> S:User-Startup: the Installer's ";BEGIN DOpus5" block (tools/rig/rig.py os_tree_dopus: `Resident LoadWB DOpus5:C/LoadDB`). No Patina line (dist/install.dos:694-717 removes the old User-Startup Patina blocks).
2. Startup-Sequence by-name `LoadWB` = DOpus LoadDB: opens the Workbench screen, splash "Opus 5 loading... please wait." (a window on that screen), then runs SYS:WBStartup.
3. SYS:WBStartup/Patina.info (dist/install.dos Leg W) starts Patina during or right after step 2. patina-flat starts from Startup-Sequence before IPrefs.

## Which write triggers the reset [read]
patina/patina.c:285 desk_start_gate -> desk_dos_start_apply (patina/desk_dos.c:1720). Gate (desk_start_due, desk_core.c:1538) needs only: WB screen lockable + tick ok + desk_dos_wb_ready. With DOpus as Workbench the screen exists as soon as LoadDB opens it, i.e. while the splash is up.
On a fresh install ENV: has no Sys prefs, so desk_start_plan plans palette (+ pointer, wbpattern, font): desk_dos.c:1740-1800 writes them via dk_put_palette / dk_put (IPrefs-watched) -> IPrefs CloseWorkBench.
The start path never calls dk_foreign_windows(). Only Patina's own windows are closed (dk_windows_close, desk_dos.c:~1786). The s80 IPREFS deferral (desk_defer_map, dk_apply ~516) exists only in dk_apply (theme pick), not in desk_dos_start_apply.

## Which window blocks [read + s80 evidence]
The DOpus splash (and any lister) is a foreign window on the Workbench screen. thoughts/shared/s80-evidence/iprefs-case2.md proved on a 3.2 rig: with any foreign window open, ANY IPrefs-watched stock-editor USE (identical bytes included) raises the Retry/Cancel requester. DOpus's splash window sits in that role; the requester appears over the splash, the splash waits (55%) for the screen/WBStartup to settle. Note desk_window_foreign (WFLG_WBENCHWINDOW excluded) may not count DOpus windows at all: open owner question in the s80 handoff.

## Reproduces on main / with s81-blockpen: UNKNOWN (not run)
s81-blockpen (a2191d04, build/amiga/Patina 17:43 already built) fixes desk_pens_reset always true. That only avoids dk_wait_reset's wait and the pen-map defer; it does NOT stop the writes themselves (iprefs-case2: requester comes from the write, not the pens). Prediction [unverified]: still hangs.

## Proposed fix [read]
Root cause layer: patina/desk_dos.c desk_dos_start_apply (~1720-1830): before the plan writes, if dk_foreign_windows() > 0 (and the DOpus splash counts as foreign), drop PALETTE/WBPATTERN/POINTER/FONT from the plan, remember the start as pending, and re-run desk_start_gate on later ticks (patina.c g_desk_started stays 0 until no foreign window). Also gate desk_dos_wb_ready for DOpus: wait for the DOpus splash to close (title/port) before starting. Unit test: desk_core.c desk_start_plan helper gating by foreign count (fail-before, as s80 proposed for dk_apply). Same fix as iprefs-case2 "no IPrefs-watched write while foreign windows" -- one helper for both paths.
Not one obvious line: no branch created.

## Next step to verify (20-30 min of rig)
UPTERM_RIG=<free> rig.py dopus --os 32 (expect ~10 min start on a busy machine), put build/amiga/Patina plus a WBStartup Patina.info into build/rig<n>/os-32, reboot, read PATINA_FOREIGN / PATINA_DESK_START_PLAN sentinels and Intuition requester by text. Repeat with vtcon-s81-blockpen/build/amiga/Patina.

## Cleanup
Rig 7 stopped (ps confirms none). Deleted build/rig-saved/7/20261114-181025 (18.7 MB) and build/rig7. df: 2.7 GB free (other sessions' use).
