---
date: 2026-10-06
topic: TTY:, a serial line as a real tty DOS device (W52), and vsh <stream> (W53a)
tags: [serial, pty-handler, ldisc, vsh, aux]
status: draft (dropped 2026-10-06: auxcon-handler covers it; see ledger W52)
---

# TTY: -- a serial line with a real tty (W52) and `vsh <stream>` (W53a)

Research: `thoughts/shared/research/2026-10-06_aux-handler-and-vsh-shell.md`.
Ledger: W52, W53 in `thoughts/shared/plans/2026-09-28-vtcon.md`.

## Problem, in the domain's terms

A serial line opened as a DOS stream must behave as a Unix tty: termios with a
true raw mode (cfmakeraw: no \n conversion, no XON/XOFF, every byte through),
ISIG signals, a window size that `stty rows/cols` sets. The ROM's AUX: (on 3.2
a stub forwarding to con-handler) is line-cooked and its raw mode is not raw,
so `sz` over `NewShell AUX:` breaks. upgetty already gives a real tty, but only
for the one shell it starts; it is not a device any program can open.

## Where the fix lives -- options

- **A. A serial mode in pty-handler (chosen).** pty-handler already serves the
  slave side of a tty completely (ldisc, SCREEN_MODE, WAIT_CHAR, TCGETA/TCSETA,
  WINSZ, CHANGE_SIGNAL). Its master side is only `ld_input`, the out[] queue and
  `master_reads` (pty_handler.c:317-340, :686-690). In serial mode the master is
  the serial line, pumped by the handler itself. One ldisc, one process, no new
  packet code. Cost: Startup parsing, a third wait source, serial pump.
- B. upgetty-style bridge process plus an alias name. AmigaDOS has no way to
  alias a device name onto PTY:<id>/s; it would need a forwarding handler on
  top. More parts, two processes per line. Rejected.
- C. A new handler copying pty-handler's slave code. Duplication. Rejected.

## Decisions

- Device name **TTY:**, opt-in. The ROM's AUX: is left alone. (Owner asked
  2026-10-06 to confirm TTY: vs replacing AUX:; renaming is one mount file.)
- **No W53(b)** (vsh as the system/boot user shell) unless the owner says
  otherwise: SystemTags/Execute callers expect AmigaDOS syntax (eriQue: "svart
  att fa det har att funka bra mot resten av ecosystemet").
- Defaults as upgetty: serial.device unit 0, 19200 baud, 8N1, XON/XOFF off
  (SERF_XDISABLED), RAD_BOOGIE, no RTS/CTS (FS-UAE's serial line is
  three-wire). Options in the mount's Startup: `DEVICE=name UNIT=n BAUD=n RTSCTS`.
- Serial code is **shared**, not copied: upgetty's serial half
  (device/upgetty.c:88-104, :211-227, :252-283) moves into `device/serial_io.c`
  + `.h`; upgetty and pty-handler both use it.
- Startup string parsed by a pure function `tty_startup_parse()` (host-tested).
  Read from the DeviceNode's dn_Startup (the handler gets the DeviceNode in
  dp_Arg3 today, pty_handler.c:828-832).
- Back-pressure: when the ldisc input is full, the handler does not requeue
  CMD_READ; serial.device's buffer holds bytes (SDCMD_SETPARAMS io_RBufLen
  8192); with RTSCTS the hardware stops the sender.
- In serial mode every open name on the device (TTY:, TTY:x) opens the one
  slave. More than one opener shares it, as a console.

## Phases

### P1 -- shared serial I/O (device/serial_io.c)
- Move open/params, async read, write out of upgetty.c; upgetty calls them.
- Check: `make amiga` builds upgetty; `tools/rig/getty_rig.py` and
  `tools/rig/zmodem_rig.py` pass (the only cases this can reach).

### P2 -- pty-handler serial mode
- `tty_startup_parse(const char *s, tty_serial_opts *o)` in
  `handler/tty_startup.c` + `.h`; host test in `tests/` wired into vttest_host
  (defaults; each option; bad BAUD/UNIT rejected; quoted Startup).
- Handler: if dn_Startup has serial options, open serial via serial_io; add the
  serial read reply port to the waitset; serial bytes go to `ld_input`; ldisc
  output goes to an async CMD_WRITE (one in flight, next chunk on reply).
- Check: host tests; `make amiga`.

### P3 -- mount file and the reachability test
- `dist/TTY` mount file (Handler L:pty-handler, Startup "UNIT=0 BAUD=19200",
  as dist/PTY otherwise).
- `tools/rig/tty_rig.py`, reusing getty_rig's `Wire`: Mount TTY: from a VTC:
  Mountlist, `Run NewShell TTY:`, then on the host side:
  1. prompt arrives; `echo hi` echoes `hi`;
  2. `Wait 30` then 0x03: prompt back within 3 s (ISIG reaches the opener);
  3. `stty rows 40 cols 100` then `stty size` prints `40 100`;
  4. `sz` of a 70 KB file over TTY:, host `lrz` receives it byte for byte;
  5. sentinel: `Status` shows no upgetty -- the bytes went through
     pty-handler's serial mode.
- Check: tty_rig passes; getty_rig and zmodem_rig still pass.

### P4 -- `vsh <stream>` (W53a)
- vsh.c:1352: an argument that opens and is IsInteractive() becomes vsh's
  console (SelectInput/SelectOutput, the interactive loop); a file stays a
  script, as sh.
- Check: tty_rig step 6: `Run vsh TTY:` (no NewShell) gives a vsh prompt on the
  host side; `vsh script` still runs a script (existing tests).

### P5 -- kit, docs, ledger
- Kit: dist/TTY ships in the drawer `Storage/DOSDrivers/TTY` (not mounted by
  Install). README: a TTY: section (mount, `NewShell TTY:`, `vsh TTY:`,
  options, why not AUX:).
- Ledger W52/W53 ticked with commits.

## Success criteria

- Automated: host tests (startup parser), `make amiga`, tty_rig 6 of 6,
  getty_rig and zmodem_rig unchanged.
- Manual (owner, real A1200 + null-modem cable): `Mount TTY:` from the kit's
  Storage drawer, `NewShell TTY:`, a terminal on the other machine gets a
  prompt; `sz` a file to it.

## Known limits

- The open "key lost before tcsetattr(raw)" bug (ledger :188-194) lives in the
  PTY read-queue path this mode also uses.
- The echo buffer drops bytes when full (research doc, section 1).
