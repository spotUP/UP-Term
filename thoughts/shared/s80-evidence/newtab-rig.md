# New tab rig check (commit 822ada98, binaries build/amiga dated 14 Nov 12:09, not rebuilt)

Rig 5, purpose "s80 vtcon session: New tab rig check". Driver (scratchpad newtab_drv.py) uses tools/rig ami.py, wbrun_rig.windows(),
patina_gui_rig.open_menu/walk_to, tabstate.counters(). Patina started with `Run >NIL: <UPTERM_BIN>/Patina`.

Fresh desktop evidence: windows = only 'Patina bar'; no vsh process.

(a) Menu: Super+Space, root > Window > New tab (Return).  PASS
    after: windows 'UP-Term'(XCON) + 'Patina bar'; one vsh process (Process 9).
(c) With that one window open, menu New tab again.  PASS
    after: still exactly one 'UP-Term' window; second vsh (Process 11); tabstate tabs=2 active=2 tabs-opened=1.
(b) Hotkey super+t (Left Amiga + T, raw 0x14 qual 0x40) on fresh desktop after rig reboot.  PASS
    after: one 'UP-Term' window; one vsh (Process 9); tabs=1 km-hits=0 (Patina commodity handled it, no double open).
No screenshots taken.
