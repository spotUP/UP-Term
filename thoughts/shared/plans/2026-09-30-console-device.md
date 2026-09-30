---
date: 2026-09-30
topic: UP-Term console.device replacement (ledger D0-D4) and CON:/RAW: replacement (H5)
tags: [vtcon, up-term, console.device, con-handler, CON, RAW, plan, ledger]
status: final
---

# Plan: console.device replacement + CON:/RAW: replacement

Research: `thoughts/shared/research/2026-09-30_console-device-replacement.md` (cited as
R-n for its section n). Ledger rows this plan executes: D0, D1, D2, D3, D4 and H5 of
`thoughts/shared/plans/2026-09-28-vtcon.md`; tick them there when the phases below close.

GOAL: every text window on the Amiga draws with the vtcon engine -- Shells in CON:/RAW:
(H5) and programs that open console.device on their own windows (D) -- with the ROM's
public behaviour unchanged, on AmigaOS 3.0-3.2, installable and removable without a
reboot and without SetFunction.

DONE = every ID below ticked, each reachability test green on the rig, the Kickstart
matrix (DV5) filled in for every ROM on this machine, the manual checks listed under
**Manual** handed to the owner.

## Decisions (made; do not re-litigate)

- **DD1 Public contract only** (ledger D0): the commands, units, flags and vectors of
  R-1, the public `struct ConUnit`, nothing past offset 0x128, no ROM addresses, no
  SetFunction. Where the ROM's public behaviour is unknown, a probe measures it (phase DP)
  before code depends on it.
- **DD2 Two independent opt-ins** in one tool, `C:UPConsole`: `CON ON|OFF` (H5: CON: and
  RAW: served by the XCON: handler) and `DEVICE ON|OFF` (D: console.device served by
  UP-Term). Both reversible at runtime; Install asks for each, default No; Uninstall turns
  both off. They are separate because H5 carries no device risk and covers the Shell,
  which is most of the use.
- **DD3 Order: DP -> H5 -> DX -> D1 -> D2 -> D3 -> D4 -> D5 -> DV.** H5 first: it reuses
  the proven handler whole, gives every Shell the engine + H7 editing, and needs no
  device. With CON ON the ROM con-handler no longer opens console.device for CON:, so the
  device phases then serve the remaining direct openers (census in D1.7) and systems
  where the user wants only the device.
- **DD4 Device units run the AMIGA personality**, always. The device's job is
  compatibility; xterm stays XCON:'s. Charset Latin-1, pens as the ROM (Workbench pens,
  A8's AMIGA rule).
- **DD5 One process per unit** ("UP-Term console unit"), as XCON: and ibmcon (R-3). It
  owns the `vt_term`, the renderer, the frame clock, the input buffer. `BeginIO`:
  CMD_WRITE always goes to the unit process (PutMsg to `cu_MP`, which IS the unit's
  request port) and is replied after `vt_feed` -- the grid is current, the screen follows
  at the next 50 ms frame (XCON:'s pacing, R-5). CMD_READ is answered quick when input is
  buffered, else queued. CMD_CLEAR, keymap commands, NSCMD_DEVICEQUERY run quick in the
  caller under the unit's SignalSemaphore. CloseDevice sends UPCMD_DIE, the unit renders
  what is pending, deletes ITS OWN ports (ibmcon 1.8 lesson, R-3), replies, exits.
- **DD6 One device-wide input handler**, name "UP-Term console", priority 5: below
  ixemul's 10 (IXE: "must be before console.device"), above the ROM console's handler.
  If DP1 measures the ROM handler at 5 or higher, the priority is ROM+1 (still < 10);
  if that is impossible (ROM >= 9) the handler goes at 9 and D1.4 records it. The handler
  NEVER waits and NEVER allocates (AROS deadlock note, R-1.3): it copies each event for
  one of our units into that unit's fixed 64-entry ring and Signals the unit; a full
  ring drops the event and counts it (UPCMD_STATS). It passes the chain on unconsumed
  (the ROM console has no unit on our windows).
- **DD7 Routing**: RAWKEY, RAWMOUSE, TIMER -> the unit whose `cu_Window` is
  `IntuitionBase->ActiveWindow`; ACTIVEWINDOW/INACTIVEWINDOW/SIZEWINDOW/REFRESHWINDOW/
  CLOSEWINDOW -> `ie_EventAddress` when it is one of our windows, else the active one
  (AROS's rule, R-1.3). Pure function `upc_route()` in `device/upc_core.c`, host-tested.
- **DD8 Resize is polled as well as evented**: the unit compares the window's inner size
  on every event it handles, every frame and before every write (P2 showed Intuition may
  send no SIZEWINDOW; the ROM also recomputes on write, R-1.4). Charmap units redraw
  unless CONFLAG_NODRAW_ON_NEWSIZE.
- **DD9 Refresh**: CHARMAP/SNIPMAP units repaint on REFRESHWINDOW when the window lacks
  IDCMP_REFRESHWINDOW (then nobody else refreshes it): BeginRefresh, `vr_redraw`,
  EndRefresh in the unit process. Fallback if DP1 shows no REFRESHWINDOW in the chain:
  on each TIMER event (10 Hz) the unit checks `WLayer->Flags & LAYERREFRESH` and does
  the same. STANDARD units never repaint (SMART_REFRESH: layers does it).
- **DD10 Copy/paste (SNIPMAP only)**: drag select (`vr_select`), RAmiga-C writes the
  selection to clipboard unit 0 as FTXT (`clip_write`, R-5); RAmiga-V puts the clipboard
  text into the read stream as typed characters -- the documented behaviour (NDK-DOC
  OpenDevice NOTES) -- whether or not ConClip runs; `CSI 0 SP v` is never generated.
  With raw key events asked for, the keys go out as raw reports instead (as documented).
- **DD11 Vectors**: RawKeyConvert, CDInputHandler and private 1-4 of our base forward to
  the ROM base (asm stubs: save a6, load `upc_RomBase`, jsr the same LVO, restore a6).
  Our CDInputHandler first routes the events to our units (DD7), then forwards.
- **DD12 Keymaps**: a unit's `cu_KeyMapStruct` is copied from the default at open
  (CD_ASKDEFAULTKEYMAP on the ROM); CD_ASK/SETKEYMAP read/write it; key conversion calls
  RawKeyConvert with it. CD_ASK/SETDEFAULTKEYMAP are delegated with DoIO on the
  CONU_LIBRARY request the device holds on the ROM (one system default).
- **DD13 Identity**: `lib_Version`/`lib_Revision` copied from the ROM device at install
  (programs' version checks see the running system); `lib_IdString`
  "console.device <v>.<r> (UP-Term dd.mm.yy) <gitrev>"; a base extension starts with
  `upc_Magic` = 'UPTC' and `upc_RomBase`. Tools detect UP-Term by the magic, never by
  version.
- **DD14 Unknown commands** -> IOERR_NOCMD, including CD_SETUPSCROLLBACK/POSITION (their
  ROM semantics are undocumented, R-1.1). Scrollback is our own: CHARMAP/SNIPMAP units
  keep 200 lines, Shift+PgUp/PgDn and RAmiga Up/Down move the view (XCON:'s keys,
  `console_key`); STANDARD units keep none.
- **DD15 Install without SetFunction**: `UPConsole DEVICE ON` LoadSegs
  `DEVS:up-console.device` (romtag name "console.device", NOT loadable by that name from
  DEVS:, R-4), opens the ROM device CONU_LIBRARY (kept open for good), InitResident links
  ours as "UP-Term console.device"; then under Forbid(): `Remove()` the ROM node, rename
  ours `console.device`, Permit(). Existing ROM units are untouched (their io_Device).
  `DEVICE OFF`: Forbid(); `RemDevice(ours)` (our Expunge Remove()s the node and frees
  when OpenCnt is 0, else sets LIBF_DELEXP and goes on the last Close); `AddHead` the
  ROM node; Permit(). Survives nothing across a reboot: Install puts `UPConsole DEVICE ON`
  in S:User-Startup between `;BEGIN UP-Term console` / `;END UP-Term console` lines
  (the boot Shell window keeps the ROM device: it opened before User-Startup).
- **DD16 Forward-to-ROM opens**: flag bit `UPCONFLAG_ROM` (0x40000000, only sent after
  checking the magic) and an exclusion list of task names (loaded into the base by
  `UPConsole EXCLUDE name|CLEAR`; DevOpen cannot use DOS) make our DevOpen call the ROM
  device's Open vector with io_Device = ROM base (what exec's OpenDevice does, same
  Forbid context); the caller then talks to the ROM unit directly. This is the escape
  hatch for any program that turns out to depend on ROM internals.
- **DD17 ConUnit**: the unit struct begins with a real 296-byte `struct ConUnit`
  (`io_Unit` points at it). After every write batch, resize and mode change the unit
  process fills the read-only fields and the pens/font/modes/raw-event bits from the
  engine and window via `upc_conunit_fill()` (portable, host-tested against a mirror
  struct); compile-time asserts pin `offsetof(cu_RawEvents) == 0x125` and
  `sizeof(struct ConUnit) == 0x128`. Writes by programs to read/write fields: only
  `cu_KeyMapStruct` is honoured (it is the keymap), the rest are overwritten at the next
  fill (documented in the kit README).
- **DD18 XCON: with the device installed**: `rom_console()` checks the magic and opens
  with `UPCONFLAG_ROM`, so its DISK_INFO unit stays a ROM unit exactly as today (no
  second engine on its window).
- **DD19 CON:/RAW: swap (H5)**: `UPConsole CON ON` LoadSegs `L:vtcon-handler` once (kept
  loaded for good: open windows may run it) and, under `LockDosList(LDF_DEVICES|LDF_WRITE)`,
  points the CON and RAW entries at it in the way DP3 measures DOS uses (dn_SegList when
  DOS starts CON: from a seglist; dn_Handler + dn_SegList 0 when it loads by name), with
  dn_Task 0, dn_GlobalVec -1, dn_StackSize 16000. The pristine values are saved in a
  public SignalSemaphore "UP-Term console" (found again by later runs) and put back by
  `CON OFF`. The handler takes its personality from its DOS name: CON/RAW -> AMIGA, RAW
  starts raw, XCON -> XTERM as today.
- **DD20 V47 (3.2) CON ON is refused until H5.3 lands**: the V47 Shell needs medium mode
  (SetMode 2); today XCON: treats it as raw (R-4).
- **DD21 Conflicts refuse, never stack** (ledger D0): DEVICE ON refuses when any vector
  -6..-72 of the ROM console base points outside `[resident, rt_EndSkip)` of
  `FindResident("console.device")` (a SetFunction patch), when a console.device node
  with our magic exists ("already on"), or when the node found is not the resident's
  device. CON ON refuses when the CON/RAW entries differ from DP3's pristine table for
  the running dos.library version, naming what it found (KingCON, ViNCEd...).
- **DD22 Kickstart matrix = the ROMs on this machine** (R-4): 3.0 39.106, 3.1 40.068,
  3.5 40.071, `KICK_323.rom` if it boots the rig's system to Workbench. No 3.1.4 ROM
  exists here: its row reads "not tested: no ROM on this machine" and the owner's A1200
  (V4) is the real-hardware row.
- **DD23 Memory**: unit scrollback 200 lines (CHARMAP/SNIPMAP), 0 (STANDARD); override
  with `ENV:UP-Term/ConsoleScrollback`, read by the unit process (not DevOpen).

## Constraints for whoever implements

- The rig is shared: run rig items only when no other session holds it; kill emulators by
  config path only. `handler/vtcon_handler.c` was being edited by another session on
  2026-09-30: H5/DX/D5 start after that work is committed.
- Rule 1: everything under `device/upc_core.*` and engine changes is portable C89, host-
  tested; OS code stays in `device/upcon_*.c`, `render/vtwin.c`.
- Rule 4: one reachability test per feature (H5.6, DV1); every other behaviour in host
  suites or targeted rig probes.
- No mutable statics in device/unit code except library bases (every unit runs the same
  segment; R-5 build facts).

## Checklist

**Phase DP: probes (no product code; each writes its result into this file and R-6)**
- [ ] DP1 Input chain census. `tests/amiga/chainprobe.c`: opens a SIMPLE_REFRESH window
      with IDCMP 0 and a CON: window, adds handlers at priorities 9, 5 and -5, counts
      events per class and per `ie_EventAddress`/ActiveWindow match while the rig script
      types, clicks, drags, resizes, depth-arranges and closes; prints input.device's
      handler list (name, priority; code from `tests/amiga/sizewatch.c` l.66-72).
      Rig: `tools/rig/conprobe_rig.py dp1`. Records: ROM console handler name/priority,
      and which classes arrive for IDCMP-less windows (fixes DD6's number, DD9's path).
- [ ] DP3 DosList dump. `tests/amiga/dosnode.c`: CON, RAW, XCON entries -- dn_Type,
      dn_Task, dn_Handler, dn_StackSize, dn_Priority, dn_Startup, dn_SegList (and whether
      it lies in ROM / the resident segment list), dn_GlobalVec. Every Kickstart of DD22.
      Output table goes into `device/upconsole.c` as the pristine table (DD21).
- [ ] DP4 ROM command census. `tests/amiga/cdprobe.c`: units 0/1/3 on a test window:
      io_Error for commands 0-14 and NSCMD_DEVICEQUERY's list; CONFLAG_NODRAW_ON_NEWSIZE;
      a wrapped 100-column line in a 60-column CONU_SNIPMAP window resized to 120 columns
      (screenshot: re-wrapped or not). Every Kickstart of DD22.
- [ ] DP5 Medium-mode bytes (3.2 only). `tests/amiga/mediumprobe.c`: SetMode(Output(),2),
      hex-dump reads while the script types TAB, Shift+TAB, Up, Down, a line. Closes
      matrix Q9. Skipped with a written reason when no 3.2 row boots (DD22).
- [ ] DP6 Boot matrix: `tools/rig/rig.py` gains `--kick <file>`; record which ROMs boot the
      rig's system to Workbench with amiagent.
Success: every DP row has its numbers written here and in R-6; no code depends on an
unmeasured value.

**Phase H5: CON:/RAW: served by the XCON: handler (opt-in)**
- [ ] H5.1 Handler knows its DOS name (from the startup packet's DeviceNode, `c->node`):
      CON/RAW default to AMIGA, RAW opens raw; XCON unchanged. In `parse_spec`
      (vtcon_handler.c ~l.474) defaults. Host-free; rig-checked in H5.6.
- [ ] H5.2 ACTION_UNDISK_INFO (513) and V47's rule: DISK_INFO disables AUTO until
      UNDISK_INFO (RN-CH 47.1). Matrix 7.1 row updated.
- [ ] H5.3 Medium mode SetMode(fh,2): cooked line editing, but TAB, Shift+TAB, Up, Down
      send DP5's bytes at once. `handler/lineedit.c` gains the mode; host test in
      `tests/test_lineedit.c` (each key gives DP5's bytes, other keys edit). Lifts DD20.
- [ ] H5.4 `device/upconsole.c` -> `C:UPConsole` (vbcc, ReadArgs
      `CON/K,DEVICE/K,EXCLUDE/K,STATUS/S`): CON ON/OFF per DD19/DD21, STATUS prints both
      states. Makefile target `build/amiga/UPConsole`.
- [ ] H5.5 Kit: `dist/Install` asks (default No), writes the User-Startup block;
      `dist/Uninstall` runs `UPConsole CON OFF` and removes the block; README section.
      `tools/rig/install_rig.py` gains: block written, CON ON after reboot, Uninstall
      removes it, CON: back to ROM.
- [ ] H5.6 REACHABILITY `tools/rig/concon_rig.py`: `UPConsole CON ON`; `NewShell
      CON:0/20/640/200/t`; probe `tests/amiga/conwho.c` run in it sends
      ACTION_VTCON_GWINSZ to `*` -- only the vtcon handler answers it (ROM:
      ERROR_ACTION_NOT_KNOWN) -- and prints the grid; romprobe through `CON:` gives
      50/50 equal to the ROM results (`tools/probe_compare.py`); a RAW: open reads a key
      unbuffered; `CON OFF`, a new CON: window answers ERROR_ACTION_NOT_KNOWN again.
Success: H5.6 green on 3.0 and 3.1; on 3.2 green after H5.3 or refused with the DD20
message; ledger H5 ticked.

**Phase DX: one window core for XCON: and the device**
- [ ] DX1 `render/vtwin.[ch]`: move the window-binding code out of vtcon_handler.c
      (cb_damage, cb_scroll, cb_bell, cb_title, report_defaults, cb_colors, cb_layout,
      the attach half of open_window, render, frame_start, output's feed/pacing part,
      special_key, keypad_key, copy_selection, paste's text source, console_key,
      raw_report, resize's layout part, mouse_event's selection part -- R-5 table).
      Handler policy (ldisc, line editor, breaks, packets) stays in the handler; vtwin
      reports "bytes for the input" and "break key" through callbacks. No behaviour
      change: `make test`, `make test-rig`, `vttest_rig.py` 31/33 as before,
      `screen_rig.py`, `cube_rig.py`, `ptytest_rig.py` green.
- [ ] DX2 vtwin key input takes (code, qualifier, prev-keys, keymap) instead of an
      IntuiMessage, so the device feeds InputEvents and the handler IntuiMessages
      through one path.
Success: XCON: behaves identically (the listed rig checks), vtcon_handler.c smaller by the
moved code, no duplicate of any moved function remains.

**Phase D1: the device (units, commands, input)**
- [ ] D1.1 `device/upc_core.[ch]` + `tests/test_upcon.c` (suite `upcon`, in Makefile TESTS):
      read queue (partial satisfy, io_Actual, queued reads in order, CMD_CLEAR, paste text
      before later keys), event ring (64 entries, overflow counted), `upc_route` (DD7),
      `upc_conunit_fill` (DD17) on a mirror struct.
- [ ] D1.2 `device/upcon_device.c`: RomTag (RTF_AUTOINIT, NT_DEVICE, first hunk
      `moveq #-1,d0; rts`), base with the DD13 extension, Open (units -1/0/1/3, flags,
      io_Error set and io_Device cleared on failure, DD16 forward), Close, Expunge (DD15),
      BeginIO/AbortIO (AbortIO of a queued CMD_READ replies IOERR_ABORTED: the V47
      con-handler aborts its read at close, RN-CH 47.1).
- [ ] D1.3 `device/upcon_unit.c`: the unit process (DD5) on vtwin; STANDARD/CHARMAP/
      SNIPMAP rules (DD8, DD9, DD10), NODRAW flag, frame clock, flush on UPCMD_DIE.
- [ ] D1.4 `device/upcon_input.c`: the input handler (DD6, DD7), added on the first unit
      open, removed on the last close.
- [ ] D1.5 Commands: CMD_READ, CMD_WRITE (-1 length, io_Actual, io_Length 0, io_Data
      advanced), CMD_CLEAR, NSCMD_DEVICEQUERY, private UPCMD_STATS (0x7F00: units open,
      bytes written, reads answered, events dropped) and UPCMD_DIE (0x7FF0), the rest
      IOERR_NOCMD (DD14).
- [ ] D1.6 Makefile: `build/amiga/up-console.device` (vbcc + vlink, no startup, like the
      handler, Makefile l.235-260); `make amiga` builds it; DEBUG=1 logs to
      RAM:upcon.log from the unit process only.
- [ ] D1.7 Opener census (DEBUG=1): DevOpen records caller task name, unit, flags; rig
      runs the Shell, NewShell, Ed, MultiView, More, Workbench Execute Command, an
      ixemul `less`, ConClip. Table written into R-2 (replaces its "unverified" row).
- [ ] D1.8 Reflow: if DP4 showed the ROM re-wraps linked lines, the engine gains it
      (`vt_set_reflow`, portable, host tests in `tests/test_amiga.c`, matrix updated) and
      charmap units turn it on; if DP4 showed no re-wrap, this row closes citing the
      screenshot.
Success: `make test` green incl. `upcon`; the device builds; the census table exists.

**Phase D2: vectors and keymaps**
- [ ] D2.1 `device/upcon_vec.s` (vasm) forward stubs for -42..-72 (DD11); our
      CDInputHandler routes then forwards.
- [ ] D2.2 CD_ASK/SETKEYMAP per unit, CD_ASK/SETDEFAULTKEYMAP delegated (DD12).
- [ ] D2.3 Rig probe `tests/amiga/rkcprobe.c`: RawKeyConvert over all 128 codes x
      {none, Shift, Alt, Ctrl} with the default keymap, output file before DEVICE ON and
      after: byte-identical. A CD_SETKEYMAP'd unit converts with its own map (a swapped
      two-key map changes exactly those keys).
Success: D2.3 identical tables; ledger D2 ticked.

**Phase D3: ConUnit fields current**
- [ ] D3.1 `upc_conunit_fill` wired after writes/resizes/mode changes; compile-time
      offset asserts (DD17).
- [ ] D3.2 Rig probe `tests/amiga/cudump.c`: every read-only field, tab stops, modes,
      raw-event bits, pens, font of the unit behind a CON: window, same window size and
      writes, ROM (DEVICE OFF) vs ours (DEVICE ON): equal field by field; any difference
      written here with its reason. ixemul `tests/amiga/ixtty` in a ROM CON: over our
      device: winsize equals the grid, again after a resize.
Success: D3.2 equal (or every difference justified); ledger D3 ticked.

**Phase D4: installer and uninstall**
- [ ] D4.1 `UPConsole DEVICE ON|OFF`, `EXCLUDE`, `STATUS` (DD15, DD16, DD21). Refusals
      print one line naming the conflict.
- [ ] D4.2 Kit: Install question (default No), User-Startup block, Uninstall; README.
      `install_rig.py` extended: DEVICE ON after reboot, STATUS says on, Uninstall
      leaves the ROM device in the list (magic absent).
- [ ] D4.3 Conflict tests: `tests/amiga/patchcon.c` SetFunctions RawKeyConvert to a stub
      (test only), DEVICE ON must refuse and name the vector; restore; DEVICE ON twice
      refuses "already on"; DEVICE OFF with a unit open leaves our code until its Close
      (UPCMD_STATS through the open unit still answers; after Close the segment is gone:
      `Avail` back to the pre-install value).
Success: D4.2/D4.3 green; ledger D4 ticked.

**Phase D5: XCON: next to the device**
- [ ] D5.1 `rom_console()` passes UPCONFLAG_ROM when the magic is present (DD18). Rig: with
      DEVICE ON, ixemul `less` in XCON: starts without the grey-window redraw and
      TIOCGWINSZ is right after a resize (ttyprobe_rig.py green).

**Phase DV: verification**
- [ ] DV1 REACHABILITY `tools/rig/condev_rig.py` (CON OFF, DEVICE ON): a ROM con-handler
      window (`NewShell CON:...`) runs `echo hi`; `tests/amiga/devwho.c` reads UPCMD_STATS
      through CONU_LIBRARY: units open >= 1 and bytes written moved by at least the
      echo's bytes (the sentinel: only our device counts); the screenshot shows "hi".
- [ ] DV2 romprobe through the ROM CON: over our device: 50/50 equal to the ROM results.
- [ ] DV3 Speed, `rig.py --exact`: 2000-line `type` in a ROM CON: over our device is not
      slower than over the ROM device (the same run as ledger's 19.5 s baseline).
- [ ] DV4 Memory per unit (AvailMem before/after open): CHARMAP 80x25 with 200 lines
      <= 256 KB; STANDARD <= 64 KB. Recorded here.
- [ ] DV5 Kickstart matrix (DD22): per row DV1, DV2, D2.3, D3.2, H5.6 results.
- [ ] DV6 Soak: 30 minutes of opening/closing CON: windows with typing, DEVICE ON: free
      memory back to its start value; a task holding signal bit 31 opens and closes a
      unit 100 times and still holds it (ibmcon 1.8 regression, R-3).

## Automated vs manual

Automated (host): `make test` (suites incl. new `upcon`, `lineedit` medium mode, engine
reflow if D1.8 adds it). Automated (rig, one at a time, rig free): conprobe_rig.py
(DP1-DP5), concon_rig.py (H5.6), condev_rig.py (DV1, DV2, D2.3, D3.2, D4.3, DV6),
install_rig.py (H5.5, D4.2), the existing XCON: checks after DX (test-rig, vttest_rig,
screen_rig, cube_rig, ptytest_rig, ttyprobe_rig).

**Manual (owner; never ticked by the agent)**
- M1 A1200 (real hardware, V4): Install with both options Yes, reboot; Workbench Shell
  icon and `NewShell` open UP-Term CON: windows; type, resize, copy/paste with RAmiga C/V,
  Shift+PgUp scrollback. PASS = looks and behaves like the ROM Shell, plus scrollback.
- M2 Same machine, CON OFF, DEVICE ON: open Ed on a file, scroll, resize; the Shell;
  close everything. PASS = no visual difference from the ROM, no crash.
- M3 Uninstall, reboot: ROM console everywhere (STATUS: both off).

## Risks

- **R1 (biggest) Input routing rests on DP1.** If Intuition does not pass RAWKEY/mouse
  events for IDCMP-less windows below itself, a device unit gets no keys; the ROM console
  proves they arrive somewhere, so DP1 finds where before D1.4 is written. Refresh and
  resize have designed fallbacks (DD8, DD9).
- R2 Programs relying on ROM internals (private ConUnit tail, the private snip interface,
  drawing into the console's area): crash or glitch only under DEVICE ON. Mitigation:
  DEVICE is opt-in, the census (D1.7) names direct openers, EXCLUDE forwards a named
  program to the ROM (DD16).
- R3 Deadlock with layer locks during a window drag (AROS note): the handler never waits
  (DD6); CMD_WRITE callers wait for the unit, which may wait for the layer, which is the
  ROM's behaviour too ("the RPort is in use while this write is pending").
- R4 The V47 con-handler's reliance on line linking (RN-CH 47.2): covered by D1.8 if the
  ROM re-wraps; unverifiable without a 3.2 row (DD22).
- R5 Memory on small machines: a process + grid per console window (DV4, DD23).
- R6 Third-party patches installed after UPConsole (they would SetFunction our base):
  out of our control; STATUS reports vectors outside our segment.
