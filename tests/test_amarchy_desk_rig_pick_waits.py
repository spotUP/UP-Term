#!/usr/bin/env python3
"""test_amarchy_desk_rig_pick_waits.py -- a theme pick on the rig returns only after Amarchy finished applying it, and
closes the menu only then.

  python3 -I tests/test_amarchy_desk_rig_pick_waits.py

S28-PENS-NOT-LIVE-AGAIN: on a 3.2 rig the theme list recolours about 5 s before desk_dos_apply returns (Palette USE,
IPrefs reset wait, Workbench USE); a pen read before that sees the stock pens.

THEME-PICK-MENU-STABLE: desk_dos_apply also hides the menu window (gui.c gui_hide) and opens it again on the same level
when it returns (gui_show). close_menu() run during the apply saw no menu window and stopped, the menu came back open,
and the next pick's Left Amiga + Space closed it (MENU-TOGGLE): "open menu: no stable state within 60s". Both pick
paths (amarchy_desk_rig.pick and amarchy_theme_save_rig.one_select) must wait for AMARCHY_WBCLOSES (written by
lf_gt_select, amarchy/leaf.c, once the apply returned) BEFORE closing the menu.

The fake rig models that: after the picking Return the menu shows the recoloured list, is hidden from the next rig
command on until the apply finishes (some AMARCHY_WBCLOSES polls later), then shows again; close_menu() on a hidden
menu closes nothing. A pick is correct when the menu is closed at the end. Exit 1 on failure."""
import pathlib, sys, time, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig',
             'amarchy_gui_rig', 'amarchy_tile_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
g = sys.modules['amarchy_gui_rig']
g.UP, g.DOWN, g.RETURN, g.TITLE = 'up', 'down', 'return', 'Amarchy'
import amarchy_theme_save_rig as t
import amarchy_desk_rig as d

APPLY_POLLS = 6                     # the apply finishes on the 6th AMARCHY_WBCLOSES poll after Return
state = {}


def reset():
    state.clear()
    state.update(env={'AMARCHY_WBCLOSES': '1'}, menu='closed', applying=False, polls=0, finishes=APPLY_POLLS, returns=0)


def run(cmd):
    if state['applying'] and state['menu'] == 'recoloured':
        state['menu'] = 'hidden'                    # gui_hide: the window goes while the palette is written
    if cmd.startswith('Delete') and 'AMARCHY_WBCLOSES' in cmd:
        state['env'].pop('AMARCHY_WBCLOSES', None)
        return 0, ''
    if cmd.startswith('GetEnv') and 'AMARCHY_WBCLOSES' in cmd:
        if state['applying']:
            state['polls'] += 1
            if state['polls'] >= state['finishes']:
                state['applying'] = False
                state['env']['AMARCHY_WBCLOSES'] = '2'
                if state['menu'] == 'hidden':
                    state['menu'] = 'open'          # gui_show: the menu comes back on the level it was on
        return (0, '') if 'AMARCHY_WBCLOSES' in state['env'] else (5, '')
    return 0, ''


def key(k, *mods):
    if k == g.RETURN and state['menu'] == 'open':
        state['returns'] += 1                       # Return 1 enters Style, 2 the theme list, 3 picks
        if state['returns'] == 3:
            state.update(applying=True, menu='recoloured')


def close_menu():
    if state['menu'] in ('open', 'recoloured'):     # Escape reaches only a menu window that is there
        state['menu'] = 'closed'
    return state['menu'] != 'open'


def open_menu():
    state['menu'] = 'closed' if state['menu'] == 'open' else 'open'   # MENU-TOGGLE
    state['returns'] = 0


g.close_menu = close_menu
g.where = lambda title: ('s', {'title': title}) if state['menu'] in ('open', 'recoloured') else (None, None)
t.c.run = d.c.run = run
t.ami.key = key
t.open_menu = open_menu
t.OPEN_SECS = t.STEP_SECS = 1


def wait_stable(*a, **k):
    """The recoloured list is seen stable; gui_hide follows while the caller goes on"""
    if state['applying'] and state['menu'] == 'recoloured':
        state['menu'] = 'hidden'
    return ('box', 'px'), 0.0


t.wait_stable = wait_stable
t.style_downs = lambda: 0
t.step = lambda k, what: (key(k), (('box', 'px'), 0.0))[1]
t.env = lambda name: {'AMARCHY_ROWS': 'mono (current)|vantablack', 'AMARCHY_SEL': '1'}.get(name, '')
time.sleep = lambda s: None
fails = []

# amarchy_desk_rig.pick
reset()
name, up = d.pick()
if name != 'vantablack':
    fails.append('pick() picked %r, expected vantablack' % name)
if state['applying'] or 'AMARCHY_WBCLOSES' not in state['env']:
    fails.append('pick() returned before the apply finished (%d of %d polls)' % (state['polls'], APPLY_POLLS))
if state['menu'] != 'closed':
    fails.append('pick() left the menu %s: it closed it while the apply hid it, the next pick\'s hotkey closes it' % state['menu'])

reset()
state['finishes'] = 10 ** 9                         # an apply that never finishes
t.APPLY_SECS = 0.05
try:
    d.pick()
except TimeoutError:
    pass
else:
    fails.append('pick() returned although the apply never finished')
t.APPLY_SECS = 60

# amarchy_theme_save_rig.one_select: stop right after its menu path (the checks after it read the rig)
reset()
t.theme_line = lambda p: 'mono'
t.dopus_pens_reset = lambda: None


class Done(Exception):
    pass


def after_path(name):
    raise Done()


t.header_accent_pixels = lambda rgb: 99
t.theme_accent = lambda name: (0, 0, 0)
t.strangers = after_path
try:
    t.one_select(1, set())
except Done:
    pass
if state['applying'] or 'AMARCHY_WBCLOSES' not in state['env']:
    fails.append('one_select() went on before the apply finished (%d of %d polls)' % (state['polls'], APPLY_POLLS))
if state['menu'] != 'closed':
    fails.append('one_select() left the menu %s: the second select\'s hotkey closes it ("open menu: no stable state")' % state['menu'])

if fails:
    sys.exit('FAIL: ' + '; '.join(fails))
print('OK pick() and one_select() wait for AMARCHY_WBCLOSES (apply done) and close the menu after it')
