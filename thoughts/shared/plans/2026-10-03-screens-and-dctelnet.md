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
- [x] P1.2 a native screen (SCREENMODE 0x29000 PAL hires): 4 planes chosen by the mode's own
      kind (DIPF_IS_FOREIGN; a Workbench on a card had made it 8), the 16 ANSI pens allocated
      shared at open (ObtainBestPen took free pens and overwrote 9 colours before).
      ownscreen_rig.py 8/8. The 3.2 rig: still to run.
- [~] P1.3 Settings > Screen (Workbench / Own screen / Full screen) and /screen: the window moves
      live with its text (vtwin_unbind / vtwin_rebind keep the engine; open_window split into
      lock_screen + create_window, close_window's window parts into window_parts_close; the move
      waits until the IDCMP loop has replied its messages). Refused with 2+ tabs. ownscreen_rig.py
      11/11. OPEN: a ScreenMode requester (ASL, in the worker) for screen-mode; Prefs fields for
      screen / screen-mode / screen-depth (the keys are kept on a save already).
- [x] P2 fonts by pixel aspect: the pair table (topaz 8 <-> TopazPro 16, IBM 8 <-> IBM 16), the
      bundled fonts (licences checked: IBM 16 = Moebius VGA 8x16, Apache-2.0), chosen at open and
      on a screen change. DONE 2026-10-03: render/fontpair (host 28 checks), vtwin_set_screen /
      vtwin_fit_aspect, profile key font-aspect = off, aspect_rig 4/4. Only topaz 8 pairs (topaz
      9 / 11 are other designs, a chosen size stays); /font-size steps through the face, the
      pair's square font one of its sizes (menus_rig 13/13). Kit: Files/fonts; install.dos
      copies only the sizes FONTS: lacks, a marker each, FixFonts; Uninstall removes the
      marked ones (rig: IBM 16 added and removed, the user's IBM 8 / 11 kept).
- [x] P1.4 Intuition's pens in the user's Workbench colours (owner 2026-10-03: "they should use
      the colors from the users wb settings"): wb_pens copies the Workbench's DrawInfo pens'
      RGB to pens past the 16 ANSI ones (5 planes on AGA by default, 8 on a card; ECS 4 planes:
      the nearest ANSI pen). The cursor is drawn in colours, not COMPLEMENT (pen numbers past
      16 inverted to anything: a cyan cursor in a visitor window). ownscreen_rig 13/13, the
      cursor check failing on the old code.
- [ ] P3 (OPEN 2026-10-06: the planar fast path exists, 7ecdb88, and RT1/CC1 hardware proofs, but no cycle-exact comparison against Text() and no lazy-WaitBlit/stride record found) the DIRECT path on the own native screen: lazy WaitBlit, stride, measured cycle-exact
      against Text() (68020; a 68000 row when there is an A500 rig); on by default where it wins.
- [ ] P4 PETSCII personality: dispatch, screencodes, C64 colours, reverse, keys, 40 columns
      (16x8 cells), the Petscii fonts; host tests from DCTelnet's.
- [ ] P5 capture / log to file, Save screen as .ans.
- [ ] P6 ZMODEM autostart in the window (the stream sniffer) using zm/.
- [ ] P7 ARexx command port (SEND, WAITFOR, CAPTURE, GETSTATUS).
- [ ] P8 ANSI music and an audio.device bell (PC-ANSI personality).
- [ ] P9 function-key macros per profile; a SyncTERM key set; BS/DEL swap.
- [ ] P10 (HALF DONE: telnet = net/tn.c, net/uptelnet.c, tests/test_telnet.c 28 checks, ffa32a4, not over PTY: but on bsdsocket; rlogin: no code in net/, OPEN; rig run of uptelnet is OWNER step A1.4) a telnet / rlogin command over PTY: (DCTelnet's IAC / NAWS / TTYPE code).
