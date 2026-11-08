#!/usr/bin/env python3
"""test_amarchy_pick_keep_checked.py -- a rig theme pick with keep=True fails loudly when the FP-1 "Keep?" click is missed.

  python3 -I tests/test_amarchy_pick_keep_checked.py

DT-9 (rig 63): the click was missed silently (keep_requester returned early on apply_settled before the requester opened, and the
pick never read the result); Amarchy reverted the theme 15 s later and the next pick landed on the same theme. Now pick(keep=True)
raises unless Keep was clicked, and keep_requester(expect=True) ignores apply_settled and waits for the requester or the full
timeout. Pattern copied from tests/test_amarchy_keep_wait.py (fake clock, stubbed ami/ui_click) and
tests/test_amarchy_desk_rig_pick_waits.py (stubbed rig modules). Exit 1 on failure."""
import pathlib, sys, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig', 'amarchy_gui_rig', 'amarchy_tile_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
g = sys.modules['amarchy_gui_rig']
g.UP, g.DOWN, g.RETURN, g.TITLE = 'up', 'down', 'return', 'Amarchy'
import amarchy_theme_save_rig as t
import amarchy_desk_rig as d

now = [0.0]
t.time = types.SimpleNamespace(time=lambda: now[0], sleep=lambda s: now.__setitem__(0, now[0] + s))
clicks = []
REQ_AT = 6.0   # the requester opens 6 s in; apply_settled is already True (the DT-9 race)


def requester_open():
    return now[0] >= REQ_AT and not clicks


class Conn:
    def __enter__(self): return self
    def __exit__(self, *a): return False


def ui_click(cn, label, title, nth=1):
    if not requester_open():
        raise RuntimeError('no such window')
    clicks.append(label)


t.ami._conn = lambda secs: Conn()
t.ami._m = lambda: types.SimpleNamespace(ui_click=ui_click)
t.requester_open = requester_open
fails = []

# 1. keep_requester(expect=True) clicks a requester that opens after apply_settled already said True
now[0] = 0.0; clicks.clear()
try:
    kept = t.keep_requester(30, 0.0, lambda: True, expect=True)
    if not kept or clicks != ['Keep']:
        fails.append('expect=True with settled True: Keep not clicked (kept=%s)' % kept)
except TypeError as e:
    fails.append('keep_requester has no expect mode: %s' % e)

# 2. the default (no expect) still leaves early when settled and nothing is open (callers that expect no requester)
now[0] = 0.0; clicks.clear()
kept = t.keep_requester(30, 0.0, lambda: True)
if kept or now[0] > 5:
    fails.append('default mode with settled True must return False early (kept=%s at %.1f s)' % (kept, now[0]))

# 3. pick(keep=True) raises when Keep was not clicked, and does not when it was
env = {'AMARCHY_ROWS': 'a (current)|b|c', 'AMARCHY_SEL': '0'}
t.c.run = lambda cmd: (0, '')
t.open_menu = lambda: None
t.wait_stable = lambda *a, **k: (('s',), 0.0)
t.step = lambda key, what: (('s',), 0.0)
t.style_downs = lambda: 0
t.env = lambda name: env[name]
t.wait_apply_done = lambda: True
t.menu_state = lambda: ('s',)
t.ami.key = lambda *a, **k: None
g.close_menu = lambda: True
for result, should_raise in ((False, True), (True, False)):
    t.keep_requester = lambda *a, _r=result, **k: _r
    try:
        d.pick(keep=True)
        raised = False
    except Exception:
        raised = True
    if raised != should_raise:
        fails.append('pick(keep=True) with Keep result %s: raised=%s, expected %s' % (result, raised, should_raise))
t.keep_requester = lambda *a, **k: (_ for _ in ()).throw(AssertionError('keep=False must not click'))
try:
    d.pick(keep=False)
except AssertionError as e:
    fails.append(str(e))

for f in fails:
    print('[FAIL] ' + f)
print('[FAIL] test_amarchy_pick_keep_checked' if fails else '[OK] test_amarchy_pick_keep_checked')
sys.exit(1 if fails else 0)
