---
date: 2026-10-03
topic: Own screen / full screen, aspect fonts, the native-screen fast path, PETSCII, and the rest from DCTelnet
tags: [vtcon, screen, fonts, petscii, dctelnet]
status: draft
---

# Screens and the DCTelnet port

Owner 2026-10-03 (research/2026-10-03_dctelnet-port.md has the findings): "can we add support for
pubscreens so the term can run in fullscreen mode? ... retro32term that is supposed to be fast on
native screens ... petscii and fonts that adapt for square pixels and non square pixels ... what
else should be ported over from DC Telnet?"

## Decisions (lead)

- **Own screen** is a window option and a profile key, not a separate program: OWNSCREEN,
  FULLSCREEN (a borderless backdrop window over the whole screen), PUBSCREEN name, SCREENMODE,
  DEPTH; profile `screen = workbench | own | fullscreen`, `screen-mode`, `screen-depth`. The
  screen is public (UP-Term, UP-Term.2 ...: other programs open on it), 16 pens = the terminal's
  ANSI colours (8 planes on a graphics card), Intuition's pens among them, interleaved, and goes
  with its last window (a visitor keeps it until it closes).
- **retro32-term is not ported as code**: its technique (CPU byte writes per plane, blitter
  fills and scrolls) is UP-Term's DIRECT path already, generalised (any depth up to 8, many
  instances). What is taken from it: the details the DIRECT path lacks (lazy WaitBlit, the
  BytesPerRow stride for interleaved screens) and the screen it was made for -- the own 4-plane
  native screen -- where it is measured again and switched on when it wins.
- **Fonts by pixel aspect**: DCTelnet's rule (screenfont.c): a table of font pairs, the 16-pixel
  one on square pixels (DisplayInfo resolution x == y), the 8-pixel one otherwise; back to 8 when
  25 rows do not fit.
- **PETSCII** is a fourth personality of the engine (host-testable), with C64 fonts.

## Checklist

- [x] P1.1 own screen: OWNSCREEN / FULLSCREEN / PUBSCREEN / SCREENMODE / DEPTH and the profile keys
      (prefs_core keeps them on a save); the title on the screen in full screen; the screen closes
      with its last window, a visitor's window keeps it until it goes. tools/rig/ownscreen_rig.py
      7/7 (3.1, RTG Workbench).
- [ ] P1.2 a native screen on the rig (SCREENMODE PAL hires, 4 planes): colours exact, menus; the
      3.2 rig too.
- [ ] P1.3 Settings > Screen (Workbench / Own screen / Full screen) live: the window moves to the
      other screen with its text (vtwin reattach keeping the engine); /screen; a ScreenMode
      requester (ASL, in the worker) for screen-mode; Prefs fields.
- [ ] P2 fonts by pixel aspect: the pair table (topaz 8 <-> TopazPro 16, IBM 8 <-> IBM 16), the
      bundled fonts (licences checked: IBM 16 = Moebius VGA 8x16, Apache-2.0), chosen at open and
      on a screen change.
- [ ] P3 the DIRECT path on the own native screen: lazy WaitBlit, stride, measured cycle-exact
      against Text() (68020; a 68000 row when there is an A500 rig); on by default where it wins.
- [ ] P4 PETSCII personality: dispatch, screencodes, C64 colours, reverse, keys, 40 columns
      (16x8 cells), the Petscii fonts; host tests from DCTelnet's.
- [ ] P5 capture / log to file, Save screen as .ans.
- [ ] P6 ZMODEM autostart in the window (the stream sniffer) using zm/.
- [ ] P7 ARexx command port (SEND, WAITFOR, CAPTURE, GETSTATUS).
- [ ] P8 ANSI music and an audio.device bell (PC-ANSI personality).
- [ ] P9 function-key macros per profile; a SyncTERM key set; BS/DEL swap.
- [ ] P10 a telnet / rlogin command over PTY: (DCTelnet's IAC / NAWS / TTYPE code).
