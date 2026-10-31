# s28 AM-ICON-NOBORDER evidence

- Setting: BOOL wbp_Borderless in struct WorkbenchPrefs, chunk WBNC of ENV:Sys/Workbench.prefs (NDK 3.2 vendor/ndk-3.2r4-Include_H/prefs/workbench.h:25-37; BOOL is a WORD, exec/types.h:167, so body offset 20). The 3.2 SYS:Prefs/Workbench binary holds WBNC and "No Border" strings. No WorkbenchControl tag exists for it (grep of workbench/workbench.h: only SetDefaultStackSize, SetTypeRestartTime, SetGlobalFlags).
- Path: amarchy/desk_dos.c dk_wbborder() (called from desk_dos_apply after dk_flat_pens) reads the file, desk_wbborder() (amarchy/desk_core.c) flips the word, dk_put("Workbench") feeds SYS:Prefs/Workbench FROM <work> USE, like every other part. Saved value: T:amarchy-revert/wbborder.prev. 3.1: SYS:Prefs/Workbench has no WBNC string, so skipped.
- Host test: tests/test_desk.c test_desk_wbborder. make ONLY=desk test: 1778 checks, 0 failed. Revert check (flat branch forced to return 0): 6 failed. Restored: 0 failed.
- make amiga: builds.
- Rig: NOT DONE. Rig 8 was already running for another session (title "Amarchy s28: backdrop vs window colour", started 23:31:56); not touched.
- Open: with no Workbench.prefs file at all, flat on does nothing (the core needs the file to keep the other fields); the rig check must show whether 3.2 always has one.

## 3.2 measurement and no-file fix (rig 9, fresh 3.2, 2026-11-01)

- Stock 3.2: ENV:Sys has only pointer.prefs and screenmode.prefs; ENVARC:Sys has WBConfig.prefs (54 bytes, no WBNC chunk) and no Workbench.prefs. So the file does not exist by default.
- wasabi snoop while the stock SYS:Prefs/Workbench (47.32) cycled Border Size, then Use, then Save (build/rig9/wasabi/snoop.log):
  - `SYS:Prefs/Workbench Open("ENV:Sys/Workbench.prefs", read) = fail (Error 205: Object not found)` (editor start, no file)
  - Use: `SYS:Prefs/Workbench Open("ENV:Sys/Workbench.prefs", create) = ok`, then `« IPrefs » Open("ENV:Sys/workbench.prefs", read) = ok` (IPrefs reads it back)
  - Save: `Open("ENVARC:Sys/Workbench.prefs", create) = ok` and `Open("ENV:Sys/Workbench.prefs", create) = ok`
  - WBConfig.prefs is never opened. Use writes ENV: only; Save writes ENVARC: and ENV:.
- File: 84 bytes, FORM PREF, PRHD, one WBNC chunk of 50 bytes. Border Size "No Border" sets the WORD at body offset 20 to 1 (and the emboss rect to -1 -1 1 1). Stock content: stack 4096, restart 3, precision 16, rect -3 -3 3 3, border 0, name length 25, NewIcons 1, ColorIcons 1, extension words.
- Fix: desk_wbborder() (amarchy/desk_core.c) with no file writes that stock file with the border word 1 and keeps DESK_WB_CREATED in wbborder.prev; off writes the stock file with border 0 through USE, then dk_wbborder() deletes ENV:Sys/Workbench.prefs and the prev file, only while the file is byte-identical to what we wrote (else forgotten, left alone). The rect is not changed by us.
- Host: tests/test_desk.c test_desk_wbborder_absent; `make test ONLY=desk` 1790 checks 0 failed; with the create branch reverted 5 failed.
- Rig 9: flat on (theme pick, entry point desk_dos_apply) with the file absent -> ENV:Sys/Workbench.prefs 84 bytes, FORM/WBNC, border word 0001, wbborder.prev = "2", no ENVARC copy. flat off -> ENV:Sys/Workbench.prefs gone, T:amarchy-revert empty. Frame pixels: NOT verified (the theme pick repaints the whole screen; the whole-screen diff cannot isolate the icon frame). Screenshot: ~/Desktop/upterm-shots/s28-icon-noborder-rig9.png (not viewed).
