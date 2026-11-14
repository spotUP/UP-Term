# super+t opens a terminal when none is open

Status: NO CHANGE NEEDED. Already implemented on main by 822ada98 ("Open the first UP-Term window from New tab when no terminal is open").
- cxdesc.c:101 cx_row_installed installs KS_TERMINAL rows only for KA_TAB "new".
- cx.c:43 gate: with upactive_count()>0 and a terminal-scope row the key is passed on (handler handles it; no double open).
- patina.c:428 do_row KA_TAB -> act_open_uterm() (existing launcher).
- tests/test_keymap.c:1404-1417 pin cx_row_installed for tab:new and other T rows.
No edit, no build, no host test run (nothing to fail-before). Amiga build md5: n/a.
Rig steps owed (needs a Patina build containing 822ada98):
1. Close all UP-Term windows. 2. Press Right-less Left Amiga + t (super+t). PASS: one UP-Term window opens.
3. With a terminal open and focused, press super+t. PASS: exactly one new tab, no second window.
