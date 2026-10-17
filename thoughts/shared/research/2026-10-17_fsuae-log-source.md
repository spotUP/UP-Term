# FS-UAE log flood source (2026-10-17)

Evidence: the 7.9 GB log is gone; build/rig/logs/fs-uae.log.txt (635 KB, after the 50 MB cap work) survives. Line shapes, digits folded:

- 6222 `need_metadata_file 6 WF_ALWAYS WF_EXISTS`, 1533 `need_metadata_file 2 WF_ALWAYS`
- 6069 `- writing mode ----rwed`, 1409 `----rw-d`, 276 `----r---`
- 3327 `- writing comment `
- everything else: under 60 each (bsdsocket thread starts, fs_image_destroy).

Source: src/od-fs/fsdb_host.cpp fsdb_set_file_info in the FS-UAE tree. It runs on every Amiga set-protection, filenote and set-date on a host drawer (hard_drive_N) and calls write_log unconditionally (no log_* option gates it), 2-3 lines per call. It runs for default bits because uaem_write_flags defaults to 1 (WF_ALWAYS = always write a .uaem). A copy/install of many files therefore logs and writes metadata per file.

Option: `uaem_write_flags`. The rig config now sets `nhsparwed` (tools/rig/rig.py): .uaem is still written for notes, inexact times and any non-default bit; the default-bit writes and their log lines go. Nothing in tools/ or tests/ reads default-bit .uaem files (grep .uaem). No log_* option was ever set.

Measurement, rig 3, same workload (5x Protect rwed + 5x Filenote on VTC:, then stop): metadata log lines 18 before, 13 after; the 13 are the Filenote calls (a note is real metadata, still logged). Protect-only calls are silent now. A bulk copy of files with default bits no longer logs. Log bytes per minute were not separable at this workload size (the log is block-buffered, 54.8 KB vs 54.7 KB incl. startup noise); the cap (50 MB) and watcher remain as backstop.

Residual: filenote/non-default-bit calls still log; silencing them needs a change to the FS-UAE source (write_log unconditional), not a config option.

Test: tools/test_rig_fsuae_log.py checks the generated config line.
