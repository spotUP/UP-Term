---
date: 2026-11-05
author: opus-brief agent vtcon-c1
row: S57-ICL-PINK-SHADE rig proof
build: main c0e2365d
verdict: NOT RUN. make dist fails on main, no full-install rig can be built
---

# Pink/red icon label proof on rig 145: blocked by a main build break

## Result
`UPTERM_PORTS=.../upterm-ports/.claude/worktrees/kit-main make dist` (and `make -k dist`) on main c0e2365d stops at
`build/amiga/upprefs`: `Error 21: Reference to undefined symbol _ip_any_free` (vlink).
No rig 145 was started, no boot counted. Zero pixels measured. Nothing to clean on the rig side.

## Cause (read from the Makefile and sources)
- f24f6906 added `ip_any_free` to amarchy/iconpen_core.c and made amarchy/flat_core.c:842 call it.
- Makefile line 939/941 (`$(BUILD)/amiga/upprefs`) links amarchy/flat_core.c but not amarchy/iconpen_core.c.
  amarchy/iconpen_core.c is in AMARCHY_MENU_SRC (line 1013) and AMARCHY_GUI_SRC (line 1027) only.
- It is the only failing target in `make -k dist` (dist2.log: one Error 21, one make error).
- Fix (not applied, brief is read-only on code): add amarchy/iconpen_core.c to the upprefs prerequisites and link line.
  Also check any host test target that links flat_core.c (tests/test_flat_core.c passed upstream, so likely fine).

## Recipe to use once main builds
`make wasabi`; `UPTERM_PORTS=/Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main make dist`;
`UPTERM_RIG=145 INSTALLER_KEEP=1 python3 tools/rig/installer_rig.py ONLY fresh-all --purpose "<who: what>"`
(fresh-all = every box on every page ticked, reboot, Amarchy up, kit stays installed), then
`rig.py start --max --os 32 --idle-minutes 0` boots and pixel counts per boot.
