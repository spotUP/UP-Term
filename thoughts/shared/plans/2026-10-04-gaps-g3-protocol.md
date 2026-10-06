---
date: 2026-10-04
topic: G3 -- protocol, OSC and terminfo gaps of the xterm personality
tags: [plan, ledger, xterm, osc, terminfo, kitty-keyboard, gaps]
status: implemented
---

# G3: protocol, OSC, terminfo -- progress ledger

Source: `thoughts/shared/research/2026-10-04_modern-terminal-gaps.md` (gap list items
4, 9, 12, 15, 17, 18, 19, 21 and the "Found" RIS / ?2026 notes). Branch
`feature/gaps-g3-protocol`. Each item: host regression test that fails before the fix,
matrix section 9 updated in the same commit. Tests: `tests/test_protocol.c`
(`make test ONLY=protocol`), vshrc in `tests/test_sh_exec.c`.

Constraints (decided, do not re-litigate):
- Never touch csi_fast's fast cases, put_ascii_run, scroll, fills, draw_rows,
  vr_asm_cell, flush (speed campaign). New CSI/OSC/DCS go to the general dispatch.
- sizeof(vt_cell) stays 16; `pad` stays free (G2 / U1 wants it for 21-bit ch).
  Hyperlinks ride on the `ext` rare-style index (style table gets a link id).
- Key encoder changes additive (G1 edits the same function).
- Focus events: G1 delivers them, so kxIN/kxOUT stay in terminfo.
- No FS-UAE rig (benchmark running).

Checklist (ID, item, commit):

- [x] G3-01 #17 DECRQSS "p answers what DA1 says (62); DECRQM ?66 / ?1048 and full audit -- 3997a3b
- [x] G3-02 RIS resets modifyOtherKeys, title stack, cursor style (to the host default) -- 5f4949f (kitty flags: with G3-11)
- [x] G3-03 ?2026 hold timeout -- 6d96a87 (1 s: foot 1 s, tmux 1 s, kitty 2 s, read from their sources)
- [x] G3-04 #4 remote TERM: ssh/telnet/rlogin get xterm-256color unless configured (vshrc + `command`) -- d24e3b0
- [x] G3-05 #21 DA3, LS2/LS3/LS1R/LS2R/LS3R, media copy (xterm-256color's mc0/mc4/mc5) -- 09fb797 (REP, SS2/SS3, title stack were present)
- [x] G3-06 #21 DECIC / DECDC -- ade3fed
- [x] G3-07 #21 DECFRA / DECERA / DECSERA / DECCRA + DECCARA / DECRARA / DECSACE -- ade3fed
- [x] G3-08 #21 ?1015 urxvt and ?1016 SGR-pixel mouse encodings -- df96a65 (host pixel wiring: see notes)
- [x] G3-09 #21 ?2048 in-band resize reports -- e2c46cc
- [x] G3-10 #21 DECSLRM / DECLRMM: not implemented, decision below
- [x] G3-11 #18 kitty keyboard protocol: CSI >u <u ?u =u, flags 1/2/4/8/16, encoder, host presses/repeats/releases -- 6fa7ccb (RIS clears the stacks there)
- [x] G3-12 #9 OSC 52: streaming OSC path, base64 decode, SET to the clipboard, QUERY opt-in (program-clipboard) -- 7466869
- [x] G3-13 #19 OSC 7 cwd stored; new tabs start there when local -- fc16295; vsh reports its cwd -- 6318b95
- [x] G3-14 #19 OSC 8 hyperlinks (via the ext style table), Ctrl + click opens with link-open -- fc16295
- [x] G3-15 #19 OSC 133 prompt marks, Right Amiga + Shift + Up/Down -- fc16295; vsh marks -- 6318b95
- [x] G3-16 #19 OSC 9 / 777 notifications (title bar 5 s) -- fc16295
- [x] G3-17 #19 OSC 4/104, 10-12/110-112: set/query were complete; X11 names + colourN added -- 42576bc
- [x] G3-18 #12 terminfo: Sync, rep, Cs/Cr, Ms, Tc, fe/fd, smm/rmm, kUP3..kNXT7, kf25-63; termcap regenerated; tic -cx clean; make test-terminfo green -- 1798082
- [x] G3-19 #15 XTGETTCAP from engine/vtcaps.inc generated from the terminfo; host test parses the entry -- 1798082
- [x] G3-20 make amiga (68020 and CPU=68000) zero warnings; full make test green (21 suites); make test-ref 148 streams green

Running count: 20 of 20 (G3-10 by decision: not implemented).

## Decisions

- G3-10 DECSLRM / DECLRMM not implemented: left/right margins change the wrap column
  of put_ascii_run (and its asm), the horizontal extent of scroll_up/scroll_down, IL/DL,
  ICH/DCH and every fill -- all hot paths the speed campaign owns this week. DA1 keeps
  `62` (VT220), DECRQSS "p answers 62, so no program is told it has margins (tmux then
  redraws a vertical split instead of scrolling it: slower, correct). DECIC/DECDC and
  the rectangles work over the full width.
- Mode bits: G1 took 0x200000 (?1007); G3 uses 0x1000000 (?1015), 0x2000000 (?1016),
  0x4000000 (?2048), leaving 0x400000/0x800000 free between.
- `command` builtin in vsh (G3-04): needed so telnet()/rlogin() in the vshrc reach the
  real programs.
- Fail-before proof for engine items: scratchpad `failproof.sh REV SUITE` builds the
  current tests against REV's engine.

## Notes / gotchas found

- xterm-256color (ncurses 6.4) says kbs=^H; the engine sends DEL (vtcon's own entry says
  ^?). Debian/Ubuntu/Fedora patch kbs=^? and readline takes both; upstream-ncurses hosts
  (Arch, macOS) and vim's t_kb may read DEL as <Del>. Not changed (owner decision).
- meml/memu (ESC l / ESC m, HP memory lock) in xterm-256color: not implemented; no
  program found using them.
- ?1016 pixels: vtwin_mouse passes cells, so the report is the cell's corner pixel until
  the host calls vt_encode_mouse_px (G1 owns vtwin_mouse this week).
- kitty flag 4: the host gives the shifted key, not the base-layout key (base 0).
- OSC 133 marks: a reflow drops them (reflow is off in UP-Term windows; G4 rewrites it).
- OSC 52 selections c/p/s/0-7 all go to clipboard unit 0; q (secondary) alone is ignored.
  A query answers at most 64 KB.
- vt_encode_key's old body is legacy_key (unchanged); G1's encoder edits land there.
- Merge with G1: render/vtwin.c (kitty_key call in vtwin_key, link_click in vtwin_mouse,
  console_key prompt jump and Shift+PgUp on the alternate screen) and the vt_callbacks
  memset in vtwin_attach overlap G1's rewrite of vtwin_mouse/vtwin_key.
- A program that copies engine/vtengine.c alone (DCTelnet) now needs engine/vtcaps.inc
  beside it.
