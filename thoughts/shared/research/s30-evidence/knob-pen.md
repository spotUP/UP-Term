# Scroller knob own pen (rig 56, Amiberry, OS 3.2)

Owner decision 2026-11-02: knob too bright. Own pen = surface mixed toward muted text; follows every theme; depth <= 3 falls back to DK_MUTED.

Design (reuses the pen_dim precedent): amarchy/flat.c dim_refresh reads RGB of pen_back (surface) and pen_dimtext (muted), mixes with flat_knob_colour (flat_core.c, FLAT_KNOB_PCT 60 toward muted), ObtainBestPen -> flat_state.pen_knob (offset 24, FLAT_STATE_SIZE 28). flat_knob_pen returns it at depth 4..8, else DK_MUTED. flat_scroller_pen uses it for the knob.

Tests: make test ONLY=flat_core 862 checks 0 failed (test_knob_pen: pen used at depth 4/8 and via flat_scroller_pen, depth 3 gives DK_MUTED, invalid pen gives DK_MUTED, vantablack mix 494949, light pair strictly between). make test ONLY=desk 2202 checks 0 failed (test_knob_colour: every shipped theme between bg and muted; vantablack 494949). make amiga rc 0. Revert-fails not run by rebuild; by construction the tests need the new symbols.

Percent note: brief says "halfway" and "about 4A4A4A". True 50 percent of 000000/7A7A7A is 3D3D3D; 60 percent truncates to 494949. Owner to confirm; change = FLAT_KNOB_PCT and the two test values.

Rig proof: UPTERM_RIG=56 rig.py start --max --emu amiberry --os 32 (--allow-stale-base: branch is off local main 8add5dd1, 5 origin commits missing, not touching flat); amarchy_gui_rig.py --setup; amarchy-flat to RAM:, flat prefs on, vantablack picked via amarchy_desk_rig.pick. Status line: pens-held 16 ... dim=250 knob=249 (screen depth 8). Shot ~/Desktop/upterm-shots/s30-knob-pen.png (640x256 grab, PIL):
| check | measured | verdict |
|---|---|---|
| vertical knob x626..635 y23..212 | 494949, 190 of 190 rows | PASS |
| horizontal knob y238..243 | 494949, 585 px per row, all knob | PASS |
| track beside knob | 000000 | PASS |
| file/drawer names | 7A7A7A (2170 px) and 8D8D8D unchanged | PASS |
Not measured: depth <= 3 on a rig (host test only). A 222222 body fill appears in the grab (not from this change; unexamined).
