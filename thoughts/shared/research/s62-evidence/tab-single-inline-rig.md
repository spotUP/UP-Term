# Single-match Tab inline, rig 166, origin/main 7a913e8d

Build: make worktree-ready/wasabi/amiga/dist OK. installer_rig ONLY fresh-all: PASS (35 ok).
Rig 166 stopped (plain stop), build/rig166 gone, no fs-uae for rig166.

## Results
- Inconclusive (text cannot tell ghost from inserted). The window text reader (UPTerm ACTIVE screen) returns the
  ghost text too: with only RAM:tabprobe_zzz present, `Type RAM:tabprobe_z` already READ `Type RAM:tabprobe_zzz`
  BEFORE Tab was pressed, and the same after Tab. Return then printed `x`. No pixel check done (budget).
- Control with two files (zzy, zzz): line after Tab read `Type RAM:tabprobe_zz` (common prefix) in an earlier run =
  no wrong insert. The final re-run (zzy re-created) read `...tabprobe_zzz` after Tab, ambiguous (ghost).
- Test-setup finding: `CD RAM:` in plain AmigaShell and in vsh turned into `Cd RAM:rig-libs-done` and failed
  ("object is not of required type" / vsh cwd "RAM Disk:rig-libs-done", `ls` "cannot open directory '.'").
  Cause not hunted; looks like the ghost suggestion being accepted by Return. So the CD RAM: recipe step is not
  usable on this build; full RAM: paths were used instead.
- Needed next: measured-pixel comparison (ghost dim colour vs normal text) of the cells after `tabprobe_z`.

## Register row
2026-11-06 | rig 166 fresh-all 7a913e8d (--max --os 32) | INCONCLUSIVE: text reader shows ghost, Tab single-match inline not distinguishable; CD RAM: becomes RAM:rig-libs-done | thoughts/shared/research/s62-evidence/tab-single-inline-rig.md
