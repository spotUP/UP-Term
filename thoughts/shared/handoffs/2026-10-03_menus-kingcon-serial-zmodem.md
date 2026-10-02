---
date: 2026-10-03
topic: Settings menus, KingCON completion, the installer kit, the serial login and our zmodem
tags: [vtcon, h8, h9, t4, installer, zmodem, upgetty, rig]
status: final
---

# Handoff 2026-10-03 (session of 2026-10-02/03)

## Tasks
Owner-driven, all in ~/Code/vtcon (branch main, NOT pushed -- the owner decides):
- Installer kit (Commodore Installer script, Files/ layout), Shell-icon / SYS:System icon /
  serial-at-boot options.
- coreutils 5.2.1 in SYS:UP-Term/bin via vsh $PATH (fileutils shims retired).
- H8 KingCON completion (all FNCMODE styles, the Complete menu, the cache, .info, ASL).
- H9 Settings menu S1-S7 (cursor, bell, bold, meta, copy, wheel, completion, KingCON style,
  Font..., Theme..., Profile, Save settings to profile, Scrollback) -- all live.
- T4 serial login: upgetty fixed (keys never reached the shell: Write inside LG()), our own
  ZMODEM (zm/: core + C:sz / C:rz), PTY: SetMode raw = cfmakeraw, serial login at boot.

## Critical references
- Ledger: thoughts/shared/plans/2026-09-28-vtcon.md (H8 K1-K9, H9 S1-S7, T4 G1-G4, R1, A1, C1,
  N1, D1, T1 tabs, I1).
- KingCON spec: thoughts/shared/research/2026-10-02_kingcon-completion.md.
- Userland decision: thoughts/shared/plans/2026-10-02_unix-userland.md.
- Rig tests: tools/rig/{install_rig,kingcon_rig,zmodem_rig,getty_rig,condev_rig,prefs_rig}.py.

## Learnings
- The rig's system disk is ~/Downloads/nyhd2.hdf (owner's); rig.py copies it to
  build/rig/sys.hdf. A damaged copy is in build/rig/sys.hdf.backup-2026-10-03 (delete once the
  owner confirms nothing from it is missed).
- Never two drivers on the rig: another session's amiagent installer killed a run; the
  neovim agent is told not to touch it.
- The owner: "you dont think you are overdoing the tests a bit?" -- verify a change with its
  own check, one full install_rig only when landing a kit-wide change.
- amiagent EXEC sometimes stalls (>60 s, the command already gone): open, unexplained.
- vbcc: `(void)x` is warning 153/65 under -warnings-as-errors (-dontwarn=153,65 where used);
  a macro that compiles to nothing must still evaluate side effects (the upgetty bug).
- 115200 serial without flow control overruns on the rig; FS-UAE never raises CTS, so
  RTS/CTS on makes nothing leave the port. Defaults: 19200, no flow control, RTSCTS option.

## Open (in the ledger, in order of the owner's wishes)
- Owner's question still unanswered: "hot/live" -- (a) Prefs' Use/Save updating open windows,
  or (b) a menu change applying to all windows.
- C1 slash commands; A1 Claude from the Amiga (uptelnet + Mac endpoint); T1 tabs; D1 demo;
  R1 UP-Term in ROM (incl. the reset-proof serial login); N1 ncurses 6 wide; the rest of H9.
- rz on the rig ~1 KB/s against a 2.6 KB/s raw line (not ours as far as measured).
- A key typed before tcsetattr(raw) lost in some getty_rig runs (intermittent).
- A neovim port agent (Q1) works in ~/Code/neovim-amiga; it reports rig runs it needs.

## Artifacts
- ~/Desktop/UP-Term.lha (and unpacked ~/Desktop/UP-Term/): the current kit.
