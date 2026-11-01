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
1. DONE: the rig 8 `--only all` rerun at 6157fbe2 passed 96 of 96; DOPUS5-BLOCK-GONE-ALLCASE is ticked.
2. DONE e6ad0d1f BASEDEST-EMPTY-SYSUPTERM: check 7 compares `List SYS:UP-Term ALL` before and after. Rig 3 base-dest 93 of 95; the two FAILs (55 nano, 66 zip) are PORTS-BUILD-GONE.
5. INSTALLER-ROOM-SPLIT, host done fa5da1c9: Install.installer runs UPBigVol ROOM with install.dos's DEST/SYS split; UPBigVol writes ENV:UPTROOM and ENV:UPTROOMAT. Still open: a rig run of the GUI room page (installer_rig room case). The kit on the rigs must carry the new UPBigVol and Install.installer first (hand refresh, see item 3).
4. DCTelnet modern ANSI (owner question): research in thoughts/shared/research/2026-10-30_dctelnet-modern-ansi.md recommends the embedded engine route (dctelnet-vtcon) over up-console.device. No code yet; the work items are listed there.
3. PORTS-BUILD-GONE: upterm-ports/build is gone, so `make dist` is blocked until a `make kit-stage` (a full ports build in the other repo) runs. The owner decides. Until then, refresh the kit by hand: copy the changed Files into build/dist/up-term/Files, then run `python3 tools/mkkit.py base`.

Gotchas:
- Rigs without `rig.py dopus` boot DOpus 5.82, and DOpus add-on checks then see the old program.
- test_dist_ipk fails while ../upterm-ports sits on branch port/htop (no recipe has _SUMMARY there). Ledger TEST-IPK-PORTS-BRANCH.
- test_doc_setenv_save (dist/README.txt:890, from fc56d6b7) and test_stage_rig (super+help rows) fail without the session 27 changes. Ledger TEST-FAILS-NOT-MINE.
- Disk: about 5.7 GiB free on /. Check `df -h /` before any build or rig run.

## Session 28 update (2026-10-30)

Done, on origin/main:
- a40249ad TEST-FAILS-NOT-MINE: dist/README.txt shows the OS 3.1 SetEnv form; amarchy_clash_rig knows the Help key.
- ad6bdfd3 RIG-START-RACE: two `rig.py start` begun together launched two FS-UAE on one sys.hdf (disk checksum error on rig 3). start() now holds <rig dir>/start.lock for the whole boot. Test tests/test_rig_start_race.py.
- 6d95e04f INSTALLER-ROOM-SPLIT (a): installer_rig case `space` reads the GUI room page and asserts both UPTROOM lines and the KB numbers (rig 3 PASS 12/12). The earlier "smear" was the drawer page redrawing after Make New Drawer; the drive loop now counts DrawerAsks.
- DCTelnet MA0 handed to the DCTelnet session (dctelnet-17); its commits wait for the owner to push. upterm bc99e23 + 6914a94 (bootstrap is_checkout, repos.lock pin; the pin must be redone after the DCTelnet rebase).

Open, in order:
1. INSTALLER-ROOM-SPLIT (b): case `space-sys` (refusal on a disk that is not the drawer disk) is added but not green. The second run stalled at the Network tools page under host load 195. Rerun once load is low: `UPTERM_RIG=3 python3 tools/rig/installer_rig.py --only space-sys`. If it stalls again with a quiet host, look at the 60 s stall limit.
2. PORTS-BUILD-GONE and TEST-IPK-PORTS-BRANCH: owned by the upterm-ports session (owner 2026-10-30). Keep refreshing the kit by hand.
3. RIG10-DISTKIT-MIXED stays open.

Gotchas:
- Local main may still hold d44a14df, 8b1e662a and 93ddfac7; origin has them as a40249ad, ad6bdfd3 and 6d95e04f. Rebase main onto origin/main before new work (the patches drop out).
- build/up-term.lha (33 MB) is the hand-built kit from `mkkit.py base`.
- Disk: 16 GiB free on /, under the 20 GiB line. Clean before big builds.

## Session 29 update (2026-10-30)

Done, on origin/main:
- 2766de0d INSTALLER-ROOM-SPLIT (b) closed: space-sys PASS 10/10 on rig 3 (refusal on SYS: while the drawer is on DH1:). The first s29 run installed instead of refusing because run_case filled SYS: before run_base_case's cleanup (prepare + Uninstall) freed about 8.8 MB of a stopped run's leftovers; fill_sys now runs after the cleanup. The s28 stall was host load only. Also: check_room_text collapses OCR dash runs ("UP<em dash>-Term"); space PASS 12/12. Tests: test_installer_rig_fill_order, test_installer_drive_loop RoomText (both fail with their fix reverted).
- Ledger row INSTALLER-ROOM-SPLIT ticked.

Open, in order:
1. RIG10-DISTKIT-MIXED stays open (see its row).
2. PORTS-BUILD-GONE and TEST-IPK-PORTS-BRANCH: owned by the upterm-ports session.
3. ADDON-UNINSTALL-TRAILING-BLANK (AddonCases x3 + InstallerCases add_on fail at HEAD).

Gotchas:
- Rig 3 stopped at session end.
- Untracked files `-` and `scratchpad_info` in the repo root are not this session's; left alone.
- Disk: 15 GiB free on /, under the 20 GiB line.
