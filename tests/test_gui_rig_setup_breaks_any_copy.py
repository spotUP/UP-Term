#!/usr/bin/env python3
"""test_gui_rig_setup_breaks_any_copy.py -- patina_gui_rig --setup Breaks every running Patina program, from any path.

  python3 -I tests/test_gui_rig_setup_breaks_any_copy.py

S57 A7 (rig 124, OS 3.2 fresh install): patina_btnbg_rig.py leaves RAM:patina-flat resident. --setup only Broke
processes loaded from UP-Term:bin/ or VTCX:Apps/bin, so RAM:patina-flat stayed and setup failed "the earlier
patina-flat resident unloaded on Break". This feeds setup_break_targets() a real 3.2 Status listing. Second gap: setup copied the kit from a fixed
Rushhours:Tools/UP-Term (3.1 kit rigs); a 3.2 install lives at SYS:UP-Term, so kit_upterm_dir() reads it from the assign. Exit 1 on failure."""
import pathlib, sys, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'patina_window_rig', 'upkg_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
import patina_gui_rig as g

STATUS = """Process  1: Loaded as command: VTC:vsh
Process  3: Loaded as command: BOOTX:wasabid
Process  4: Loaded as command: UP-Term:bin/Patina
Process  5: Loaded as command: UP-Term:bin/patina-index
Process  9: Loaded as command: Empty-12MB-FFS:UP-Term/bin/patina-gui
Process 11: Loaded as command: Empty-12MB-FFS:UP-Term/bin/patina-menu
Process 12: Loaded as command: Workbench
Process 13: Loaded as command: RAM:patina-flat
Process 14: Loaded as command: VTCX:Apps/bin/Patina
"""

got = g.setup_break_targets(STATUS)
want = ['4', '9', '11', '13', '14']
if got != want:
    sys.exit('FAIL: setup Breaks %s, want %s (RAM:patina-flat is 13, the 3.2 full-path gui/menu are 9 and 11)' % (got, want))
for out, want_dir in (('UP-Term        Empty-12MB-FFS:UP-Term\n', 'Empty-12MB-FFS:UP-Term'),
                      ('UP-Term        VTCX:Apps\n             + Empty-12MB-FFS:UP-Term\n', 'Empty-12MB-FFS:UP-Term'),
                      ('UP-Term        Rushhours:Tools/UP-Term\n', 'Rushhours:Tools/UP-Term'),
                      ('UP-Term: not assigned\n', 'Rushhours:Tools/UP-Term')):
    got_dir = g.kit_upterm_dir(out)
    if got_dir != want_dir:
        sys.exit('FAIL: kit dir from %r is %r, want %r (3.2 installs live at SYS:UP-Term, not Rushhours:)' % (out, got_dir, want_dir))
print('OK setup Breaks every Patina program copy whatever its path, and copies the kit from the installed UP-Term:')
