---
date: 2026-10-03
topic: What to port from DCTelnet (retro32-term, PETSCII, aspect fonts, own screen) into UP-Term
tags: [vtcon, dctelnet, retro32-term, petscii, screen, fonts]
status: final
---

# Porting from DCTelnet

Owner 2026-10-03: "can we add support for pubscreens so the term can run in fullscreen mode? this
project has a retro32term that is supposed to be fast on native screens as well, and support for
petscii and fonts that adapt for square pixels and non square pixels, can we port it over to our
term? what else should be ported over from DC Telnet?" Trees: ~/Code/dctelnet-v2 (worktree of
dctelnet-petscii-recovered, branch feature/pr-macro-help, newest integration), dctelnet-ibmfont
(feature/ibm16-square-pixels, 3971d95, local only), dctelnet-vtcon (feature/vtcon-engine, b71b997,
local only: vtcon's engine inside DCTelnet), dctelnet-petscii-recovered (feature/pr-ssh, 1.9.1
line), DCTelnet-PETSCII (a 1.9.1 release package, no source).

## UP-Term today (file:line, 2026-10-03)

- Screens: XCON: opens on a public screen (`SCREEN name`, LockPubScreen with the default as
  fallback, handler/vtcon_handler.c ~1708-1737); BACKDROP (borderless backdrop window) and
  NOBORDER/NODRAG/NOSIZE exist. No OpenScreen, no screen-mode requester, no own public screen.
- Planar fast path: render/amiga_render.c (extract_glyphs ~837, direct_ok ~871, direct_cell ~899),
  built only with DIRECT=1; measured slower on the 68020 / AGA hires 4-plane rig (6.6 s against
  3.4 s through Text(): the CPU's chip writes wait for display DMA while Text()'s blits run beside).
- No pixel-aspect logic, no PETSCII.

## retro32-term (src/third_party/retro32-term, all copies at 2aec2db, the Unlicense)

Third-party (Andrew Hutchings, Bruno Frederic; four owner commits on spotUP/feat/layer-aware).
term-engine.c, 1057 lines, `#include`d by its host, all state file-static (one instance), 8-pixel
cells high, 8 or 16 wide, 80 columns at most, PC-ANSI parser only. Drawing: CPU byte writes per
plane (32 a cell), BltBitMap fills and scrolls with a plane mask, a lazy WaitBlit, an XOR cursor,
BytesPerRow stride (interleaved screens), exactly 4 planes and 8-aligned x for the direct path,
else the window RastPort (Text, RectFill, ScrollRaster -- the RTG path). Tuned for the 68000; no
speed numbers recorded. **The aspect fonts and PETSCII are not in it**: they are DCTelnet's
(src/screenfont.c pairs topaz 8 <-> TopazPro 16, IBM 8 <-> IBM 16 by DisplayInfo resolutionX ==
resolutionY, 8 again when 25 rows of 16 do not fit; petscii_dispatch.c, petscii_keymap.c,
petscii_screencode.c, Petscii/PetsciiLower fonts, 16x8 cells for 40 columns).

So "port retro32-term" means: its drawing technique is UP-Term's DIRECT path already (UP-Term's
is the generalised one: any depth up to 8, any 8-pixel-wide font height, many instances); the
missing parts are its lazy WaitBlit / interleaved stride details, and a measurement on the screen
it was made for (an own 4-plane native screen; a 68000).

## DCTelnet's features against UP-Term (ranked for a general terminal)

1. Own screen / public screen with a screen-mode requester (DCTelnet.c ~5406 OpenAppScreen:
   SA_DisplayID, SA_Depth, SA_AutoScroll, SA_Interleaved, SA_SharePens, SA_FullPalette).
2. Fonts by pixel aspect (screenfont.c) and a bundled CP437 font set (IBM 8/11/16 -- 16 is the
   Moebius VGA 8x16, Apache-2.0; TopazPro).
3. PETSCII personality (40 columns, C64 colours, reverse, F-keys, shift-case charsets).
4. Capture / log to file, Save screen as .ans (DCTelnet.c ~3164-3230).
5. ZMODEM autostart (DCTelnet-protocol.h ~1146 ZmodemDetect) -- UP-Term has sz/rz only.
6. An ARexx command port (rexxcmd.c, waitfor.c).
7. ANSI music and an audio bell (ansimusic.c, sound.c).
8. Function-key macros per profile; a SyncTERM key set; BS/DEL swap.
9. A telnet / rlogin command (DCTelnet-protocol.h's IAC/NAWS/TTYPE code) for PTY:.

BBS-client only (a dialer front-end, not the terminal): address book and per-BBS overrides,
SyncTERM phonebook import, login macros, redial / keep-alive, the packet (chat) window, LEDs and
toolbar, Finger, XEM / XPR libraries, DCTelnet's own SSH (BebboSSH covers it). Nothing anywhere
implements SAUCE, Avatar, RIP or ATASCII.
