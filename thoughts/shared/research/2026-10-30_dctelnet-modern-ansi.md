---
date: 2026-10-30
topic: Can DCTelnet gain UP-Term's modern ANSI terminal via the console.device replacement, or another path?
status: research complete, recommendation given, nothing implemented
---

# DCTelnet and modern ANSI: device route vs embedded engine

Trees: ~/Code/DCTelnet/dctelnet-v2 (renderer cycle), ~/Code/DCTelnet/dctelnet-vtcon (branch
feature/pr-vtcon-engine, HEAD b782687, 227 commits, vtcon submodule pinned 232065f).

## 1. How DCTelnet's console.device renderer works (dctelnet-vtcon/src/DCTelnet.c)

- Open: OpenConsoleDevice (5062-5150). devName "console.device" or "ibmcon.device" (5070); unit
  CONU_SNIPMAP for console.device (5094-5096), CONU_CHARMAP / 2 for ibmcon (5098-5102); flags
  CONFLAG_DEFAULT; io_Data = the DCTelnet window, io_Length = sizeof(Window) (5112-5113); opened
  through OpenNewestDevice (shipped.c:114, prefers a newer copy in DCTelnet's Devs drawer).
- Output: raw passthrough, no pre-parse into commands. ConWrite (417-466): CMD_WRITE via DoIO,
  one write per <=1024 bytes. Only transform is Ansi_Swap17 (SGR colour 1/7 swap, ansiscan.h:32)
  because ROM console.device draws plain text in pen 1; the swap keeps a split-sequence tail
  (consoleSwap). ibmcon gets the data untouched. Init writes "\e[20l" (821) and "\e[0 p" (2430).
- Read back: NO CMD_READ on the console (the only CMD_READ is clipboard.device, 3750). Keys come
  from IDCMP_RAWKEY on DCTelnet's own window (4040, keys.c). Window size from the window and
  font (LimitTerminalWidth 1589, TerminalGrid), not from ConUnit. ConUnit is used once:
  cu_Font cast at 2272 to switch the C64 charset. Other probes are ibmcon-only private commands
  (IBMCMD_READTEXT 0x7FE3 at 593/3204/3782, IBMCMD_SETPENS 5036).
- So the path needs: CMD_WRITE of ANSI on a window-bound unit, io_Unit->cu_Font, and the
  device painting into DCTelnet's window/screen with the window's RastPort font.

## 2. up-console.device against that path

- Has: CMD_READ (upcon_device.c:487), CD_ASKKEYMAP/SETKEYMAP (529), real 296-byte ConUnit
  filled after every write (upcon_unit.c:75-139, DD17), SNIPMAP/CHARMAP units, resize, RGB
  field plumbing (fg_rgb, upcon_unit.c:280). Engine does SGR 38;2 (vtengine.h:27-30).
- Lacks for this goal:
  - The unit is hard-wired to the AMIGA personality: u->w.pers = VT_AMIGA (upcon_unit.c:279),
    Latin-1, ROM-dialect colours. No UTF-8 (the xterm personality is not reachable from a unit).
    No way to select PCANSI/XTERM from a program: no opcode, no flag.
  - No scrollback in the device (upcon_unit.c:282; CD_SETUPSCROLLBACK answers -3,
    research 2026-09-30 line 325), by design to match the ROM.
  - It is not installed by default: the user must run `UPConsole DEVICE ON` (InitResident,
    DD15); DCTelnet cannot ship it as a library it opens itself without a loader path.
  - DCTelnet's BBS content is CP437 ANSI.SYS art (blink, bright bg, cursor-save): needs the PCANSI
    personality, which the ROM-dialect device is not.
  - Glyph/ pen model: device draws with window font and screen pens like ROM; DCTelnet's aspect
    fonts, PETSCII fonts, own-pen palette, cell-aligned scroll are drawn by DCTelnet itself.
- Device-route work needed: personality switch (new private CMD or open flag), PCANSI mapping,
  CP437 glyph path, DCTelnet-side removal of Ansi_Swap17 for the new device, install story.

## 3. State of dctelnet-vtcon

Done (git log): "vtcon engine: full ANSI (phase A)" 7790728; "Built-in renderer: vtcon's engine
parses, retro32-term draws" fdcddd4; 24-bit cell colours b3fcdfe; "24-bit on palette screens:
vt_rgb_to_256" 4ffdf28; submodule from GitHub b58f1b9; LayerInfo lock while drawing b782687.
src/vtbuiltin.c (185 lines): engine in VT_PCANSI personality (line 160) feeds damage/scroll/reply
callbacks into retro32-term's glyph writer; engine owns the grid, replies (DSR/DA) go to the BBS.
console.device and ibmcon renderers are untouched alongside it (ConWrite branches by renderer).
No thoughts/ notes in that tree (docs/ has only guide, readme, debug). The only notes are in
UP-Term: thoughts/shared/research/2026-10-03_dctelnet-port.md (names the tree, lines 13-17) and
todos.md line 89 (licence restore, repos.lock re-pin) and line 95 (upterm-bootstrap skips
worktree .git files).
Open (from the code, unverified on a rig by me): UTF-8 (pcansi is CP437; xterm personality not
selected anywhere: grep of src/*.c finds only line 160); no per-connection personality choice
(Prefs/charset); true-colour on non-palette (RTG/AGA 24-bit) screens beyond vt_rgb_to_256 not
looked at; scrollback uses DCTelnet's own scrollback window, not the engine's; sixel/mouse not
wired. A rig test of the built-in path is not in this research.

## 4. Recommendation

Embedded engine route (already started), not the device route.
Decisive reason: DCTelnet drives the renderer by raw CMD_WRITE and owns key input, window
layout, fonts, PETSCII and palette itself, while up-console.device is a ROM-dialect clone
(AMIGA personality, Latin-1, no scrollback, needs a separate resident install). Making it modern
means adding a personality switch to the device and then still losing DCTelnet's cell-exact
drawing; the engine already sits behind that drawing and proves it in 185 lines.

Work items (embedded route):
1. Personality per connection: PCANSI for BBS (default), XTERM for Unix/SSH sites; prefs and
   site_prefs field, vt_set_personality at connect (vtbuiltin.c:160 now fixed).
2. UTF-8: with XTERM personality, map code points through glyphmap to the screen font; fallback
   glyph box; test with a UTF-8 BBS/ssh session.
3. Rig-test 24-bit on a true-colour screen (not only vt_rgb_to_256) and on a palette screen.
4. Use engine scrollback or keep DCTelnet's window; decide and test with art longer than a screen.
5. Mouse reporting, OSC title, bracketed paste: route replies via the existing reply callback.
6. Keep console.device/ibmcon renderers as the compat fallback; optionally list up-console.device
   as a device-route when installed (cost: personality switch in the device first).
7. Pin the submodule, restore the licence (todos.md:89), re-pin repos.lock.

Scratch files: none created.
