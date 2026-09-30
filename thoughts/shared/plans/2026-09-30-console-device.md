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
- **DD14 Unknown commands** -> IOERR_NOCMD, (amended 2026-09-30, DV4: no scrollback in device
  units -- the amiga personality keeps none, as the ROM), including CD_SETUPSCROLLBACK/POSITION (their
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

**Order (owner 2026-09-30): all AmigaOS 3.2 work -- DP5, H5.3, DV5's 3.2 row -- comes last in
the project (ledger T3). The rest does not wait for it.**

**Phase DP: probes (no product code; each writes its result into this file and R-6)**
- [x] DP1 Input chain census. `tests/amiga/chainprobe.c`: opens a SIMPLE_REFRESH window
      with IDCMP 0 and a CON: window, adds handlers at priorities 9, 5 and -5, counts
      events per class and per `ie_EventAddress`/ActiveWindow match while the rig script
      types, clicks, drags, resizes, depth-arranges and closes; prints input.device's
      handler list (name, priority; code from `tests/amiga/sizewatch.c` l.66-72).
      Rig: `tools/rig/chainprobe_rig.py`. Records: ROM console handler name/priority,
      and which classes arrive for IDCMP-less windows (fixes DD6's number, DD9's path).
      **Written, not yet run on the rig** (condev, 2026-09-30): `make build/amiga/chainprobe`;
      the console's handler is identified by is_Code inside the console.device resident
      or is_Data = the console base; RESULT lines give the classes addressed to each
      window per priority, and RAWKEY/RAWMOUSE counts while each window is active.
      **MEASURED KS 40.63 (2026-09-30, 7/7; research section 7):** ROM handler "console.device"
      at priority **0**, passes nothing on (priority -5 saw no event). IDCMP-less windows get
      SIZEWINDOW, REFRESHWINDOW, ACTIVE/INACTIVEWINDOW, CHANGEWINDOW addressed to them, and
      RAWKEY/RAWMOUSE while active, above 0. CLOSEWINDOW is not addressed to the window. No
      TIMER events below Intuition. Consequences: DD6 priority 5 stands; R1 closed; DD9
      uses REFRESHWINDOW (its TIMER fallback would never fire -- the unit's own frame clock
      is the fallback); DD7 routes CLOSEWINDOW to the active window. Same on 39.106, 40.71, 47.115 (DP6).
- [x] DP3 DosList dump. `tests/amiga/dosnode.c`: CON, RAW, XCON entries -- dn_Type,
      dn_Task, dn_Handler, dn_StackSize, dn_Priority, dn_Startup, dn_SegList (and whether
      it lies in ROM / the resident segment list), dn_GlobalVec. Every Kickstart of DD22.
      Output table goes into `device/upconsole.c` as the pristine table (DD21).
      **Written, not yet run on the rig** (condev, 2026-09-30): `make build/amiga/dosnode`,
      `tools/rig/dosnode_rig.py` (log per Kickstart in build/rig/shots/). Prints a
      `PRISTINE { dosver, name, type, handler, stack, pri, startup, SEG_NONE|SEG_DOSRES|
      SEG_ROMTAG|SEG_OTHER, segname, globvec }` line per entry, and every DLT_DEVICE entry.
      **MEASURED KS 40.63 (2026-09-30, 2/2):** CON/RAW: dn_Type 0, no handler name, stack
      3200, pri 5, startup 0/1, GlobVec -1, shared seglist 0x40010470 (disk-loaded, SEG_OTHER).
      PRISTINE lines in research section 7 / the log. Same on 39.106, 40.71, 47.115 (DP6).
- [x] DP4 ROM command census. `tests/amiga/cdprobe.c`: units 0/1/3 on a test window:
      io_Error for commands 0-14 and NSCMD_DEVICEQUERY's list; CONFLAG_NODRAW_ON_NEWSIZE;
      a wrapped 100-column line in a 60-column CONU_SNIPMAP window resized to 120 columns
      (screenshot: re-wrapped or not). Every Kickstart of DD22.
      **Written, not yet run on the rig** (condev, 2026-09-30): `make build/amiga/cdprobe`,
      `tools/rig/cdprobe_rig.py [--noscroll]`. Decided: every command goes out with SendIO
      and is AbortIO'd after 1 s (so a queued CMD_READ also measures AbortIO's io_Error,
      D1.2); CD_SETDEFAULTKEYMAP sets `AskKeyMapDefault()` (no change, no dangling
      pointer); CD_SETUPSCROLLBACK/POSITION get a zeroed ConsoleScrollback (`NOSCROLL`
      skips them if a 3.2 ROM dereferences it); also CMD_FLUSH with a read pending. The
      re-wrap is judged by CSI 6n before/after as well as the screenshot. **Plan fix:**
      120 columns of an 8-pixel font need 960 pixels, wider than the rig's 640-pixel
      Workbench; the probe uses 60/100/120 when 120 fit, else WIDE = what fits,
      NARROW = WIDE/2, LINE = WIDE-2 (same question: a line longer than the narrow width
      and shorter than the wide one). NODRAW is judged by ink pixels before/after a
      shrink+grow of a CHARMAP window, flags 0 against CONFLAG_NODRAW_ON_NEWSIZE.
      **MEASURED KS 40.63 (2026-09-30, 9/9; table in research section 7):** units 0/1/3
      identical. **CMD_RESET blocks the caller's SendIO for good on every unit** -- the probe
      skips it unless `RESET` (`--reset unit=n`, one reboot each). CMD_UPDATE -3, STOP/START/
      FLUSH -1 with nothing queued, FLUSH with a read pending 0 and the read -2, keymap
      commands 0, scrollback and NSCMD_DEVICEQUERY -3. NODRAW confirmed. **The SNIPMAP unit
      re-wraps** (45 -> 90 columns: cursor 3;11 -> 2;11) so D1.8 builds reflow. Decided for
      D1: our device answers CMD_RESET (terminal reset, reply 0) instead of copying the hang;
      every other code copies the ROM. Same on 39.106, 40.71, 47.115 (DP6).
- [ ] DP5 Medium-mode bytes (3.2 only). `tests/amiga/mediumprobe.c`: SetMode(Output(),2),
      hex-dump reads while the script types TAB, Shift+TAB, Up, Down, a line. Closes
      matrix Q9. Skipped with a written reason when no 3.2 row boots (DD22).
      **PARTLY MEASURED (2026-09-30, KS 47.115 ROM on the rig's 3.1 system disk -- a hybrid, not a
      3.2 install):** `tests/amiga/mediumprobe.c` + `tools/rig/mediumprobe_rig.py`: SetMode(fh,2)
      returns -1; TAB after typing "ab" gives the reader `9b 31 32 3b 32 3b 33 55 61 62`
      = CSI "12;2;3U" "ab" (typed characters are not delivered before it: lines stay buffered).
      After that report the con-handler took no more input and did not return the probe's next
      Write (tried: WaitForChar or plain Read; reply "\r CSI K"+line, or a single BEL; through
      the same handle or a second one; the window as pr_ConsoleTask) -- while the ROM V47 Shell
      in a CON: window completes with TAB fine. Shell side (`tests/amiga/medshell.c`: the ROM
      Shell on a PTY: slave, this program the master): it writes `0f` before its first prompt,
      and answers the "ab" report with one BEL (no match); a guessed report for "dir RAM:T"
      (CSI 12;9;10U) got no answer in 3 s, so the field meanings are not settled (12 is not
      the line length). Shift+TAB, Up, Down bytes not yet read (each try hangs the window).
      NEXT: repeat on a real 3.2 install (the owner has AmigaOS 3.2 and offered to install it,
      2026-09-30 -- a second rig disk, owner's step), vary the typed text to decode the fields.
- [x] DP6 Boot matrix: `tools/rig/rig.py` gains `--kick <file>`; record which ROMs boot the
      rig's system to Workbench with amiagent.
      **Started 2026-09-30:** `rig.py start --kick <file>` added. The default ROM
      (`kick40068.A1200`) is 40.63 by its header. KS 47.115 (`~/Desktop/KICK_323.rom`, 3.2.3)
      boots the 3.1 system disk to amiagent, but 2 of about 8 boots hung on a black screen
      before the boot script ran (a restart cured it); DP1, DP3, DP4 on it: same as 40.63
      except console.device 46.1 answers NSCMD_DEVICEQUERY (type 6, commands 0001 0002 0003
      0009-000c 4000 -- it lists CMD_RESET, which still blocks the caller). con stack 4096.
      **DONE 2026-09-30:** 39.106 (console.device 39.28, dos 39.23) and 40.71 (console.device
      40.2) boot too; DP1, DP3 and DP4 give the same results on all four ROMs (CON/RAW stack
      3200 below 47, 4096 on 47; the rest identical). Logs build/rig/shots/*-ks<ver>.log.
Success: every DP row has its numbers written here and in R-6; no code depends on an
unmeasured value.

**Phase H5: CON:/RAW: served by the XCON: handler (opt-in)**
- [x] H5.1 Handler knows its DOS name (from the startup packet's DeviceNode, `c->node`):
      CON/RAW default to AMIGA, RAW opens raw; XCON unchanged. In `parse_spec`
      (vtcon_handler.c ~l.474) defaults. Host-free; rig-checked in H5.6.
      **DONE 2026-09-30:** `node_named()` in vtcon_handler.c; CON/RAW -> AMIGA, RAW starts raw.
      Proven by H5.6 (romprobe 51/51 through the swapped CON:, a RAW: key without RETURN).
- [x] H5.2 ACTION_UNDISK_INFO (513) and V47's rule: DISK_INFO disables AUTO until
      UNDISK_INFO (RN-CH 47.1). Matrix 7.1 row updated.
      **DONE 2026-09-30:** measured the ROM first (`tests/amiga/autoprobe.c`,
      `tools/rig/autoprobe_rig.py [CON|XCON]`): AUTO close gadget = window only, reopened by
      the next write; DISK_INFO holds it; 40.x does not know UNDISK_INFO. XCON: now matches,
      plus UNDISK_INFO. The reopen exposed a hang (about 1 run in 3): vt_new's reset flushed
      damage through the renderer of the closed window -- vr_free now leaves the renderer
      empty and every drawing entry returns without a window; the frame clock stops at
      close. Fail-first on the rig (unfixed build hung on run 2), fixed build 6/6, reach.py
      PASS, cube_rig 240/240. The race is intermittent: run autoprobe_rig.py XCON several
      times (a reboot between runs) when touching window open/close.
- [ ] H5.3 Medium mode SetMode(fh,2): cooked line editing, but TAB, Shift+TAB, Up, Down
      send DP5's bytes at once. `handler/lineedit.c` gains the mode; host test in
      `tests/test_lineedit.c` (each key gives DP5's bytes, other keys edit). Lifts DD20.
- [x] H5.4 `device/upconsole.c` -> `C:UPConsole` (vbcc, ReadArgs
      `CON/K,DEVICE/K,EXCLUDE/K,STATUS/S`): CON ON/OFF per DD19/DD21, STATUS prints both
      states. Makefile target `build/amiga/UPConsole`.
      **DONE 2026-09-30:** `C:UPConsole CON ON|OFF`, `STATUS`, `HANDLER <file>` (the rig's
      VTC: copy); state in the public semaphore "UP-Term console"; refuses non-pristine
      entries (DP3 table, stack 3200 below V47) and V47 (DD20). DEVICE/EXCLUDE say "not built".
- [x] H5.5 Kit: `dist/Install` asks (default No), writes the User-Startup block;
      `dist/Uninstall` runs `UPConsole CON OFF` and removes the block; README section.
      `tools/rig/install_rig.py` gains: block written, CON ON after reboot, Uninstall
      removes it, CON: back to ROM.
      **DONE 2026-09-30:** `Execute Install [CONSOLE|NOCONSOLE]` (asks when neither; not
      offered on V47); block `;BEGIN UP-Term console` / `C:UPConsole >NIL: CON ON` /
      `;END UP-Term console`; Uninstall runs `UPConsole CON OFF`, unstartup.sh strips both
      blocks (script now also in ENVARC:up-term). install_rig.py 31/31. It caught a crash I
      had just made: the handler's teardown deleted the frame timer before close_window
      stopped it (address error on exit with a blinking cursor) -- fixed, 31/31.
- [x] H5.6 REACHABILITY `tools/rig/concon_rig.py`: `UPConsole CON ON`; `NewShell
      CON:0/20/640/200/t`; probe `tests/amiga/conwho.c` run in it sends
      ACTION_VTCON_GWINSZ to `*` -- only the vtcon handler answers it (ROM:
      ERROR_ACTION_NOT_KNOWN) -- and prints the grid; romprobe through `CON:` gives
      50/50 equal to the ROM results (`tools/probe_compare.py`); a RAW: open reads a key
      unbuffered; `CON OFF`, a new CON: window answers ERROR_ACTION_NOT_KNOWN again.
      **DONE 2026-09-30:** `tools/rig/concon_rig.py` 9/9 on KS 39.106 and 40.63 (logs
      build/rig/shots/concon*.log): ROM window refuses GWINSZ (209); after CON ON a NewShell
      CON: window answers it (UP-Term 16 77); romprobe 51/51 lines equal to the ROM; RAW: key
      0x78 without RETURN; after CON OFF 209 again. On 47.115 CON ON is refused with the DD20
      message and STATUS stays ROM.
Success: H5.6 green on 3.0 and 3.1; on 3.2 green after H5.3 or refused with the DD20
message; ledger H5 ticked.

**Phase DX: one window core for XCON: and the device**
- [x] DX1 `render/vtwin.[ch]`: move the window-binding code out of vtcon_handler.c
      (cb_damage, cb_scroll, cb_bell, cb_title, report_defaults, cb_colors, cb_layout,
      the attach half of open_window, render, frame_start, output's feed/pacing part,
      special_key, keypad_key, copy_selection, paste's text source, console_key,
      raw_report, resize's layout part, mouse_event's selection part -- R-5 table).
      Handler policy (ldisc, line editor, breaks, packets) stays in the handler; vtwin
      reports "bytes for the input" and "break key" through callbacks. No behaviour
      change: `make test`, `make test-rig`, `vttest_rig.py` 31/33 as before,
      `screen_rig.py`, `cube_rig.py`, `ptytest_rig.py` green.
      **DONE 2026-09-30 (with DX2):** `render/vtwin.[ch]` holds the window's engine, renderer,
      fonts (spec + open), title, frame clock, layout/DECCOLM, selection, copy/paste, key and
      mouse translation, raw reports; the owner's policy is `vtwin_host` (reply, input, key,
      pasted, raw, resized). vtcon_handler.c 2534 -> 1954 lines; no moved function left in it.
      Debug-only damage/scroll timers dropped. Checks: make test green, make test-rig PASS,
      vttest_rig 32/33 (only m1-s04, the known raw-mode ONLCR case; recorded baseline 31/33),
      screen_rig all ok, cube_rig 240/240, ptytest 33/33, autoprobe 2/2, concon 9/9,
      install_rig 31/31.
- [x] DX2 vtwin key input takes (code, qualifier, prev-keys, keymap) instead of an
      IntuiMessage, so the device feeds InputEvents and the handler IntuiMessages
      through one path.
      **DONE 2026-09-30:** `vtwin_key(w, code, qual, prev, secs, micros)` -- the handler passes
      the IntuiMessage fields (prev = *IAddress); a device unit passes InputEvent fields.
Success: XCON: behaves identically (the listed rig checks), vtcon_handler.c smaller by the
moved code, no duplicate of any moved function remains.

**Phase D1: the device (units, commands, input)**
- [x] D1.1 `device/upc_core.[ch]` + `tests/test_upcon.c` (suite `upcon`, in Makefile TESTS):
      read queue (partial satisfy, io_Actual, queued reads in order, CMD_CLEAR, paste text
      before later keys), event ring (64 entries, overflow counted), `upc_route` (DD7),
      `upc_conunit_fill` (DD17) on a mirror struct.
      Done on branch condev (2026-09-30): 197 checks; also compiles with vbcc for 68000 and
      68020. Decisions made there: 512 buffered input bytes per unit, overflow dropped and
      counted (`dropped`, for UPCMD_STATS); at most 8 queued CMD_READs per unit, a ninth is
      failed with IOERR_UNITBUSY by the caller; a CMD_READ of length 0 is answered at once;
      a paste is the caller's text, referenced until `paste_done` (drained or CMD_CLEAR),
      one at a time; `upc_rq_take` pops queued reads for CloseDevice. CHANGEWINDOW (V39) is
      routed like the window classes (a resize hint for DD8). The "mirror struct" is the
      296-byte image written big-endian at the matrix 6.4 offsets (the host's own struct
      would have host pointers and alignment); the test re-types every offset from the
      matrix. Fill writes the read-only block, tab stops (the unit's, or every 8 from 0,
      then 0xFFFF), pens/draw mode, font fields, cu_Modes (bit 20 LNM, 21 ASM, 22 AWM) and
      cu_RawEvents (classes 0-23); never cu_MP, cu_KeyMapStruct, cu_Obsolete1/2,
      cu_Minterms. **For D3.2 to check** (assumed, unmeasured): bit n of cu_Modes/
      cu_RawEvents = byte n/8, mask 1<<(n%8) (BSET order, not BFSET's MSB-first);
      cu_XRExtant = cu_XROrigin + cols*cu_XRSize - 1 (and Y); cu_XCP = cu_XCCP = cursor;
      cu_XMinShrink/YMinShrink supplied by the unit. **Found:** DD7 routes TIMER events
      to the active unit only, so DD9's TIMER fallback would check LAYERREFRESH for the
      active window alone; D1.3 should run that check on the unit's own 50 ms frame clock
      (DD5) instead, which every unit has.
- [x] D1.2 `device/upcon_device.c`: RomTag (RTF_AUTOINIT, NT_DEVICE, first hunk
      `moveq #-1,d0; rts`), base with the DD13 extension, Open (units -1/0/1/3, flags,
      io_Error set and io_Device cleared on failure, DD16 forward), Close, Expunge (DD15),
      BeginIO/AbortIO (AbortIO of a queued CMD_READ replies IOERR_ABORTED: the V47
      con-handler aborts its read at close, RN-CH 47.1).
      **DONE 2026-09-30:** RomTag in `device/upcon_rom.s` (first: `moveq #-1,d0; rts`, rt_Init
      = upc_init, not AUTOINIT: Init builds the base with MakeLibrary, copies the ROM's
      version, opens the ROM's CONU_LIBRARY for good, AddDevice as "UP-Term console.device").
      Base extension at fixed offsets (compile-time checked): magic 36, ROM base 40, the
      device's own "console.device" string 44 (UPConsole names the node with it -- a name
      from UPConsole's own segment died with it and the next OpenDevice loaded a DEVS: copy
      from disk). Open: -1 library, 0/1/3 on io_Data's window, UPCONFLAG_ROM forwards to the
      ROM's Open; Close through the unit process; Expunge refused while named console.device.
      Measured: KS 40.63's RemDevice frees the seglist our Expunge returns -- UPConsole's own
      UnLoadSeg after it was a double free that took the machine down.
- [x] D1.3 `device/upcon_unit.c`: the unit process (DD5) on vtwin; STANDARD/CHARMAP/
      SNIPMAP rules (DD8, DD9, DD10), NODRAW flag, frame clock, flush on UPCMD_DIE.
      **Part 2026-09-30:** the unit process on vtwin (AMIGA, the window's font, 200/0 lines of
      scrollback, reflow on for CHARMAP/SNIPMAP), CMD_WRITE after the feed, size polled on
      every write and event, REFRESHWINDOW repaint for non-STANDARD units whose window lacks
      IDCMP_REFRESHWINDOW, ConUnit filled after writes/resizes, UPCMD_DIE flush + abort reads.
      **DONE 2026-09-30:** cdprobe through our device gives every RESULT line the ROM gives
      (census, NODRAW, shrink and grow, re-wrap). Measured and copied from the ROM on the way:
      CONFLAG_NODRAW_ON_NEWSIZE clears the unit on a resize (cursor 1;1, area empty), and rows
      a shrink pushes off the top come back on a grow (engine: the reflow overflow list;
      tests `reflow_shrink_and_grow_brings_the_rows_back`, `reflow_output_scroll_forgets_the_
      pushed_rows`). NSCMD_DEVICEQUERY answered only on a ROM of 46+, as the ROM does. Copy on
      SNIPMAP only (DD10), drag select followed on the frame clock (the rig's absolute pointer
      moves never reach priority 5; POINTERPOS/NEWPOINTERPOS routed too): `tools/rig/snip_rig.py`
      4/4, XCON:'s copy included.
- [x] D1.4 `device/upcon_input.c`: the input handler (DD6, DD7), added on the first unit
      open, removed on the last close.
      **DONE 2026-09-30:** priority 5, upc_route per event, ring + Signal, chain passed on;
      added with the first unit, removed with the last (DV1: keys reach a ROM CON: Shell).
- [x] D1.5 Commands: CMD_READ, CMD_WRITE (-1 length, io_Actual, io_Length 0, io_Data
      advanced), CMD_CLEAR, NSCMD_DEVICEQUERY, private UPCMD_STATS (0x7F00: units open,
      bytes written, reads answered, events dropped) and UPCMD_DIE (0x7FF0), the rest
      IOERR_NOCMD (DD14).
      **DONE 2026-09-30:** codes as DP4 measured (CMD_STOP/START -1, FLUSH -1 or 0 + reads
      aborted), CMD_RESET answered (terminal reset), NSCMD_DEVICEQUERY type 6 with our list,
      UPCMD_STATS; unknown IOERR_NOCMD. AbortIO of a queued read: IOERR_ABORTED (not yet
      measured on the rig).
- [x] D1.6 Makefile: `build/amiga/up-console.device` (vbcc + vlink, no startup, like the
      handler, Makefile l.235-260); `make amiga` builds it; DEBUG=1 logs to
      RAM:upcon.log from the unit process only.
      **DONE 2026-09-30:** `build/amiga/up-console.device` built by `make amiga`. DEBUG=1 logs to
      the serial port (exec RawPutChar, build/rig/serial.log) instead of RAM:upcon.log: no DOS
      call, so it is safe in Open under Forbid and in the unit process alike (the handler's
      RAM: log was found racing its packets, 2026-09-30).
- [x] D1.7 Opener census (DEBUG=1): DevOpen records caller task name, unit, flags; rig
      runs the Shell, NewShell, Ed, MultiView, More, Workbench Execute Command, an
      ixemul `less`, ConClip. Table written into R-2 (replaces its "unverified" row).
      **DONE 2026-09-30:** table in the research doc (section 2): the con-handler (CON: and
      RAW: windows: Shell, NewShell, Ed, More, ixemul less) opens SNIPMAP units; MultiView only
      CONU_LIBRARY; ConClip none. Workbench Execute Command opens a CON: window: the same path.
- [x] D1.8 Reflow: if DP4 showed the ROM re-wraps linked lines, the engine gains it
      (`vt_set_reflow`, portable, host tests in `tests/test_amiga.c`, matrix updated) and
      charmap units turn it on; if DP4 showed no re-wrap, this row closes citing the
      screenshot.
      **DONE 2026-09-30 (engine; branch `d1.8-reflow`):** `vt_set_reflow(t, on)`, off by
      default (XCON: unchanged). On a width change `vt_resize` joins wrap-linked rows of the
      primary screen and types them again at the new width with the personality's wrap rule
      (amiga at once, xterm deferred), cursor on its character, attributes per cell. Not
      reflowed: alternate screen, scrollback (lines keep their width; amiga keeps none),
      double-size rows. Tests: amiga `reflow_widening_rewraps_as_the_rom_does` (the DP4 case,
      45 -> 90: cursor 2;11, rows 90 + 10), `reflow_there_and_back_restores_the_layout`,
      `reflow_line_ending_at_the_margin`, `reflow_never_joins_a_hard_newline`,
      `reflow_keeps_the_cursor_on_its_character`, `reflow_off_keeps_the_rows`; xterm
      `reflow_keeps_the_deferred_wrap`, `reflow_leaves_the_alternate_screen`. Matrix 7a
      updated. **Open, belongs to D1.3:** the device's CONU_CHARMAP / CONU_SNIPMAP units
      call `vt_set_reflow(t, 1)` (CONU_STANDARD stays off). Narrowing is not measured on the
      ROM; the engine treats it as the inverse of widening.
Success: `make test` green incl. `upcon`; the device builds; the census table exists.

**Phase D2: vectors and keymaps**
- [x] D2.1 `device/upcon_vec.s` (vasm) forward stubs for -42..-72 (DD11); our
      CDInputHandler routes then forwards.
      **DONE 2026-09-30, one decided change:** the stubs are in `device/upcon_rom.s`;
      CDInputHandler forwards only (with the ROM's base as its device argument). Our priority-5
      input handler already sees every input.device event for our units; routing the events
      a program also pushes through CDInputHandler would hand our units every key twice.
- [x] D2.2 CD_ASK/SETKEYMAP per unit, CD_ASK/SETDEFAULTKEYMAP delegated (DD12).
      **DONE 2026-09-30:** per unit in cu_KeyMapStruct (copied from AskKeyMapDefault at open);
      the unit converts keys with it (vtwin.keymap); the defaults go to the ROM's BeginIO.
- [x] D2.3 Rig probe `tests/amiga/rkcprobe.c`: RawKeyConvert over all 128 codes x
      {none, Shift, Alt, Ctrl} with the default keymap, output file before DEVICE ON and
      after: byte-identical. A CD_SETKEYMAP'd unit converts with its own map (a swapped
      two-key map changes exactly those keys).
      **DONE 2026-09-30:** `tools/rig/rkc_rig.py` 5/5 on KS 40.63: 512-line tables identical,
      a/b-swapped unit reads 62 for a, 63 for c.
Success: D2.3 identical tables; ledger D2 ticked.

**Phase D3: ConUnit fields current**
- [x] D3.1 `upc_conunit_fill` wired after writes/resizes/mode changes; compile-time
      offset asserts (DD17).
      **DONE 2026-09-30:** filled at open, after every CMD_WRITE, CMD_RESET and resize; asserts
      pin cu_KeyMapStruct 0x042, cu_RawEvents 0x125, size 0x128.
- [x] D3.2 Rig probe `tests/amiga/cudump.c`: every read-only field, tab stops, modes,
      raw-event bits, pens, font of the unit behind a CON: window, same window size and
      writes, ROM (DEVICE OFF) vs ours (DEVICE ON): equal field by field; any difference
      written here with its reason. ixemul `tests/amiga/ixtty` in a ROM CON: over our
      device: winsize equals the grid, again after a resize.
      **DONE 2026-09-30:** `tools/rig/cudump_rig.py` 0 of 15 field lines differ on KS 40.63
      (log copied to research/2026-09-30_cudump-ks40.63.log). The first run found 5 and the ROM
      said what to write: default tab list 79 stops every 8 then 0xFFFF whatever the width,
      MinShrink 9999, cu_Mask 1 and AOL pen 0 (RTG and AGA alike), text fields from the
      window's RastPort, the LNM bit. The bit numbering D1.1 assumed (bit n = byte n/8,
      1 << n%8) is the ROM's. ixemul ixtty in a ROM CON: over our device: TIOCGWINSZ 77 x 16,
      57 x 24 after a resize -- the ROM device gives the same, and re-wraps the same line.
Success: D3.2 equal (or every difference justified); ledger D3 ticked.

**Phase D4: installer and uninstall**
- [x] D4.1 `UPConsole DEVICE ON|OFF`, `EXCLUDE`, `STATUS` (DD15, DD16, DD21). Refusals
      print one line naming the conflict.
      **DONE 2026-09-30:** DEVICE ON/OFF (DD15, DD21), EXCLUDE name|CLEAR through the private
      UPCMD_EXCLUDE/EXCLUDED on the library unit (no base offsets), STATUS lists the excluded
      names. `tools/rig/devctl_rig.py` 12/12.
- [x] D4.2 Kit: Install question (default No), User-Startup block, Uninstall; README.
      `install_rig.py` extended: DEVICE ON after reboot, STATUS says on, Uninstall
      leaves the ROM device in the list (magic absent).
      **DONE 2026-09-30:** `Execute Install [DEVICE|NODEVICE]` (asks, default No; offered below
      V47 only until the 3.2 phase tests it there), DEVS:up-console.device, block `;BEGIN
      UP-Term device` / `C:UPConsole >NIL: DEVICE ON` / `;END UP-Term device`; Uninstall DEVICE
      OFF + block out; README section CONSOLE.DEVICE. install_rig.py 37/37 (device on after
      Install, the block's line switches it, after Uninstall the ROM's, S:User-Startup byte for
      byte).
- [x] D4.3 Conflict tests: `tests/amiga/patchcon.c` SetFunctions RawKeyConvert to a stub
      (test only), DEVICE ON must refuse and name the vector; restore; DEVICE ON twice
      refuses "already on"; DEVICE OFF with a unit open leaves our code until its Close
      (UPCMD_STATS through the open unit still answers; after Close the segment is gone:
      `Avail` back to the pre-install value).
      **DONE 2026-09-30 (devctl_rig.py 12/12, KS 40.63):** patched RawKeyConvert -> DEVICE ON
      refuses naming -48; ON twice -> "already"; DEVICE OFF with a unit open -> our code stays,
      the unit keeps working ("echo still here"), and after its endcli free memory is back
      within 2.3 KB of the value before DEVICE ON (the delayed Expunge path unloads too).
      Deviation: "UPCMD_STATS through the open unit" was not sent (devwho opens a new library
      unit, which is the ROM's by then); the working echo and the memory show the same.
Success: D4.2/D4.3 green; ledger D4 ticked.

**Phase D5: XCON: next to the device**
- [x] D5.1 `rom_console()` passes UPCONFLAG_ROM when the magic is present (DD18). Rig: with
      DEVICE ON, ixemul `less` in XCON: starts without the grey-window redraw and
      TIOCGWINSZ is right after a resize (ttyprobe_rig.py green).

**Phase DV: verification**
      **DONE 2026-09-30:** `upterm_device()` + UPCONFLAG_ROM (device/upc_public.h, shared by the
      device, UPConsole and the handler). Rig: DEVICE ON, ttyprobe_rig in XCON: all 9 steps,
      black window (no grey redraw), winsize 77 x 25, and devwho counts 0 units of ours.
- [x] DV1 REACHABILITY `tools/rig/condev_rig.py` (CON OFF, DEVICE ON): a ROM con-handler
      window (`NewShell CON:...`) runs `echo hi`; `tests/amiga/devwho.c` reads UPCMD_STATS
      through CONU_LIBRARY: units open >= 1 and bytes written moved by at least the
      echo's bytes (the sentinel: only our device counts); the screenshot shows "hi".
      **DONE 2026-09-30:** `tools/rig/condev_rig.py` 12/12 on KS 40.63: DEVICE ON; a ROM
      con-handler NewShell window opens a unit of ours; `echo devhi` typed (input handler ->
      unit -> CMD_READ, 11 reads answered) and run (CMD_WRITE, 67 bytes); endcli closes the
      unit; DEVICE OFF: the ROM's again, our code unloaded. Screenshot build/rig/shots/condev.png
      looks as the ROM's window.
- [x] DV2 romprobe through the ROM CON: over our device: 50/50 equal to the ROM results.
      **DONE 2026-09-30:** `tools/rig/devverify_rig.py`: 51 of 51 lines equal (KS 40.63).
- [ ] DV3 Speed, `rig.py --exact`: 2000-line `type` in a ROM CON: over our device is not
      slower than over the ROM device (the same run as ledger's 19.5 s baseline).
- [x] DV4 Memory per unit (AvailMem before/after open): CHARMAP 80x25 with 200 lines
      <= 256 KB; STANDARD <= 64 KB. Recorded here.
      **DONE 2026-09-30 (devverify_rig.py, KS 40.63, 80 x 25 of the screen font):** STANDARD
      57,224 bytes, CHARMAP 58,032 (ROM: 5,120 and 39,920); CloseDevice gives all of it back
      (ROM too). First run 90.5 KB: the engine allocated the xterm alternate screen for every
      terminal (16 bytes a cell, 32 KB) -- now made on first use (XCON: windows save it too),
      and alloc_screen's row array is zeroed so a failed allocation frees only what it made.
      DD14 amended: device units keep no scrollback -- the amiga personality keeps none, as
      the ROM, so the 200 lines were never used.
- [ ] DV5 Kickstart matrix (DD22): per row DV1, DV2, D2.3, D3.2, H5.6 results.
- [ ] DV6 Soak: 30 minutes of opening/closing CON: windows with typing, DEVICE ON: free
      memory back to its start value; a task holding signal bit 31 opens and closes a
      unit 100 times and still holds it (ibmcon 1.8 regression, R-3).

## Automated vs manual

Automated (host): `make test` (suites incl. new `upcon`, `lineedit` medium mode, engine
reflow if D1.8 adds it). Automated (rig, one at a time, rig free): chainprobe_rig.py (DP1),
dosnode_rig.py (DP3), cdprobe_rig.py (DP4), mediumprobe (DP5), concon_rig.py (H5.6), condev_rig.py (DV1, DV2, D2.3, D3.2, D4.3, DV6),
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
