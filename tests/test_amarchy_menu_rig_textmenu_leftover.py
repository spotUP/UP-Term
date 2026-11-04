#!/usr/bin/env python3
"""test_amarchy_menu_rig_textmenu_leftover.py -- a row that leaves the Amarchy text quick menu open is closed and checked.

  python3 -I tests/test_amarchy_menu_rig_textmenu_leftover.py

AM-RIG-TEXTMENU-LEFTOVER: the text menu is an untitled 296x224 XCON window; rig rows left it open. run_row() must Esc
it after the row and FAIL the row if an untitled 296x224 window remains. Fake window list, no rig. Exit 1 on failure."""
import pathlib, sys, time, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'verdicts', 'ami', 'condev_rig', 'wasabi', 'stage_rig', 'tabstate'):
    sys.modules.setdefault(name, types.ModuleType(name))
sys.modules['paths'].REPORTS = '/nonexistent'
sys.modules['paths'].reports_dir = lambda n: pathlib.Path('/nonexistent')
sys.modules['condev_rig'].run = lambda *a, **k: (0, '')
sys.modules['wasabi'].__dict__.setdefault('x', 0)
import amarchy_menu_rig as m
time.sleep = lambda s: None

MENU = {'addr': 0x1234, 'title': '', 'width': 296, 'height': 224}
OTHER = {'addr': 0x5678, 'title': 'menuhost', 'width': 296, 'height': 224}


def scenario(stuck):
    st = {'wins': [dict(MENU), dict(OTHER)], 'keys': []}

    def key(code, qual=0):
        st['keys'].append(code)
        if code == m.RAW['esc'] and not stuck:
            st['wins'] = [w for w in st['wins'] if w['addr'] != MENU['addr']]
    sys.modules['wbrun_rig'] = types.SimpleNamespace(windows=lambda: list(st['wins']))
    m.ami.key = key
    return st


st = scenario(stuck=False)
ok, detail = m.run_row(lambda: (True, 'row ok'))
if not ok or m.RAW['esc'] not in st['keys'] or m.text_menu_windows(st['wins']):
    sys.exit('FAIL: leftover text menu not closed by run_row (ok=%s keys=%s %s)' % (ok, st['keys'], detail))
scenario(stuck=False)
st = scenario(stuck=False)
st['wins'] = [dict(OTHER)]
ok, detail = m.run_row(lambda: (True, 'row ok'))
if not ok or st['keys']:
    sys.exit('FAIL: Esc sent although no text menu was open (keys %s)' % st['keys'])
st = scenario(stuck=True)
ok, detail = m.run_row(lambda: (True, 'row ok'))
if ok or '4660' not in detail:
    sys.exit('FAIL: a text menu that survives Esc did not fail the row (ok=%s %s)' % (ok, detail))
print('OK run_row() closes a leftover text menu and fails the row when it stays')
