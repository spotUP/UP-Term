---
date: 2026-10-30
session: vtcon-05 (s25)
status: partial
---

# Session 25 handoff: rig uhc, UPKG multidir, AmiSSL in install.dos

## Done (on origin/main)
- RIG-PYTHON-UHC-FAIL closed: 64ba7a7a, 310ceb96, 66d6eef5, 12d0d426, 9eca3a5b. Rig 9 `install_rig --only all` 96/96.
- CLAUDE-SETUP-SWALLOW closed as by design (claude/tui.c:4262; thoughts/spot/s25-claude-setup-swallow.md).
- UPKG-MULTIDIR-PAYLOAD closed: 50e5fce3, 626f6ff0, a2f4f97d, merge c496fcfe. tests/test_dist_addons.py now runs (67 OK).
- INSTALL-DOS-NO-AMISSL: 95622985 runs Install-AmiSSL from install.dos when LIBS: lacks amisslmaster.library 5.
- AMISSL-RECHECK-FALSE-ERROR: f5746979 + cb5a1c1f add `Assign LIBS: AmiSSL:Libs ADD` (guarded by `Assign AmiSSL: EXISTS`) after Install-AmiSSL in install.dos and P_AMISSL. Rig 10: `Version LIBS:amisslmaster.library 5` rc 0 before reboot. Tests: test_install_dos_amissl.py + test_dist_installer.py 95 passed, 1 skipped.

## Open, in order
1. Rig run that captures the install.dos [INFO] success line (keep the ami long window log), then tick ledger rows s25 INSTALL-DOS-NO-AMISSL and s25 AMISSL-RECHECK-FALSE-ERROR (todos.md ~3607, ~3630).
2. s25 AMISSL-SHELL-WINDOW: P_AMISSL now uses (execute "T:UPTAMISSL"); unproven. Owner retests the graphical Install on a machine without AmiSSL in LIBS:. Install-AmiSSL itself has no run/execute/newshell line.
3. s25 INSTALLER-REBOOT-NOTE-STALE, s25 P-ANX-RUN-EXECUTE, s25 SHELL-INSTALL-DISK-FULL (new rows).
4. s25 ADDON-OPTIONAL-UPTERM-WRITES, s25 ALL-CASE-RERUN-STALE, KIT-TCP-STACK, INSTALL-STUCK-TCPSTACK.

## Gotchas
- `ami.py gclick <win> 90` fails on Install-AmiSSL; use `ami.click_px(<gadget centre>)`. Proceed/Yes is the left button.
- Restart the rig before each `--only all` rerun (SYS:UHC stays after Uninstall by design).
- `make dist-base` needs upterm-ports `make kit-stage` first (SOURCES.txt).
- Rig 1 (build/rig) belongs to the owner; never stop it. Rigs 9 and 10 are stopped.
