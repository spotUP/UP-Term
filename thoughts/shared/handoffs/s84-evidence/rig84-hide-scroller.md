# rig 84 hide + scroller check (2026-11-15)
Deploy: UP-Term:bin Patina/patina-* sizes equal build/amiga (Patina 412644, gui 582424); dripens put.
Listers: `lister query all` returns nothing on this DOpus (rc 20); window list used. Before: Device List x2 (+Patina bar). After vantablack pick: Device List x2 with NEW addrs (reopened), theme=vantablack.
Probe p6.log: select 1 vantablack: Keep seen, pen map rgb changed (1A1B26 414868 A9B1D6 .. -> 000000 7A7A7A BEBEBE 8D8D8D), pens indices same, PATINA_DEFERRED=1 in env. Harness FAIL lines (reopen theme list / menu window) are harness timeouts, select 2 (starship) not applied.
y rows at x=400/500 before: 0-12 1A1B26(pal0, bar) 13 A9B1D6(pal2 text) 14+ 0E0E14. After: 0-12 000000, 13 BEBEBE(pal2 = text colour), 14+ 0E0E14. Light line at y13 STILL present (same as before).
Counters before: frame calls=16 flat=0; clone draws=33 fb=0; rectfill hits=65 shits=108; dopus calls=9 flat=9 arrow=0
After picks:     frame calls=42 flat=4; clone draws=63 fb=0; rectfill hits=114 shits=192; dopus calls=18 flat=18 arrow=0
After scroll (6 arrow clicks, 3 track): frame calls=42 flat=4 (unchanged); clone draws=66; rectfill hits=121 shits=226; dopus arrow=0
Pixels after scroll: knob 8D8D8D, track 000000, arrows 8D8D8D = pal3 = FILLPEN in vantablack (pens: 3 -> 8D8D8D); theme knob pen is pal233 1A1A1A (pens-held knob=233). So knob NOT in theme knob pen (but FILLPEN is grey here, not blue: not conclusive for blue).
