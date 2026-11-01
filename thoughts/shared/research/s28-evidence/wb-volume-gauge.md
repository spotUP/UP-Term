---
title: s29 WB-VOLUME-GAUGE-OFF - the 3.2 volume gauge is a Workbench.prefs field
status: field-measured
date: 2026-11-01
---

# Field

NDK 3.2 prefs/workbench.h: struct WorkbenchExtendedPrefs (WBNC chunk body) has `BOOL wbe_DisableVolumeGauge` after the 30-byte WorkbenchPrefs
(IconMemoryType ULONG at 30, LockPens 34, DisableTitleBar 36, DisableVolumeGauge 38, BOOL = WORD).

Measured on rig 12 (3.2, `--os 32`): SYS:Prefs/Workbench (editor toggle "No Volume Gauge", gadget id 20), Save with it off, then on,
`cmp -l` of the two 84-byte ENVARC:Sys/Workbench.prefs files: exactly one byte differs, byte 74 (1-based) = file offset 73 = 26 (FORM/PRHD header)
+ 8 (WBNC chunk header) + 39, the low byte of the word at body offset 38: 0 -> 1. Nothing else changes.

# Live switch

None found: WBCTRLA_ tags in the vendored NDK workbench/workbench.h (grep) cover drawer/icon lists, stack size, hooks, global flags; nothing for the gauge.
The switch is the prefs file; the editor's USE (same route as AM-ICON-NOBORDER) tells workbench.library.

# Stock-file bug found on the way

`wbnc_stock[]` in `amarchy/desk_core.c` (the 84-byte file the border code creates when no Workbench.prefs exists) had 0x32 (TitleUpdateDelay 50 ms) at WBNC body index 39. The editor-saved file has it at index 41 (UWORD at 40). So the stock file read as DisableVolumeGauge = 0x32, gauge off. Fixed: two zero bytes added so 0x32 sits at index 41. Without the fix the new test failed 9 checks.

# Host test

`test_desk_wbgauge` in `tests/test_desk.c` (run: `make test ONLY=desk`, 1892 checks, 0 failed).
Revert proof: with `return 0;` inserted right after `*save_out = -1;` in `desk_wbgauge`, the run fails (FAIL tests/test_desk.c:333..342, among them `out[26 + 8 + 39] = 0, want 1`). With the fix restored: 0 failed.
(A first revert attempt was a no-op because the sed insert did not apply; the python insert above is the real one.)

# Rig result

Not yet verified on a 3.2 rig that the gauge disappears from an open volume window after USE, nor the byte-identical RESTORE. See the handoff row.
