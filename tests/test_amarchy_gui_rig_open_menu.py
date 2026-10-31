#!/usr/bin/env python3
"""test_amarchy_gui_rig_open_menu.py -- open_menu() opens the menu even when an earlier step left it open.

  python3 -I tests/test_amarchy_gui_rig_open_menu.py

RJ7-DRAWN-MENU-NOOPEN: LAmiga+Space toggles the Amarchy menu (amarchy.c mo_close_open). After a Packages job, Esc
in the job view only climbs back to the Packages list, so the menu window stays open; the next drawn run's
open_menu() pressed the hotkey, closed that menu and reported "menu window False". This drives open_menu() against
a fake rig whose menu window is still open and asserts it ends with the window open. Exit 1 on failure."""
import os, pathlib, sys, time, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
import amarchy_gui_rig as g

state = {'open': True, 'keys': []}          # the menu a Packages job left open


def key(code, qual=0):
    state['keys'].append((code, qual))
    if (code, qual) == (g.SPACE, g.LAMIGA):
        state['open'] = not state['open']   # the hotkey toggles
    elif code == g.ESC:
        state['open'] = False               # enough Escapes close it; one is enough for the fake


g.ami.key = key
g.where = lambda title: ({'row_scale': 1}, {'title': title}) if title == g.TITLE and state['open'] else (None, None)
time.sleep = lambda s: None

s, w = g.open_menu()
if not w:
    sys.exit('FAIL: open_menu() with the menu already open left it closed (keys %s)' % state['keys'])
if state['keys'][-1] != (g.SPACE, g.LAMIGA) or (g.ESC, 0) not in state['keys']:
    sys.exit('FAIL: open_menu() did not close the leftover menu before the hotkey (keys %s)' % state['keys'])
print('OK open_menu() reopens the menu at its root level after a leftover one')
