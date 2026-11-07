---
date: 2026-11-07
topic: A2-FIX rig.py dopus defaults to the rig's start system
tags: [rig, dopus, evidence]
status: done
---
- Start args record: tools/rig/rig.py START_ARGS = RIG/"start.args" (line 101), written by start() (line ~2323, JSON list of argv[2:]). It was written but never read. New reader rig.start_system_args() (before debugger_lines); dopus_rig.system_args() uses it when argv names no system; main() prints "[DOPUS] target: ..." first.
- Missing block on 5.82 (sys.hdf clone), measured/static: the clone is started with --no-dopus, which strips the master's ';BEGIN DirectoryOpus 5' block and Opus5 (rig.py sys_hdf_no_dopus/remove_opus5), so the earlier run's S:User-Startup copy held no DOpus line at all (evidence rig96-dopus-a2.md). The brief's hypothesis (Installer took an update path because of the old block) is therefore wrong. In the Installer script "Install DOpus5" the block is written by `(startup "DOpus5" ...)` whenever #assign is true, and #assign is 1 for mode 2 (Instead of Workbench), so the script itself would write it. Why the 3.1 sys.hdf run did not: NOT measured (reproducing needs the forbidden sys.hdf-clone rerun). The command now exits 1 naming the system: "no ';BEGIN DOpus5' block in S:User-Startup of the <target> after the install".
- Test tools/test_rig_dopus_os_default.py (not wired into the Makefile: another agent owns it). Before fix (dopus_rig.py reverted): "FAIL: record --os 32, argv names none: []" then AttributeError on target, rc=1. After: rc=0.
- Rig run (UPTERM_RIG=96): start --max --os 32 --keep, stop --keep, then `rig.py dopus` with no system: "[DOPUS] target: os-32 tree build/rig96/os-32", rc 0, no reinstall (stamp already present in the kept vtc/; "already installed ... --force installs again"). os-32/S/User-Startup holds 5 DOpus lines. Rig down afterwards.
- stop_command() (rig.py ~2845): --keep skips clear() only via "--keep" in sys.argv[2:]; confirmed by reading and by this run: vtc/ (59 entries) survived `stop --keep`. Stray vtc/dopus5-us.tmp.uaem removed.
- Scratch: scratchpad/a2fix (unpacked DOpus archive, logs) removed at the end.
