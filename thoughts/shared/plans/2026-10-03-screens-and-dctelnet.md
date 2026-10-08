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

- **Decision 2026-10-08 (owner):** DCTelnet is the BBS client; UP-Term gets no second copy of its
  BBS features (PETSCII, capture, ARexx port, ANSI music, macros, rlogin, ZMODEM autostart).
  DCTelnet runs on UP-Term's engine (dctelnet-vtcon feature/vtcon-engine), so UP-Term reuses it
  by running DCTelnet in a window, not by porting code. A first rlogin copy in net/tn.c was
  written and discarded the same day for this reason.

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
- [x] P4 covered by DCTelnet (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) e52dd69 C64 font, f94d59d/2448df3 C64 colour keys, 5894016 charset switch; src/petscii_*.c with tests): PETSCII personality: dispatch, screencodes, C64 colours, reverse, keys, 40 columns
      (16x8 cells), the Petscii fonts; host tests from DCTelnet's.
- [x] P5 covered by DCTelnet (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) a71eb3b "Capture to file, save screen as ANSI, find in the scroll back"): capture / log to file, Save screen as .ans.
- [x] P6 covered by DCTelnet (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) 5f28fe1 "Telnet protocol handling and ZModem transfer detection", d58c769, ec7ebb1; src/DCTelnet-protocol.h): ZMODEM autostart in the window (the stream sniffer) using zm/.
- [x] P7 covered by DCTelnet (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) 1a2e1ef "An ARexx port", src/rexxcmd.c, waitfor.c): ARexx command port (SEND, WAITFOR, CAPTURE, GETSTATUS).
- [x] P8 covered by DCTelnet (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) e4ada33 "Bell as a sound or off; ANSI music", src/ansimusic.c, sound.c): ANSI music and an audio.device bell (PC-ANSI personality).
- [x] P9 covered by DCTelnet for macros, function keys and BS/DEL swap (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) deced37 keys, fkey.gui / "Function Keys window" 6196853, APP_BACKSPACE_DEL_SWAPPED in src/prefs.h; UP-Term has its own backspace = del|bs pref, prefs/prefs_core.c). A SyncTERM KEY SET was not found in DCTelnet (only the phone book import, d2b206f): a DCTelnet item, not UP-Term. function-key macros per profile; a SyncTERM key set; BS/DEL swap.
- [x] P10 telnet stays in UP-Term (net/tn.c, ffa32a4); rlogin covered by DCTelnet (dctelnet-vtcon feature/vtcon-engine (also dctelnet-v2) 0064c01 "Rlogin connections", src/rlogin.c, tests/test_rlogin.c); rig run of uptelnet stays OWNER step A1.4. (was HALF DONE: telnet = net/tn.c, net/uptelnet.c, tests/test_telnet.c 28 checks, ffa32a4, not over PTY: but on bsdsocket; rlogin: no code in net/, OPEN; rig run of uptelnet is OWNER step A1.4) a telnet / rlogin command over PTY: (DCTelnet's IAC / NAWS / TTYPE code).

## Open items triage 2026-10-08

8 lines were open (P3-P10). After the owner decision above, 1 stays open.

| Item | Needs | Verdict |
|------|-------|---------|
| P3 native-screen fast path, measured cycle-exact against Text() | rig + owner on hardware | OPEN, UP-Term only. The planar path exists (7ecdb88) with RT1/CC1 hardware proofs, but no cycle-exact comparison and no lazy-WaitBlit/stride record. Needs a bench on the rig (68020; 68000 row needs an A500) and the owner's hardware numbers. Tick needs a measurement. |
| P4 PETSCII | none | covered by DCTelnet e52dd69, f94d59d, 5894016 |
| P5 capture / save .ans | none | covered by DCTelnet a71eb3b |
| P6 ZMODEM autostart | none | covered by DCTelnet 5f28fe1 (detection in src/DCTelnet-protocol.h); UP-Term keeps sz/rz in zm/ for shell use |
| P7 ARexx port | none | covered by DCTelnet 1a2e1ef |
| P8 ANSI music, bell | none | covered by DCTelnet e4ada33 (UP-Term's bell: beep/flash, render/vtwin.c cb_bell) |
| P9 macros, keys, BS/DEL | none | covered by DCTelnet deced37, 6196853; SyncTERM key set is absent there: a DCTelnet item |
| P10 telnet / rlogin | owner step A1.4 | telnet in UP-Term done (ffa32a4); rlogin covered by DCTelnet 0064c01 |

Evidence: commit subjects and files read in ~/Code/dctelnet-vtcon (feature/vtcon-engine); the
hashes exist in both DCTelnet checkouts. Not run: DCTelnet's `make ci` (Docker/VBCC) and its
behaviour on UP-Term's engine are unverified here.
