---
date: 2026-10-02
topic: xcon-crash-diagnosis
tags: [rig, xcon, handler, crash, diagnosis]
status: final
---

# XCON: "Software Failure" on every NewShell — diagnosis

**Update (later 2026-10-02): not reproduced since 2312d21.** That commit
removed `config_load`'s `AddPort` of a private port that was then deleted
without `RemPort`: every window open left a freed node on exec's public
port list, and any later `AddPort`/`FindPort` -- by any program, the next
handler included -- walked freed memory. That fits every point below: it
"fails identically" on older builds because the damage lives in the
running system, not the binary, until a reboot; it is a heisenbug because
what reuses the freed node varies. Since the fix, 30+ XCON: windows opened
and closed on the rig across the day's checks (closecrash_rig 6/6 twice,
install_rig 39/39, make test-rig, prefs_rig) without a failure. The
pre-fix handler built from 989ab65 could not even open a window within
150 s on a fresh boot. Kept below as the record.

## Symptom

Any `NewShell XCON:...` puts up an Exec "Software Failure" requester
("XCON Program failed (error #887000004)"). No window opens. The rig stays
alive; amiagent keeps answering.

## Ruled out (with evidence)

- **Not a regression from 2026-10-01/02's work.** Built and installed the
  handler from `6ac8147` (before profiles, Prefs, themes, selection colours)
  — fails identically. Same for `55ede3b` and `HEAD`.
- **Not the profile file.** With `ENV:up-term/up-term` deleted, so
  `config_load` returns at the `Open`, the crash is unchanged.
- **Not the rig's mountlist.** Booted with `build/rig/boot/Mountlist` emptied
  so DOS falls back to `DEVS:DOSDrivers/XCON` — unchanged. Restored after.
- **`dp_Arg3` is a genuine `DeviceNode`,** not a mount Segment. Dumped the
  structure from the handler: offset 0 = `0x1000a555` (`dn_Next`, a pointer),
  offset 4 = 0 (`dn_Type`, "always 0 for dos devices"), offset 8 = 0
  (`dn_Task` before the handler writes it). Matches `struct DeviceNode` in
  `dos/filehandler.h` exactly, so `dn_Task` is written to the right place.

## Where it dies

With a `DEBUG=1` build and a startup trace, the last line before death is
inside `handler_main`, after the `upconf` alloc succeeds:

```
trace enter / gotport / gotpkt / alloced / trace libs / startup
node dump / node dump2
s1 dntask / s2 replied / s3 conf 0000c128 40375af4     <- AllocVec OK
                                                     <- nothing after
```

`sizeof(con)` = 55,086 (`0xd72e`), `sizeof(upconf)` = 49,448 (`0xc128`).

It is heisenbug: one traced build opened the window correctly, with the full
path logged —

```
packet 000003ed / open pkt / open name 40028014 0000001e / open window
lockpub / openwindow 4002f2f8 / window 40028144 / vt_new / vr_init / reply
```

Grid came up 77x35 for a 640x300 window. Every other attempt failed.

## The measurement trap (cost several cycles)

`dbg()` only flushes when `dbg_file && dbg_port && IsListEmpty(port)`.
`dbg_port` is assigned *after* startup, so **every trace before the first
window is silently dropped**. "No second `trace enter`" therefore proved
nothing — a second handler process's whole life is invisible.

Widening the flush to always write looks like the fix and wedges the rig:
the guard is load-bearing. The comment above it records the reason — a DOS
call made by the handler waits for its reply on our own port and would
swallow a queued packet (hit 2026-09-29, swallowed the first Open). Writing
during startup reintroduces exactly that. Do not "fix" this flush.

Consequence: the log cannot distinguish "died here" from "line never
flushed". A reliable oracle needs something else.

## Next step

Get the faulting PC. Options, best first:

1. **Exception handler that logs the PC.** `SetExcept` takes one code at a
   time, so register for the plausible ones (address error, illegal
   instruction, privilege violation, divide by zero, overflow) and write
   `code` + `oldpc` through the existing `dbg()` before dying. Cheap, and
   names the instruction. The DBG-before-death would still not flush — write
   it straight to the serial channel (`VTCON_SERIAL`) instead, which does not
   need our DOS port.
2. **Bisect the handler.** `git log -- handler/vtcon_handler.c` back through
   `d8316b1` (the XCON:/device vtwin refactor, the most likely structural
   change), building each into the rig. ~4 min a cycle.
3. Serial/GDB under the emulator if the harness supports attaching.

## Rig/tooling facts learned here

- `rig.py install` only copies the binary into `VTC:`. It does **not** run the
  kit's `Install`, so `DEVS:XCON` is absent until `Execute VTC:runkit`.
- `Install`'s `.KEY` switches need `/S`; `Install NOCONSOLE NODEVICE` fails
  "Unknown command".
- `ami.py gclick` cannot match button gadgets: the tree prints
  `custom - "Reboot"` but gclick looks for the literal `custom:Reboot`, so it
  always answers "no matching gadget". Click via
  `INPUT` op 5 with absolute screen pixels instead (PROTOCOL.md).
- `INPUT` MOVE takes Intuition pointer units (`ami.pointer_scale()`), but
  CLICK takes absolute screen coordinates. Easy to get wrong.
- `list SYS:Processes`, `list PC:` and `removesystem` do not work on this
  3.1 rig.