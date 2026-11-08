#!/usr/bin/env python3
"""test_amarchy_gui_rig_zombie_menu.py -- open_menu() opens the menu when a theme pick's apply left the menu owner alive without a window.

  python3 -I tests/test_amarchy_gui_rig_zombie_menu.py

DT-6: after a theme pick the port AMARCHY.MENU stays up with no menu window; the next LAmiga+Space then CLOSES that owner
instead of opening a menu ("open menu: no stable state within 60s (last None)" on the second pick of a rig run). The driver
sees the port, presses the hotkey once to end the owner, then once more to open. Fake rig: window and port are separate
states; the hotkey with the port up and no window ends the port, with the port down it opens window and port. Exit 1 on failure."""
import pathlib, sys, time, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
import amarchy_gui_rig as g

state = {'win': False, 'port': True, 'keys': 0}


def key(code, qual=0):
    state['keys'] += 1
    if (code, qual) == (g.SPACE, g.LAMIGA):
        if state['port']:
            state['port'] = state['win'] = False   # the toggle ends the open owner
        else:
            state['port'] = state['win'] = True


g.ami.key = key
g.where = lambda title: ({'row_scale': 1}, {'title': title}) if title == g.TITLE and state['win'] else (None, None)
g.menu_port_up = lambda: state['port']
time.sleep = lambda s: None

s, w = g.open_menu()
if not w:
    sys.exit('FAIL: open_menu() with a windowless menu owner left the menu closed')
if state['keys'] != 2:
    sys.exit('FAIL: expected 2 hotkey presses (end the owner, open), saw %d' % state['keys'])
print('OK open_menu() ends a windowless menu owner first and then opens the menu')
