---
date: 2026-10-31
session: vtcon-05 s38
status: handoff
---

# vtcon-05 session 38: reset-loop log capture, nvim recheck, RJ7 cancel closed

## Done
- RIG-AMITCP-UNSTAGED closed (66a0da8a); AmiTCP residue deleted from rig vtc folders.
- RJ7 Esc cancel passes on current ixemul a8a7bf4c, 4/4 (f4508f70). Ledger rows RJ7-A3-RERUN, RJ7-CANCEL-STILL-SLOW, RJ7-DRAWN-UNRUN ticked.
- RIG31-RESET-LOOP: not reproduced (6 cycles, rig 4, 3.1 060). Root cause of the guest crash still open. Fixed why it was never visible: watchdog restart truncated the FS-UAE log. 8f1e504f saves build/rig<N>/reset-loop-<stamp>.fs-uae.log (newest 3) and writes the first fault into watchdog.log; test tests/test_rig_watchdog_reset_loop.py. Evidence thoughts/shared/research/2026-10-31_rig-reset-loop-root-cause.md.
- NVIM-HEAP-CORRUPT recheck: STILL FAILS 7/7 normal config on a fresh rig; -u NONE passes. Songs drawer ruled out (A/B, the ISO-8859-1 line is a harmless write_log). FS-UAE exits with no crash report, triggered from the guest. Evidence thoughts/shared/research/2026-10-31_nvim-heap-recheck-fresh-rig.md (d0ab7a19).
- UPKG-CURL-SIGNAL-HINT cause row ticked as a duplicate of the fixed row.

## Next (ledger rows, grep thoughts/spot/todos.md)
1. RIG31-RESET-LOOP: on the next loop, read the first Illegal/Exception line from reset-loop-*.fs-uae.log. Trigger to try: RTG screen close (Palette or ScreenMode USE, Amarchy theme switch). Side: rig holds 3 wasabid binaries (BOOTX: ca3950f4 = build, C:wasabid 3df49aa0, vtc/wasabid cbb93a92).
2. NVIM-HEAP-CORRUPT: find the FS-UAE exit path for a guest-triggered quit (fs-uae source at ~/Code/Emulators/fs-uae; grep exit/uae_quit callers), and the first writer to _SysBase. Quiet host only.
3. RJ7-DRAWN-MENU-NOOPEN (new): drawn menu did not open after earlier runs on the same boot, twice.
4. Remaining: NVIM-PERF, AMARCHY-LAUNCH-FIELD7 (waits up-uhc 1.4), RIG-BATCH-DAILY, upterm-ports mk/common.mk:123 `seq 1 0` on macOS.

## State
- No rigs of this session running; build/rig4, rig5, rig6 deleted. Rigs 1, 2, 8 belong to other sessions.
- Disk 29 Gi free.
