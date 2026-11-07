#!/usr/bin/env python3
"""test_gui_rig_flat_prefs_default.py -- amarchy_gui_rig reads the flat prefs with their default ON.

  python3 -I tests/test_gui_rig_flat_prefs_default.py

B9 (rig 96): --setup printed "FAIL setup: flat prefs off ..." because flat_prefs_on() demanded an explicit `= on`, but
both flat keys default to on (amconf_core.h INSTALLER-FLAT-OFF) and the installed prefs file is header-only. Exit 1 on failure."""
import pathlib, sys, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
import amarchy_gui_rig as g

HDR = '; Amarchy preferences. Written by the Amarchy preferences window (Exchange: Show Interface).\n'
OFF2 = 'flat-system-look = off\nflat-window-gadgets = off\n'
for text, want, why in ((HDR, True, 'header-only file: defaults on'),
                        ('Type: object not found\n', True, 'no file: defaults on'),
                        (HDR + 'flat-system-look = on\n', True, 'explicit on'),
                        (HDR + 'flat-system-look = off\n', True, 'gadgets key missing: default on'),
                        (HDR + OFF2, False, 'both explicit off')):
    got = g.flat_prefs_text_on(text)
    if got != want:
        sys.exit('FAIL: flat_prefs_text_on(%s) is %s, want %s' % (why, got, want))
print('OK flat prefs read with default on; off only when both keys say off')
