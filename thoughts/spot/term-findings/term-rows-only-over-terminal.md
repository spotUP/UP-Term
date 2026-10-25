# TERM-ROWS-ONLY-OVER-TERMINAL evidence

Design: mn_row_visible / mn_children_t (amarchy/menu_tree.c) hide tab:, menuid:, slash: rows and groups with no visible row. lf_terminal_present() (amarchy/leaf.c: upactive_prev + FindPort) decides once at menu open; menu.c and gui.c pass it to ix_level_t.

Host: tests/test_keymap.c terminal_rows_show_only_over_a_terminal. Fix: `[OK] keymap: 1753 checks, 0 failed`. With mn_row_visible forced to 1: `[FAIL] keymap: 1753 checks, 7 failed`.

Amiga build: Amarchy, amarchy-menu, amarchy-gui build clean (needs UPTERM_ROOT=/Users/spot/Code/UP-Term for libupkgui). menu.c and gui.c keep __stack + $STACK cookie.

Rig 4: booted, commodity started. Rows rows_no_terminal / rows_over_terminal FAIL: the quick menu on the rig is amarchy-gui (graphical window "Amarchy"), so screen_text (clipboard copy of an UP-Term window) returns empty; the rows cannot be read that way. Needs a reader for the GUI menu (uitree has only custom gadgets) or a rig run of amarchy-menu (text). Not done.

Open: window sizing (mn_osd_size, mg_width) still uses the full tree; amarchy_geom.c unchanged. An existing unfocused UP-Term window still counts as present (MRU).
