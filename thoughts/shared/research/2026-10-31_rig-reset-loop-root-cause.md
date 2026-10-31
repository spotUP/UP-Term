# RIG31-RESET-LOOP root-cause hunt (vtcon-05 s38, 2026-10-31, rig 4, HEAD 26107a53)

Verdict: guest crash NOT reproduced; first-fault evidence was being destroyed by the watchdog restart (fixed).

## Why the loop happens (already proven, not re-fixed)
A guest crash (any CPU exception, or a plain exec dead-end alert) makes exec call the ROM ColdReboot, which keeps the
68060.library's live CPU state (CACR, TTx, TC, PCR); every following ROM boot dies in the early alert 80000006 and
resets ~8/s (term-findings/rig9-installer-loop.md result 8; `crashme coldrom` reproduces it with no crash at all).
That is a 060 + KS 3.1 ROM + emulator-reset property, not repo code; the host watchdog restart is the remedy. The
reset loop is a symptom of whichever guest crash comes first.

## Today's rig 5 loops (19:44:33, 19:48:57)
No first fault recoverable: build/rig5/logs/fs-uae.log.txt and .tail at 20:00 hold only the clean boot after the
19:56 restart (0 Illegal/Exception, 0 CPU reset lines). Cause: `start` runs paths.cap_fsuae_log(limit=0), which
truncates the live log and keeps the last 1 MB as .tail; the next start overwrites the .tail. The watchdog's
restart therefore destroyed the evidence of every loop it recovered.

## Older first faults (scratchpad 11e17e88 copies, 29 Oct)
- rig5-fs-uae.log.txt: RTG 800x600 -> NTSC switch (BEAMCON0 0020->0000 PC=10031696), three "ignoring file ...
  (cannot be represented in ISO-8859-1)" lines (VTC:Songs scanned), SetSwitch() - amiga (RTG screen closed), then
  "Illegal instruction: 000c at 10000A14 -> 00F80AD2", "SERIAL: ... baud=9600 PC=f8361c" (exec alert path), CPU reset.
  10000A14 is low Z3 fast RAM just below the previous boot's supervisor stack (ISP 1000227C in result 8): a wild
  jump into data, not code of a loaded module.
- rig5-v4-fs-uae.log.txt: same RTG -> NTSC switch, then CPU reset PC=f80dc8 with NO exception line (dead-end alert).
- rig31-loop.md: "Exception 3 (9c) at 9c -> 0", then "Illegal instruction: 1867 at 00001956": jump into low memory.
Three different PCs, all wild jumps (data / low memory): a stale function pointer (interrupt server, input handler,
hook) or memory corruption, not one buggy instruction. The ISO-8859-1 lines touch the other agent's FS-UAE-exit
finding (same Songs files scanned right before the 29 Oct crash); coincidence not ruled out.

## Repro attempts on rig 4 (3.1, --max, --idle-minutes 0, fresh)
wasabid: BOOTX:wasabid (running, build/rig4/boot/wasabid) md5 ca3950f41b9f13580e618509cfed1ee5 = build/wasabi/wasabid
today; C:wasabid on sys.hdf 3df49aa0c071344a82ea7dce97ed0902; build/rig4/vtc/wasabid cbb93a92191c9c366eb4ef56c31b5064.
Three wasabid binaries on one rig; the running one is the BOOTX: copy.
1. 3 cycles: ami.py open (UP-Term), type nano, Ctrl-X, type exit: 0 CPU resets, 0 faults, ping OK each time.
2. upkg_rig.py --only updatelist --force: passed 6 of 6, 0 resets.
3. 3 more nano/exit cycles after the upkg run: 0 resets, 0 faults.
Caveat: wasabi ps showed no separate nano process while nano was typed (ixemul runs inside vsh's CLI process, so
this is expected but not proven that nano started); vsh exited each time (window closed).

## Fix (tooling, where the evidence was lost)
rig.py save_reset_loop_log(): recover_reset_loop copies the live FS-UAE log to
build/rig<N>/reset-loop-<stamp>.fs-uae.log BEFORE stop(), keeps the newest 3, and writes the first fault
(paths.fsuae_crash, or "no CPU exception logged (ROM reset)") into watchdog.log. Regression test
tests/test_rig_watchdog_reset_loop.py::test_recovery_keeps_the_first_fault_of_the_looping_run_before_the_restart:
fails on HEAD rig.py, passes with the fix.

## Next (when it recurs)
Read build/rig<N>/reset-loop-*.fs-uae.log: first Illegal/Exception line and the 30 lines before it. For a PC in Z3
RAM, map it with wasabi ps/seglists on a second boot; for the RTG -> NTSC -> SetSwitch amiga sequence, try closing an
RTG screen (Palette/ScreenMode prefs USE, Amarchy theme switch) as the trigger.
