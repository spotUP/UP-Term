---
date: 2026-09-30
topic: console.device replacement (ledger D0-D4) and the CON:/RAW: replacement (H5) -- what exists
tags: [vtcon, up-term, console.device, con-handler, CON, RAW, conunit, keymap, input.device, exec-device, research]
status: final
---

# console.device replacement and CON:/RAW: replacement: what exists

Documentary. Opinions and decisions live in the plan
(`thoughts/shared/plans/2026-09-30-console-device.md`). Line numbers in
`handler/vtcon_handler.c` are of the WORKING TREE on 2026-09-30 (HEAD 1f64cad plus
uncommitted edits of another session); they will drift, the function names will not.

Citation keys from the conformance matrix section 0
(`thoughts/shared/research/2026-09-28_console-conformance-matrix.md`) are reused:
H-CON, H-CU, NDK-DOC, FD20, OFFS20, RKM-xx, AM-xx, RN-CH, RN-CD, DOSDOC. New keys:

| Key | File |
|-----|------|
| NDK-EXEC | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Autodocs/exec.doc` (AddDevice, RemDevice, OpenDevice, InitResident, FindName, SetFunction) |
| NDK-FD | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/FD/console_lib.fd` |
| NDK-INC | `~/opt/amiga/m68k-amigaos/ndk-include/devices/{console,conunit}.h` -- byte-identical to the `.ndk/Include_H` copies (checked with `diff`) |
| RN-CC | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/ReleaseNotes/ConClip-RelNotes` |
| AROS | AROS `rom/devs/console/` at commit `4db224352dac78ce279056cf2cae47ed6b096a8e` (github.com/aros-development-team/AROS), files `console.conf`, `cdinputhandler.c`, `console.c`, `consoletask.c`, fetched to the session scratchpad. A clean-room reimplementation: evidence of what a compatible device must do, NOT of what the ROM does |
| IXE | `/Users/spot/Code/ixemul-vtcon/library/ix_sigwinch.c`, `library/__tioctl.c` (the patched 48.2 tree) |
| IBMCON | `/Users/spot/Code/dctelnet-petscii-recovered/ibmcon/` (README.md, ibmcon.device.bugfixed.asm header l.1-160) |

## 1. The console.device interface

### 1.1 Commands (H-CON, NDK-DOC; full table in matrix 6.1)

| Command | Value | What a replacement must do |
|---------|-------|----------------------------|
| `CMD_READ` | 2 | Queue until input exists; then satisfy with what is available up to io_Length, set io_Actual (NDK-DOC CMD_READ). Input = keymap-converted keys, special-key CSI reports (matrix 5.3), raw event reports asked for with `CSI n {` (matrix 5.1/5.2), replies (CPR, window bounds), paste text or the paste report. |
| `CMD_WRITE` | 3 | io_Length -1 = NUL-terminated. Interpret and draw. Results io_Actual = bytes, and (matrix 6.1, AD20) io_Length 0, io_Data advanced. "The RPort of the console window is in use while this write command is pending" (NDK-DOC). |
| `CMD_CLEAR` | 5 | Drop pending input/reports (NDK-DOC). |
| `CD_ASKKEYMAP` / `CD_SETKEYMAP` | 9 / 10 | Copy the unit's `struct KeyMap` (32 bytes, 8 pointers) out / in. |
| `CD_ASKDEFAULTKEYMAP` / `CD_SETDEFAULTKEYMAP` | 11 / 12 | Device default (used for new units and `RawKeyConvert(..., NULL)`); since V36 SET does not copy, the struct must stay in memory (matrix 6.1). |
| `CD_SETUPSCROLLBACK` / `CD_SETSCROLLBACKPOSITION` | 13 / 14 | 3.2 header additions (`struct ConsoleScrollback { APTR cs_ScrollerGadget; UWORD cs_NumLines; }`, H-CON l.25-26, l.102-106). **No autodoc, no release note: semantics unknown** (matrix Q10; looked in NDK-DOC, RN-CD, RN-CH). |
| `NSCMD_DEVICEQUERY` | 0x4000 | Supported by the ROM device; 45.4 lists itself (RN-CD). |
| CMD_RESET/UPDATE/STOP/START/FLUSH | 1/4/6/7/8 | Whether the ROM accepts them: **undocumented** (matrix Q10). AROS answers only its command list (`console.c` l.51-52 lists CMD_READ, CMD_WRITE ...). |

All document IOF_QUICK as possible: "mn_ReplyPort set if quick I/O is not possible".

### 1.2 Units and flags (H-CU, NDK-DOC OpenDevice)

`OpenDevice("console.device", unit, ioreq, flags)`, `io_Data` = the `struct Window *`
(required unless unit -1).

| Unit | Value | Contract |
|------|-------|----------|
| `CONU_LIBRARY` | -1 | Only fills io_Device (the base for RawKeyConvert/CDInputHandler); must still be closed. |
| `CONU_STANDARD` | 0 | "Binds the supplied window to a unique console. Sharing a console must be done at a level higher than the device." No map. |
| `CONU_CHARMAP` | 1 | (V36) Character map kept "to restore obscured portions of windows which are revealed, and to redraw a window after a resize". "Must be opened as SIMPLE REFRESH windows." The map is private. |
| `CONU_SNIPMAP` | 3 | (V36) As 1, plus mouse highlight and RIGHT AMIGA C copy. |
| flag `CONFLAG_NODRAW_ON_NEWSIZE` | 1 | (V37) Charmap units do not redraw on resize; ignored for unit 0. |

Copy/paste contract (NDK-DOC OpenDevice NOTES): RAmiga-C/RAmiga-V are swallowed unless
the program asked for raw key events. Without a clipboard utility the snip goes to a
private device buffer and RAmiga-V puts the text in the read stream "as if the user had
typed all of the characters"; with the console-to-clipboard utility (ConClip) the snip
goes to clipboard.device and RAmiga-V puts `CSI 0 SP v` (9B 30 20 76) in the read
stream, "left up to the application" (the con-handler pastes the clipboard).

RKM-98 caveats: one console per window; do not mix graphics.library rendering in the
console's area; charmap units should take Intuition events as raw events, not IDCMP.
AM-30: closing the window before the console crashes.

### 1.3 Library vectors (NDK-FD)

```
##base _ConsoleDevice
##bias 42
##public
CDInputHandler(events,consoleDevice)(a0/a1)          -42
RawKeyConvert(events,buffer,length,keyMap)(a0/a1,d1/a2)   -48
##private
consolePrivate1..4()()                               -54 -60 -66 -72  (V36)
```

- **CDInputHandler**: "Accept input events from the producer, which is usually the rom
  input.task ... returns events not used by this handler. This function is available for
  historical reasons" (NDK-DOC). I.e. it is the console's input.device handler. AROS
  installs it as an input handler named `"console.device InputHandler"` at **priority 0**
  (`cdinputhandler.c` l.243-244) and routes RAWKEY (down only), SIZEWINDOW, CLOSEWINDOW,
  REFRESHWINDOW, GADGETDOWN/UP, MENULIST, RAWMOUSE, ACTIVEWINDOW, INACTIVEWINDOW and
  TIMER to the unit whose window is the active one (or ie_EventAddress for
  (IN)ACTIVEWINDOW), passing the chain on unconsumed. AROS's comment on why the handler
  PutMsg()s a copy and never waits for the console task: "during an interactive
  size/drag intuition holds LockLayers() across many input events, so waiting for the
  console task while it waits for the layer lock deadlocks all input".
- ixemul installs its SIGWINCH handler at priority 10 with the comment "must be before
  console.device" (IXE `ix_sigwinch.c` l.104): it expects console.device's handler
  below 10 and IECLASS_SIZEWINDOW events for CON: windows in the chain.
  **The ROM handler's real priority and name are unmeasured** (no recorded run of
  `tests/amiga/sizewatch`, which prints the handler list, l.66-72).
- P2 measured (ledger P2) that KS 3.1 Intuition put NO IECLASS_SIZEWINDOW into the chain
  for a drag of an XCON: window. XCON: windows have IDCMP_NEWSIZE. Whether Intuition
  forwards the class for windows WITHOUT that IDCMP flag (a con-handler window) is
  unmeasured; ixemul's design and AROS both assume it does.
- **RawKeyConvert**: IECLASS_RAWKEY only; keyMap NULL = device default; d0 = bytes, -1 on
  overflow (matrix 6.3).
- **Private 1..4**: AROS names them `GetConSnip()`, `SetConSnip(a0)`,
  `AddConSnipHook(a0 hook)`, `RemConSnipHook(a0 hook)` -- "Undocumented OS2-3.x only
  C:ConClip functions" (AROS `console.conf`). The NDK gives no names or signatures.
  ConClip V47 has "a new interface" to the V47 con-handler (RN-CH 47.10, RN-CC 47.2);
  what that interface is: **unverified** (not in the NDK).

### 1.4 struct ConUnit (H-CU; offsets verified in OFFS20, table in matrix 6.4)

296 bytes. `cu_MP` (MsgPort, 34), then "read only variables": `cu_Window`, `cu_XCP/YCP`
(character position), `cu_XMax/YMax` (max character position), `cu_XRSize/YRSize`
(character raster size), `cu_XROrigin/YROrigin`, `cu_XRExtant/YRExtant`,
`cu_XMinShrink/YMinShrink`, `cu_XCCP/YCCP` (cursor); "read/write variables (writes must
be protected)": `cu_KeyMapStruct` (storage for Ask/SetKeyMap), `cu_TabStops[80]` (0 at
start, 0xFFFF at end), `cu_Mask`, `cu_FgPen`, `cu_BgPen`, `cu_AOLPen`, `cu_DrawMode`,
2 obsolete fields, `cu_Minterms[8]`, `cu_Font`, `cu_AlgoStyle`, `cu_TxFlags`,
`cu_TxHeight/Width/Baseline`, `cu_TxSpacing`, `cu_Modes[3]` (bit 20 LNM, 21 ASM, 22 AWM),
`cu_RawEvents[3]` (a bit per IECLASS). Anything after offset 0x128 in a ROM unit is
private; the public header is identical in NDK 3.2 (`conunit.h 47.1`) and ADCD 2.1.

Known readers of ConUnit fields:

| Reader | Fields | Source |
|--------|--------|--------|
| ixemul 48.2 TIOCGWINSZ (non-vtcon consoles) | `cu_Window` (checked against id_VolumeNode), `cu_XMax+1`, `cu_YMax+1` | IXE `__tioctl.c` l.255-270 |
| XCON:'s own `sync_size()` | WRITES `cu_XMax/YMax` of the ROM unit it opened for DISK_INFO callers | `vtcon_handler.c:1914` |
| con-handler, other programs | **unknown**: no source or document lists readers; see plan item D1.7 (opener census) | looked in NDK-DOC, RN-CH, AM-65 |

The ledger note that "the ROM console recomputes cu_XMax/YMax only when written to"
(P2) is a rig measurement on KS 3.1.

## 2. Who uses console.device

| User | How | Confidence |
|------|-----|------------|
| ROM con-handler (CON:, RAW:, the Shell, NewShell, Workbench "Shell" icon, `Open("*")`) | One console unit per window; SIMPLE_REFRESH + snip-mapped unit by default, SMART option selects CONU_STANDARD (LIB-572, AM-30, matrix 7.3); CMD_READ always pending; asks raw events for the close gadget (RN-CH 47.1: "forgot to re-enable the window close event"); answers DISK_INFO with its console IOStdReq in id_InUse (AM-65); V47 relies on the device keeping wrapped lines "linked" for resize (RN-CH 47.2) and aborts its pending read at close (RN-CH 47.1) | documented |
| ixemul programs in a ROM CON: | DISK_INFO -> the con-handler's IOStdReq -> ConUnit size (TIOCGWINSZ); input handler for SIGWINCH | IXE source |
| XCON: (vtcon_handler.c) | `CONU_LIBRARY` at start for `RawKeyConvert` (`handler_main`, l.2295-2296; closed at l.2408-2409); a `CONU_STANDARD` unit on its own window, opened on the first DISK_INFO (`rom_console`, l.1976), cursor switched off (`CSI 0 SP p`), then the window redrawn because opening the unit cleared it | code |
| ConClip | The private snip vectors (AROS names), clipboard unit 0 | AROS, RN-CC |
| Ed, MultiView, More, other Intuition programs opening console.device on their own windows | **Unverified.** No local source or document says which system programs open console.device directly. The task brief names Ed and MultiView; the plan measures it (item D1.7, an opener log in a debug build of the new device) instead of assuming | not confirmed; looked in ADCD Libraries/Devices manuals (examples only), NDK ReleaseNotes (`grep -l console`), no Ed/MultiView source here |
| Third-party console patches (KingCON, ViNCEd, MCP, NewEdit-style SetFunction patches) | Replace con-handler and/or patch console.device vectors | from knowledge (ledger D0); none installed on the rig (unverified) |
| DCTelnet | Does NOT use console.device: it opens its own `ibmcon.device` (IBMCON) | IBMCON README |

## 3. The ibmcon.device precedent (IBMCON)

A third-party Exec device DCTelnet draws with, reassembled and maintained in
`~/Code/dctelnet-petscii-recovered/ibmcon/`:

- RTF_AUTOINIT RomTag; DevInit builds a root base. **Every OpenDevice gets a private
  clone of the whole device base** and a private handler process (`IBMCON_Handler`);
  io_Device is repointed at the clone. DevBeginIO forwards CMD_WRITE to the handler's
  port; the handler owns all rendering state (a `con` struct on its stack) and replies.
  CloseDevice sends private `CMD_DIE` ($7FF0).
- Private commands $7FE0-$7FE3 (SETPENS, GETCURSOR, GETMODES, READTEXT) for things the
  byte stream cannot carry.
- Bugs it had that a replacement must not repeat (README 1.8, fix list l.100-150):
  the handler's message port deleted in the opener's task freed the OPENER's signal bit
  (bsdsocket's bit 31: connections hung) -- a port must be deleted by the task that
  created it; DevOpen must set io_Error and clear io_Device on failure; AllocSignal's -1
  must be sign-extended; column counts must be clamped to buffer sizes.
- Only CMD_WRITE; no input side, no ConUnit, no keymap. It is the precedent for the
  process-per-unit structure and the build (vasm, `-Fhunkexe`), not for the interface.

## 4. Installing a replacement without SetFunction

Facts from NDK-EXEC:

- `OpenDevice(name, ...)`: "The device may exist in memory, or on disk" -- it looks the
  name up in `SysBase->DeviceList` (FindName, case-sensitive: NDK-EXEC OpenDevice BUGS)
  and loads from DEVS: only when absent. So a disk file named `console.device` is never
  loaded while the ROM one is in the list.
- `AddDevice(device)` "adds a new device to the system device list". Whether it enqueues
  by priority or appends is **not stated** (NDK-EXEC). `FindName` returns the first node
  with the name.
- `RemDevice(device)` "calls the device's EXPUNGE vector ... The device may refuse to do
  this if it is busy or currently open". The ROM console.device is open by every console
  window, so RemDevice cannot remove it; removing it from the list must be done with
  `Remove()` of its node under `Forbid()` (the node stays valid: ROM, or a ROM-update
  module that is never unloaded).
- `InitResident(romtag, seglist)` with RTF_AUTOINIT calls MakeLibrary with the four init
  longwords and links the returned base into the device list (NDK-EXEC InitResident).
- `SetFunction` exists for patching vectors; the owner's direction (ledger D0) is not to
  use it.
- console.device versions: 40.x on 3.1; 45.1-45.4 on 3.1.4 (RN-CD, "AmigaOS 3.1.4
  project"); 46.1 on 3.2, "bumped to V46 so the module is automatically loaded on 3.2"
  (RN-CD) -- on 3.1.4/3.2 the device can be a RAM module from the ROM update, so its code
  is not necessarily in the $F80000 ROM range.
- Existing openers keep working after a list swap: an IORequest carries io_Device, not a
  name; CloseDevice/BeginIO go through that pointer.

CON:/RAW: at the DOS level (for H5):

- The kit mounts XCON: from `dist/XCON` (`Handler = L:vtcon-handler`, `GlobVec = -1`,
  `StackSize 16000`, `Priority 5`); `dist/Install` l.73-78 mounts it by path.
- How dos.library 39/40/47 creates the CON: and RAW: DosList entries (dn_SegList to a
  ROM segment, a resident-segment name, or dn_Handler) is **unverified**: not in DOSDOC,
  AM-5D/5E, or the NDK headers. The plan measures it (probe DP3) on each Kickstart before
  the swap code is written.
- V47 con-handler: medium mode `SetMode(fh, 2)` with CSI sequences for TAB, Shift+TAB,
  Up, Down that the V47 Shell uses for history and completion (RN-CH 47.1); exact bytes
  unknown (matrix Q9). ACTION_UNDISK_INFO 513 re-enables AUTO (RN-CH 47.1).
  XCON: today treats any non-zero SetMode as raw (`packet()`, ACTION_SCREEN_MODE,
  l.2123-2135) and does not answer ACTION_UNDISK_INFO (falls to `default`).

Kickstart images on this machine (for the version matrix; found with `find ~ -maxdepth 5`):
3.0 39.106 (`~/Desktop/Kickstarts/`, `~/Library/CloudStorage/Dropbox/WB/`,
`Up_Rough_Demo_System/web/maker/public/puae/kick39106.A1200`), 3.1 40.068 (the rig's,
`tools/rig/rig.py:27`), 3.5 "40.071" (`~/Desktop/Kickstarts/`), `~/Desktop/KICK_323.rom`
(name suggests 3.2.3: **identity unverified**). No 3.1.4 ROM found. Whether matching OS
3.2 system disks exist locally: **unverified**.

## 5. What vtcon already has that a device can reuse

Engine (`engine/vtengine.h`, portable C89, host-tested):

| Need | Function |
|------|----------|
| Unit terminal | `vt_new` (h:140), `vt_free` (141), `vt_set_personality(VT_AMIGA)` (142): the amiga personality answers all 50 ROM probe cases like the ROM CON: (ledger E4, matrix 7a) |
| CMD_WRITE | `vt_feed` + `vt_flush` (h:159-160, frame-paced) or `vt_write` (h:155) |
| Resize, size | `vt_resize` (h:161; **no reflow of wrapped lines**, vtengine.c:3121), `vt_cols`/`vt_rows` (163-164) |
| ConUnit cursor/modes/raw events | `vt_cursor` (182), `vt_modes` (183), `vt_raw_events` (186) |
| Replies into the read stream | `vt_callbacks.reply` (cb_reply) |
| Snip text | `vt_copy_text` (193), `vt_row_wrapped` (169) |
| Keys, mouse, paste | `vt_encode_key` (250), `vt_encode_mouse` (255), `vt_encode_paste` (257) |

Renderer (`render/amiga_render.h`): `vr_init` (94), `vr_free` (96), `vr_layout` (99),
`vr_redraw` (100), `vr_damage` (101), `vr_scroll` (102), `vr_cursor_off/on` (104-105),
`vr_cell_at` (107), `vr_set_view` (110, scrollback view), `vr_select`/`vr_selection`
(113-114), `vr_set_defaults` (78), `vr_blink_tick` (81), `vr_set_alt_font` (86),
`vr_palette_changed` (92). It draws on any `struct Window` (XCON: already uses it on
foreign `WINDOW 0x` windows: `open_window`, l.671-681).

XCON: handler (`handler/vtcon_handler.c`) -- the window-binding code a device unit needs,
today static functions inside the handler:

| Function | Line | Role |
|----------|------|------|
| `cb_damage`, `cb_scroll` | 276, 291 | engine -> renderer |
| `cb_reply` | 306 | reports into the input (ldisc in termios mode) |
| `cb_bell`, `cb_title` | 312, 318 | DisplayBeep, UTF-8 title -> Latin-1 |
| `report_defaults`, `cb_colors`, `cb_layout` | 345, 353, 364 | pens <-> engine defaults; CSI t/u/x/y layout |
| `open_window` (after `have_window:`) | 644 (attach part ~l.710-738) | vt_new from the window's inner size, personality, vr_init, alt fonts, defaults, cell pixels, first redraw |
| `close_window` | 741 | teardown incl. foreign-window IDCMP restore |
| `render`, `frame_start`, `output` | 803, 815, 826 | frame pacing (50 ms), DECCOLM resize, scrollback view reset |
| `copy_selection`, `paste` | 1565, 1596 | selection -> clipboard FTXT (Latin-1) via `clip_write`; clipboard -> input via `clip_read` (`handler/clip.h`) |
| `special_key`, `keypad_key` | 1499, 1528 | raw code -> VT_KEY_* |
| `console_key` | 1634 | RAmiga C/V, scrollback keys |
| `key_event` | 1661 | raw report (`CSI 1 {`), keymap via `RawKeyConvert` (l.1742), Meta = Left Amiga, break keys, ldisc |
| `raw_report` | 1793 | window-class raw event reports |
| `resize` | 1811 | vr_layout + vt_resize + size sync + SIGWINCH post |
| `mouse_event` | 1825 | mouse reports / drag select |
| `idcmp` | 1869 | IntuiMessage dispatch (XCON: owns its window's IDCMP; a device unit cannot) |
| `sync_size` | 1914 | writes cu_XMax/YMax of the ROM unit |
| `winch_handler`, `post_sizewindow` | 1932, 1943 | own input handler at pri 20 that injects IECLASS_SIZEWINDOW for ixemul (precedent for an input handler in vtcon code) |
| `rom_console` | 1976 | the DISK_INFO unit (see section 2) |
| `packet` / DISK_INFO | 2022 / 2236 | InfoData: `id_DiskType 'CON\0'` (value unverified), `id_VolumeNode` = window, `id_InUse` = rom_io |

Also reusable: `handler/clip.c` (`clip_write`/`clip_read`, IFF FTXT on unit 0,
PRIMARY_CLIP), `handler/lineedit.c` (cooked editing for H5), `tty/ldisc.c`,
`tests/amiga/romprobe.c` + `tools/probe_compare.py` + `tests/probes/amiga_cases.txt`
(ROM-vs-engine cursor probe, 50 cases), `tests/amiga/sizewatch.c` (lists input.device
handlers with priority and name), `tools/rig/install_rig.py` (Install/Uninstall checks).
`device/` exists and is empty.

Build facts: the handler is linked with no C startup (`handler_entry` first,
`vlink -bamigahunk -nostdlib`, Makefile l.235-260), vbcc `-cpu=68020`, NDK 3.2 headers,
`__reg("a0")` for register arguments (winch_handler). Per-process state rule: "No mutable
globals ... except the library bases" (vtcon_handler.c l.187-191) -- the same holds for a
device whose code is shared by every unit.

Memory: 500 lines of scrollback at 12-byte cells is ~480 KB per 80-column window
(ledger A500 note gives ~640 KB at 16-byte cells).

## 6. Not confirmed (and where I looked)

| Item | Looked in |
|------|-----------|
| ROM console input handler priority/name; whether Intuition forwards SIZEWINDOW/REFRESHWINDOW/RAWMOUSE into the chain for windows without those IDCMP flags | NDK-DOC, RKM-94..98, AM-30/65, ledger P2 (only IDCMP windows measured); AROS and ixemul give indirect evidence |
| Which programs open console.device directly (Ed, MultiView, ...) | NDK ReleaseNotes, ADCD manuals; no sources here |
| Private vector signatures (snip functions) and the V47 ConClip interface | NDK-FD (unnamed), RN-CC, AROS names only |
| CD_SETUPSCROLLBACK semantics; ROM handling of CMD_RESET/UPDATE/STOP/START/FLUSH | H-CON, NDK-DOC, RN-CD |
| Whether the ROM charmap console re-wraps linked lines on resize (RN-CH 47.2 implies it) | RN-CH, NDK-DOC |
| How dos.library creates CON:/RAW: DosList entries | DOSDOC, AM-5D/5E, NDK headers |
| AddDevice ordering (Enqueue by priority vs AddTail) | NDK-EXEC |
| V47 medium-mode bytes | RN-CH, matrix Q9 |
| KICK_323.rom identity, 3.2 disks, any 3.1.4 ROM | filesystem search listed in section 4 |

## 7. Measured on the rig (KS 40.63, console.device 40.2, dos 40.3), 2026-09-30

Logs: `build/rig/shots/chainprobe-ks40.63.log`, `dosnode-ks40.63.log`, `cdprobe-ks40.63.log`
(not committed; rerun with the rig scripts named). Other Kickstarts wait for DP6.

**DP1 input chain** (`tools/rig/chainprobe_rig.py`, 7/7):
- Chain at start: commodities 53, intuition 50, (probe 9, 5), **console.device 0**
  (is_Code 0x4000c92e in RAM, is_Data = the console base), (probe -5).
- **Nothing reaches priority -5**: the ROM console handler passes no event on, for any
  class, whichever window is active (CON: or not). A handler that must see events goes
  above 0.
- For a SIMPLE_REFRESH window with IDCMP 0, the chain above 0 carries SIZEWINDOW,
  REFRESHWINDOW (3 for one drag-over), ACTIVEWINDOW, INACTIVEWINDOW and CHANGEWINDOW with
  ie_EventAddress = that window; RAWKEY (8) and RAWMOUSE arrive while it is active.
- CLOSEWINDOW: 2 events, neither addressed to either window (ie_EventAddress is not the
  window); the close click activates the window first, so "the active window" names it.
- No IECLASS_TIMER event reached priority 9 in 50 s (Intuition, at 50, consumes them).

**DP3 DosList** (`tools/rig/dosnode_rig.py`, 2/2): CON: and RAW: are DLT_DEVICE, dn_Type 0,
no handler name, stack 3200, pri 5, startup 0 (CON) / 1 (RAW), GlobVec -1, both sharing
seglist 0x40010470 in fast memory -- not a ROM address and not a dos resident: a
con-handler loaded from disk (this rig's L: carries one). KingCON is mounted as KCON/KRAW.

**DP4 ROM command census** (`tools/rig/cdprobe_rig.py`, 9/9; same on units 0, 1 and 3):

| Command | io_Error |
|---|---|
| CMD_INVALID | -3 IOERR_NOCMD |
| CMD_RESET | **never returns: SendIO blocks the caller for good** (all three units; `--reset unit=n`) |
| CMD_READ, no input | stays queued; AbortIO gives -2 IOERR_ABORTED |
| CMD_WRITE, CMD_CLEAR | 0 |
| CMD_UPDATE | -3 |
| CMD_STOP, CMD_START, CMD_FLUSH | -1 with nothing queued |
| CMD_FLUSH with a read pending | 0; the read replies -2 |
| CD_ASKKEYMAP / SETKEYMAP / ASKDEFAULTKEYMAP / SETDEFAULTKEYMAP | 0, io_Actual 32 |
| CD_SETUPSCROLLBACK, CD_SETSCROLLBACKPOSITION | -3 (no scrollback on 40.2) |
| NSCMD_DEVICEQUERY | -3 (not a new-style device) |

- CONFLAG_NODRAW_ON_NEWSIZE: a CHARMAP unit with flags 0 redraws after shrink+grow
  (ink 3120 -> 3120); with the flag the inner area stays empty (3120 -> 0).
- **The SNIPMAP unit re-wraps** a wrapped line on widening: 100 characters in 45 columns,
  cursor 3;11; widened to 90 columns, cursor 2;11 (screenshot: 90 + 10). The console uses
  its own 8-pixel font, not the 6-pixel screen font, so the probe sizes by cu_XMax.
- Probe fixes found on the way: ReadPixel per pixel hung the probe for minutes on the
  rig's RTG screen (now ReadPixelLine8 per row); the re-wrap verdict now uses the unit's
  measured widths.
