# FS-UAE log cap (2026-10-17)

Problem: all rig FS-UAE processes wrote ~/Documents/FS-UAE/Cache/Logs/fs-uae.log.txt, uncapped; 7.9 GB, disk full.

## FS-UAE options (strings of /Applications/FS-UAE.app/Contents/MacOS/fs-uae)
- logs_dir (also logs-dir): directory of the log; file name fs-uae.log.txt inside. Default Documents/FS-UAE/Cache/Logs.
- base_dir, log_flush (flush after every line), log_input, log_bsdsocket, log_illegal_mem, log_autoscale, uae_log.
- Not measured (no rig may run): whether FS-UAE truncates its log at start. Unverified; the cap code does not rely on it.
  The empty file found after the incident only shows the owner truncated it.

## rig.py findings
- Verbose options set by rig.py: none. No log_*, no uae debug flags. bsdsocket_library = 1 only (the library, not log_bsdsocket).
- serial_port = build/<rig>/serial.log is a separate file, also uncapped (kprintf traces). Not changed; consider a cap if it grows.
- The log's content is lost, so what filled it is unknown. Candidate: a repeated warning line.

## Changed
- paths.py: FSUAE_LOG_DIR (build/rig*/logs), FSUAE_LOG_MAX (50 MB), FSUAE_LOG_TAIL (1 MB), cap_fsuae_log().
- rig.py: config line `logs_dir = <rig>/logs`; start() cuts any old log (tail kept as <name>.tail).
- ami.py req(): calls cap_fsuae_log() (one stat per call; every rig script and the MCP pass through it), so a running rig is capped in place.
  Sized by allocated blocks so a non-append writer's sparse file does not count.
- Gap: a rig nobody talks to is not capped. Only the rig tooling's calls trigger the check.

## Test
tools/test_rig_fsuae_log.py (`make test ONLY=rig_fsuae_log`): log dir per rig 1-3 inside the rig dir, rig.py source carries logs_dir
and no log_* option, cap on fake logs (cut, tail kept, under-limit untouched, sparse file by allocated size). No FS-UAE run.
The config generation itself is checked on the source text, not by running setup().

## Follow-up: watcher while the rig runs
The ami.req cap misses a rig nobody talks to. `rig.py start` now spawns `rig.py logwatch` (detached, one per
rig via flock on logs/.logwatch.lock). It caps the FS-UAE logs and serial.log every 30 s until the rig's
FS-UAE process is gone. Test: tools/test_rig_fsuae_log.py drives `rig.start()` (stubbed emulator) and asserts
exactly one logwatch spawn; fails when the spawn is removed (measured).
