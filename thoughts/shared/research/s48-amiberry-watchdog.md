# Amiberry watchdog / crash detection (s48)

Verdict: gap. paths.fsuae_reset_count and paths.fsuae_crash used line.startswith("CPU reset" / "Illegal instruction" /
"Exception "). Amiberry's macOS console prefixes every line "2026-11-01 21:31:27.607 Amiberry[4853:104620118] " (seen in
build/rig30/logs/fs-uae.log.txt), so nothing ever matched: the reset-loop recovery in rig.py watchdog never fired and
fsuae_crash (detach_rig, recover_reset_loop) reported nothing. Also Amiberry logs "CPU reset" only outside compatible
mode, so a JIT/non-compatible rig differs from a compatible one.

Amiberry strings (checkout /Users/spot/Code/Emulators/amiberry @ 6e3ca06, src/):
- newcpu.cpp:8446  "CPU reset PC=%x (%s)..\n" (cpureset(), only when !cpu_compatible and !cycle_exact path)
- custom.cpp:6632  "Reset at %08X. Chipset mask = %08X\n" (custom_reset(), every reset; 1 in a fresh rig30 boot log)
- newcpu.cpp:3999  "Illegal instruction: %04x at %08X -> %08X\n"
- newcpu.cpp:3334, 3365, 2812  "Exception %d (%x) at %x -> %x!\n" (bus/address error frames; double fault -> cpu_halt)
- newcpu.cpp:6137  "CPU halted: reason = %d PC=%08x\n" (cpu_halt, reasons incl. CPU_HALT_DOUBLE_FAULT)

Fix: tools/rig/paths.py uae_log_line / uae_log_is_reset / uae_log_is_exception, used by both functions (no second parser).
"Reset at" counts only on prefixed Amiberry lines (FS-UAE may log it too and would double count).
Tests: tests/test_rig_watchdog_reset_loop.py class AmiberryLog (4 tests, real line format).
