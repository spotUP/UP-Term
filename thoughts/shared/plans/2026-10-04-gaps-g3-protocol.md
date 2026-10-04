---
date: 2026-10-04
topic: G3 -- protocol, OSC and terminfo gaps of the xterm personality
tags: [plan, ledger, xterm, osc, terminfo, kitty-keyboard, gaps]
status: draft
---

# G3: protocol, OSC, terminfo -- progress ledger

Source: `thoughts/shared/research/2026-10-04_modern-terminal-gaps.md` (gap list items
4, 9, 12, 15, 17, 18, 19, 21 and the "Found" RIS / ?2026 notes). Branch
`feature/gaps-g3-protocol`. Each item: host regression test that fails before the fix,
matrix section 9 updated in the same commit.

Constraints (decided, do not re-litigate):
- Never touch csi_fast's fast cases, put_ascii_run, scroll, fills, draw_rows,
  vr_asm_cell, flush (speed campaign). New CSI/OSC/DCS go to the general dispatch.
- sizeof(vt_cell) stays 16; `pad` stays free (G2 / U1 wants it for 21-bit ch).
  Hyperlinks ride on the `ext` rare-style index (style table gets a link id).
- Key encoder changes additive (G1 edits the same function).
- Focus events: G1 delivers them, so kxIN/kxOUT stay in terminfo.
- No FS-UAE rig (benchmark running).

Checklist (ID, item, commit):

- [ ] G3-01 #17 DECRQSS "p answers what DA1 says (62); DECRQM ?66 / ?1048 and full audit
- [ ] G3-02 RIS resets modifyOtherKeys, title stack, cursor style (to the host default), kitty flags
- [ ] G3-03 ?2026 hold timeout to the other terminals' value (testable constant)
- [ ] G3-04 #4 remote TERM: ssh/telnet/rlogin get xterm-256color unless configured (vshrc + `command`)
- [ ] G3-05 #21 DA3, LS2/LS3/LS1R/LS2R/LS3R (REP, SS2/SS3, title stack already present: verified)
- [ ] G3-06 #21 DECIC / DECDC
- [ ] G3-07 #21 DECFRA / DECERA / DECCRA (+ DECSERA)
- [ ] G3-08 #21 ?1015 urxvt and ?1016 SGR-pixel mouse encodings
- [ ] G3-09 #21 ?2048 in-band resize reports
- [ ] G3-10 #21 DECSLRM / DECLRMM: decision recorded
- [ ] G3-11 #18 kitty keyboard protocol: CSI >u <u ?u =u, flags 1/2/4/8/16, encoder
- [ ] G3-12 #9 OSC 52: streaming OSC path, base64 decode, SET to the clipboard, QUERY opt-in
- [ ] G3-13 #19 OSC 7 cwd stored; new tabs start there when local; vsh reports its cwd
- [ ] G3-14 #19 OSC 8 hyperlinks (side table via ext), open with a profile command
- [ ] G3-15 #19 OSC 133 prompt marks, previous/next prompt in the scrollback
- [ ] G3-16 #19 OSC 9 / 777 notifications (title flash)
- [ ] G3-17 #19 OSC 4/104, 10-12/110-112 set and query: audit complete
- [ ] G3-18 #12 terminfo: Sync, rep, Cs/Cr, kUP3.., Ms, Tc, Smulx/Setulc kept; termcap in step; tic -c clean
- [ ] G3-19 #15 XTGETTCAP from one table checked against the terminfo source by a host test
- [ ] G3-20 make amiga zero warnings; full make test (landing)

Running count: 0 of 20.

## Decisions

## Notes / gotchas found
