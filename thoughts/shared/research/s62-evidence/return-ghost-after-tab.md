# Return takes a ghost only after Tab (owner 2026-11-06)

Cause: a994d26e made Return insert any suggestion(), so a passive history/file ghost was run (CD RAM: became Cd RAM:rig-libs-done).
Fix: le_line.sug_mode/sug_key (replaces sug_off; LE_SUG_OFF = Esc drop, LE_SUG_TAB = Tab pressed with a ghost shown, keyed on line_key). Handler calls le_tab_key() on Tab. Any key but Return clears TAB mode. Return inserts only in TAB mode with an unchanged line. Right/End unchanged.
Size: con is at its 26000 byte bound; fields packed (ghost_file, ghost_asked_file now unsigned char) so le_line does not grow (sentinel 3200 holds).
Test: tests/test_lineedit.c return_takes_a_ghost_only_when_tab_showed_it. Reverting the gate: FAIL at lines 241, 254, 268 (passive history ghost, Esc after Tab, passive file ghost); with fix 484 checks, 0 failed.
Note: in the real handler Tab normally inserts the completion text itself; the ghost path matters when Tab leaves a ghost shown (no change to the line). Not rig-tested.
