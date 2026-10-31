# s28 AM-ICON-NOBORDER evidence

- Setting: BOOL wbp_Borderless in struct WorkbenchPrefs, chunk WBNC of ENV:Sys/Workbench.prefs (NDK 3.2 vendor/ndk-3.2r4-Include_H/prefs/workbench.h:25-37; BOOL is a WORD, exec/types.h:167, so body offset 20). The 3.2 SYS:Prefs/Workbench binary holds WBNC and "No Border" strings. No WorkbenchControl tag exists for it (grep of workbench/workbench.h: only SetDefaultStackSize, SetTypeRestartTime, SetGlobalFlags).
- Path: amarchy/desk_dos.c dk_wbborder() (called from desk_dos_apply after dk_flat_pens) reads the file, desk_wbborder() (amarchy/desk_core.c) flips the word, dk_put("Workbench") feeds SYS:Prefs/Workbench FROM <work> USE, like every other part. Saved value: T:amarchy-revert/wbborder.prev. 3.1: SYS:Prefs/Workbench has no WBNC string, so skipped.
- Host test: tests/test_desk.c test_desk_wbborder. make ONLY=desk test: 1778 checks, 0 failed. Revert check (flat branch forced to return 0): 6 failed. Restored: 0 failed.
- make amiga: builds.
- Rig: NOT DONE. Rig 8 was already running for another session (title "Amarchy s28: backdrop vs window colour", started 23:31:56); not touched.
- Open: with no Workbench.prefs file at all, flat on does nothing (the core needs the file to keep the other fields); the rig check must show whether 3.2 always has one.
