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

## Session 26 update (2026-10-30)

- INSTALL-DOS-NO-AMISSL and AMISSL-RECHECK-FALSE-ERROR closed: rig 10 run C3 with AmiSSL removed from LIBS: (Version rc 20 before) printed "[INFO] AmiSSL: LIBS:amisslmaster.library version 5 is missing or older, running Install-AmiSSL." and "[INFO] AmiSSL installed: LIBS: has it now, usable at once.", no [ERROR], Version 5.27 rc 0 before reboot. Text read by OCR (Execute >log and the ami.py long window lost it). Evidence thoughts/spot/s26-amissl-shell-window.md.
- AMISSL-SHELL-WINDOW closed, not reproduced (owner watched run A2).
- ae1989d3: Install.installer's AmiSSL note says "AmiSSL installed: LIBS: has it now, usable at once." instead of asking for a reboot.
- 7de8acf1: dist/wasabi/SOURCE.txt names wasabi 83acd194 (the repins had left 562672a8); the drawer test guards it, so every repin must update SOURCE.txt.
- build/rig9 deleted (2.4 GB) with vtcon-32 and dctelnet-17 agreeing.
- Open, first item next session: RIG10-DISTKIT-MIXED in todos.md. build/dist/up-term/Files lacks Agent, amnotify, bar.conf, S although dist/parts.txt lists them; run make dist at HEAD (df -h / first, ~3.5 GiB free) and check whether they appear. If not, fix the Makefile rule with a test.
- Install-AmiSSL clicks: on 2-row pages Proceed/Yes is (279,377); (279,394) is About; the last Proceed at (400,394) can need two clicks.

## Session 27 update (2026-10-30)

Done, on origin/main:
- cee4fe42 SHELL-INSTALL-DISK-FULL: install.dos runs `UPBigVol ROOM` before any Copy, MakeDir or Assign. It refuses with rc 20 and names the free and needed KB. Rig 3 proof: DEST=RAM:UP-Term gives "RAM:UP-Term has 0 KB free; the parts you chose need about 38307 KB." and nothing is copied. base-dest installs with no room message. Ledger row ticked.
- 6157fbe2 DOPUS5-BLOCK-GONE-ALLCASE: the check was wrong, not the uninstall. A rig that boots the master's DOpus 5.82 never gets a ;BEGIN DOpus5 block. The check now demands the block only when S:User-Startup after the Installs holds it. Regression test added.

Open, in order:
1. The rig 8 `--only all` rerun at 6157fbe2 (scratchpad all8b.log). Tick DOPUS5-BLOCK-GONE-ALLCASE when 97 of 97 pass.
2. BASEDEST-EMPTY-SYSUPTERM: base-dest FAIL 7 on a fresh rig 3 found an empty SYS:UP-Term. List it before the case to tell whether it comes from the image or the install.
3. PORTS-BUILD-GONE: upterm-ports/build is gone, so `make dist` is blocked until a `make kit-stage` (a full ports build in the other repo) runs. The owner decides. Until then, refresh the kit by hand: copy the changed Files into build/dist/up-term/Files, then run `python3 tools/mkkit.py base`.

Gotchas:
- Rigs without `rig.py dopus` boot DOpus 5.82, and DOpus add-on checks then see the old program.
- Install.installer's GUI room check counts the SYS parts against the drawer's volume. It overestimates when DEST is not on SYS:. install.dos splits the parts per volume. Ledger row INSTALLER-ROOM-SPLIT.
