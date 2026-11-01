#!/usr/bin/env python3
"""test_amarchy_desk_rig_pick_waits.py -- pick() returns only after Amarchy finished applying the picked theme.

  python3 -I tests/test_amarchy_desk_rig_pick_waits.py

S28-PENS-NOT-LIVE-AGAIN: on a 3.2 rig the theme list recolours about 5 s before desk_dos_apply returns (Palette USE,
IPrefs reset wait, Workbench USE). pick() used to close the menu, sleep 3 s and return, so the caller read the stock
Workbench pens and a flat frame hook with no flat frames, and reported "flat pens not live" for a product that works.
pick() now deletes AMARCHY_WBCLOSES before the pick and waits until lf_gt_select (amarchy/leaf.c) writes it again.
This drives pick() against a fake rig whose apply finishes several polls after the menu closed and asserts pick()
does not return before that. Exit 1 on failure."""
import pathlib, sys, time, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig',
             'amarchy_gui_rig', 'amarchy_theme_save_rig', 'amarchy_tile_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
g = sys.modules['amarchy_gui_rig']
g.UP, g.DOWN, g.RETURN = 'up', 'down', 'return'
g.close_menu = lambda: state.update(menu_closed=True)
import amarchy_desk_rig as d

APPLY_POLLS = 6                     # the apply finishes on the 6th poll after the menu closed
state = {'env': {'AMARCHY_WBCLOSES': '1'}, 'menu_closed': False, 'polls': 0, 'returned_at': None}


def run(cmd):
    if cmd.startswith('Delete') and 'AMARCHY_WBCLOSES' in cmd:
        state['env'].pop('AMARCHY_WBCLOSES', None)
        return 0, ''
    if cmd.startswith('GetEnv') and 'AMARCHY_WBCLOSES' in cmd:
        if state['menu_closed']:
            state['polls'] += 1
            if state['polls'] >= APPLY_POLLS:
                state['env']['AMARCHY_WBCLOSES'] = '2'
        return (0, '') if 'AMARCHY_WBCLOSES' in state['env'] else (5, '')
    return 0, ''


d.c.run = run
d.ami.key = lambda *a: None
d.t.open_menu = lambda: None
d.t.OPEN_SECS = d.t.STEP_SECS = 1
d.t.wait_stable = lambda *a, **k: ('state', None)
d.t.style_downs = lambda: 0
d.t.step = lambda *a: ('state', None)
d.t.env = lambda name: {'AMARCHY_ROWS': 'mono (current)|vantablack', 'AMARCHY_SEL': '1'}.get(name, '')
d.t.menu_state = lambda: (None, None)
time.sleep = lambda s: None

name, up = d.pick()
if name != 'vantablack':
    sys.exit('FAIL: pick() picked %r, expected vantablack' % name)
if state['polls'] < APPLY_POLLS or 'AMARCHY_WBCLOSES' not in state['env']:
    sys.exit('FAIL: pick() returned before the apply finished (%d of %d polls, AMARCHY_WBCLOSES %s)'
             % (state['polls'], APPLY_POLLS, 'set' if 'AMARCHY_WBCLOSES' in state['env'] else 'unset'))

state.update(env={}, menu_closed=False, polls=-10 ** 9)   # an apply that never finishes
d.APPLY_SECS = 0.05
try:
    d.pick()
except TimeoutError:
    pass
else:
    sys.exit('FAIL: pick() returned although the apply never finished')
print('OK pick() returns only after AMARCHY_WBCLOSES is written again (apply done)')
