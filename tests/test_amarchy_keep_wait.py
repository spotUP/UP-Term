#!/usr/bin/env python3
"""test_amarchy_keep_wait.py -- a theme pick waits for the FP-1 Keep/Revert requester until it is seen, however late.

  python3 -I tests/test_amarchy_keep_wait.py

THEME-SAVE-RIG-KEEP-SECS: on a fresh rig the first pick's requester "Amarchy desktop theme" opens later than the old fixed
12 s wait (measured rig 149): the harness gave up, nobody answered, the requester reverted the apply 15 s later. The wait
now ends on the requester, or on apply_settled (apply done, T:amarchy-revert/pending absent: nothing will be asked), bounded
by KEEP_SECS. Fake clock, no real sleep. Exit 1 on failure."""
import pathlib, sys, types

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
for name in ('paths', 'ami', 'condev_rig', 'stage_rig', 'amarchy_window_rig', 'upkg_rig', 'amarchy_gui_rig', 'amarchy_tile_rig'):
    sys.modules.setdefault(name, types.ModuleType(name))
import amarchy_theme_save_rig as t

now = [0.0]
clock = types.SimpleNamespace(time=lambda: now[0], sleep=lambda s: now.__setitem__(0, now[0] + s))
t.time = clock
st = {}
clicks = []


def reset(requester_at, apply_done_at=3.0, marker=True):
    now[0] = 0.0
    clicks.clear()
    st.update(requester_at=requester_at, apply_done_at=apply_done_at, marker=marker)


def requester_open():
    return st['requester_at'] is not None and now[0] >= st['requester_at'] and not clicks


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


def run(cmd):
    if cmd.startswith('GetEnv'):
        return (0, '') if now[0] >= st['apply_done_at'] else (5, '')
    if cmd.startswith('Type'):
        return (0, '') if st['marker'] and not clicks else (10, '')
    return 0, ''


t.c.run = run
fails = []
EXTRA = [t.apply_settled] if hasattr(t, 'apply_settled') else []   # the unfixed harness has neither

# a requester 15 s after Return still gets Keep (the old 12 s wait gave up)
reset(requester_at=15.0)
kept = t.keep_requester(t.KEEP_SECS, 0.0, *EXTRA)
if not kept or clicks != ['Keep']:
    fails.append('requester at 15 s: Keep not clicked (kept=%s clicks=%s at %.1f s)' % (kept, clicks, now[0]))

# a pick that asks nothing (apply done at 3 s, no marker) returns long before the limit
reset(requester_at=None, marker=False)
kept = t.keep_requester(t.KEEP_SECS, 0.0, *EXTRA)
if kept or now[0] > 10:
    fails.append('pick that asks nothing: kept=%s, waited %.1f s of %d' % (kept, now[0], t.KEEP_SECS))

# apply done but the marker is present: the requester is coming, keep waiting for it
reset(requester_at=20.0, marker=True)
kept = t.keep_requester(t.KEEP_SECS, 0.0, *EXTRA)
if not kept:
    fails.append('marker present, requester at 20 s: gave up at %.1f s' % now[0])

# no requester and a marker that never goes: bounded by KEEP_SECS, False
reset(requester_at=None, marker=True)
kept = t.keep_requester(t.KEEP_SECS, 0.0, *EXTRA)
if kept or now[0] < t.KEEP_SECS:
    fails.append('stuck marker: kept=%s after %.1f s, expected the full %d s bound' % (kept, now[0], t.KEEP_SECS))

for f in fails:
    print('[FAIL] ' + f)
print('[FAIL] test_amarchy_keep_wait' if fails else '[OK] test_amarchy_keep_wait')
sys.exit(1 if fails else 0)
