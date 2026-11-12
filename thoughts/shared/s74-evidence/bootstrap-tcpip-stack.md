---
date: 2026-11-12
topic: up-term-boot.lha installs AmiNetXDuo when the Amiga has no TCP/IP stack
status: fail on the brief's scale (case B detection and installer start proven; full stack install, case A and case C not proven)
---

# Bootstrap kit: TCP/IP stack when none is present

Owner decision 2026-11-12: `up-term-boot.lha` carries AmiNetXDuo 1.0.0-rc2 (MIT) and
`Get-UP-Term` runs its own `Install-AmiNetXDuo` when the Amiga has no TCP/IP stack.
No own network configuration code.

## Verdict: FAIL against the brief's scale (A and B not both proven)

- Case A (stack present, ends "UP-Term installed."): NOT TESTED on the rig. The rig has no
  Amiga `lha` (`Which lha` rc 5) and none was found on the Mac, so `Get-UP-Term` cannot
  unpack the fetched `up-term.lha`; the detection path for "stack present" is the existing
  `tcpprobe` check and is unchanged in behaviour (only skipped into the new block when all
  probes fail).
- Case B (no stack): detection PASS, installer start PASS, full install NOT PROVEN.
  With `tcpprobe` replaced by an s-bit script that quits 5, `Get-UP-Term TO RAM:dl` took the
  no-stack branch and started `Installer SCRIPT Install-AmiNetXDuo`; the window
  "Install-AmiNetXDuo" opened and was driven through its first pages. The generic page driver
  (`installer_rig.drive`, case name `bootanx`) then looped for ~1050 s between two pages
  (one with 8 toggles, two columns: an `askoptions` page; one without), 1023 page records,
  title "Install-AmiNetXDuo (0% done)". Not diagnosed whether this is the driver answering
  a validation loop or the script's own redo loop. `S:AmiNetXDuo-Files` was never written,
  so the "reboot" message and rc 5 were not reached on the rig.
  Nothing on the system changed: no `S:AmiNetXDuo-Files`, no `S:Network-Startup`, no
  AmiNetXDuo line in `S:User-Startup`, no `UPTGET:` assign, no `ENVARC:certs`, `RAM:dl` empty.
- Case C (network card in FS-UAE): NOT DONE. FS-UAE has A2065 emulation, and the
  AmiNetXDuo ReadMe lists "Commodore A2065 . . . a2065.device", but AmiNetXDuo does not ship
  `a2065.device` (its Devs/Networks has anxgenet.device, anxnet.device, anxwifipi.device) and
  no copy was found locally.

## Changes

- `dist/Get-UP-Term`: after the `TO` check, probes for a stack (`tcpprobe`; assigns
  AmiNetXDuo: AmiTCP: Miami: Genesis: TheWire:; `S:AmiNetXDuo-Files`,
  `LIBS:genesis.library`, `LIBS:bsdsocket.library`). None found: checks the
  `AmiNetXDuo/Install-AmiNetXDuo` drawer and an Installer (else rc 20), says what happens,
  `CD AmiNetXDuo`, runs `Installer SCRIPT Install-AmiNetXDuo APPNAME AmiNetXDuo MINUSER AVERAGE
  DEFUSER AVERAGE NOLOG`, `CD /`. No `S:AmiNetXDuo-Files` after: rc 20 with a retry line.
  Else: "AmiNetXDuo is installed. Reboot the Amiga so that it starts, then run / Execute
  Get-UP-Term TO <drawer> again in this drawer to fetch and install UP-Term." rc 5.
  Return codes: 0 UP-Term installed; 5 AmiNetXDuo installed now, reboot and run again
  (nothing fetched); 20 failed.
- What a user with no stack sees first: "This Amiga has no TCP/IP stack (no
  bsdsocket.library), and the download needs one." / "Starting the AmiNetXDuo installer (a
  free TCP/IP stack): it asks for your network card's" / "SANA-II driver and for DHCP or a
  fixed address. UP-Term itself is not installed yet."
- `Makefile`: `ANX_SRC`, `ANX_FILES` (vendor drawer without Developer, Developer.info),
  `ANX_COPY`; the kit copy uses it (the old `rm -rf` of Developer is gone, no new `rm -rf`);
  dist-boot builds `tcpprobe`, copies it and the AmiNetXDuo drawer, content check has 11
  entries and refuses a Developer drawer.
- `tests/test_dist_installer.py`: staging assertions for the new Makefile lines.
- `README.md`, `dist/LICENSES.txt`: boot kit row and AmiNetXDuo licence row.

## Build

- `df -h /` 15 GiB free before the build, 11 GiB at the end.
- `make dist-boot` rc 0: `[OK] build/up-term-boot.lha: 5944733 bytes`
- sha256 `ba63085af99c0b626e4ab5cf475cd8b3fe06f4de2852516d4a57829bb280d0c0`
- 216 `AmiNetXDuo/` entries, 0 Developer; `diff -rq` vendor drawer vs stage: only
  Developer and Developer.info differ.
- Host tests after all edits: test_dist_installer OK (skipped=1), test_dist_addons OK,
  test_dist_iconlib OK, test_dist_solidwin OK.

## Rig 97 setup (FS-UAE, OS 3.2, purpose "up-term-ba s74: bootstrap TCP/IP stack test")

- The rig's emulator bsdsocket.library (4.1) cannot be switched off without losing the
  wasabid control channel, so "no stack" was made by replacing `tcpprobe` with an s-bit
  script `Quit 5` (installer_rig precedent).
- No Amiga `lha`: the archive was unpacked on the host and copied to `RAM:boot/up-term-boot`;
  a stub `lha` (s-bit script `Quit 0`) passed the `Which lha` check.
- First run failed setup: "Get-UP-Term: RAM:dl does not exist. Nothing was changed." rc 20.
  Second run with `MakeDir RAM:dl` reached the Installer as above.
- Page log: `build/rig-reports/rig97/installer/bootanx.pages.txt` (976 KB, kept).
- Rig stopped (`rig.py stop` rc 0, no process left); `build/rig-saved/97` (11 MB) deleted.

## Not tested

- Real `lha` unpack of `up-term-boot.lha` on an Amiga.
- AmiNetXDuo install to the end, reboot message and rc 5 on a machine.
- Case A end to end ("UP-Term installed." after the download).
- Case C (real network card, a2065.device or other SANA-II driver).

## Register row text (for thoughts/shared/research/test-register.md, not edited here)

| 2026-11-12 | s74 | up-term-boot.lha no-stack path | rig 97 OS 3.2 | PARTIAL: no-stack detection and Install-AmiNetXDuo start pass; full stack install, stack-present path and network card not proven (no Amiga lha, Installer page loop under the driver, no a2065.device) | thoughts/shared/s74-evidence/bootstrap-tcpip-stack.md |
