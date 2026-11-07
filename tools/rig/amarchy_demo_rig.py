#!/usr/bin/env python3
"""amarchy_demo_rig.py -- the Amarchy demo tour (`Amarchy demo`, the DEMO verb, amarchy/demo.c). Rows run in order and
share state (the pre-demo window boxes), like the sibling amarchy_bar_rig.py. A verdict per row in
<RIG>/amarchy/demo_<row>.verdict; --only <row>, --fresh.

The rows drive the CLI form (`Amarchy demo` through wasabi run), the same port verb as the menu row:
  start        Amarchy up, the window boxes before the demo are recorded
  tour         `Amarchy demo` returns running=1; AMARCHY_DEMO_SCENE (GetEnv) reaches scene 3 within 60 s, polled every 5 s
  status       `Amarchy demo status` shows running=1
  key_stops    one key through wasabi `key`; within 3 s AMARCHY_DEMO_DONE exists and status shows running=0
  boxes        every window box equals the pre-demo box
Rig window purpose text to pass: "up-term-49: B4 Amarchy demo rig test".
Needs the rig up (rig.py start --max --purpose ...), Amarchy in UP-Term:bin (stage_rig puts it there)."""
import argparse, pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths, verdicts
import ami, condev_rig as c
import stage_rig, tabstate
import amarchy_rig as ar
from amarchy_bar_rig import windows, wb_front

OUT = pathlib.Path(paths.REPORTS) / 'amarchy'
RAW_SPACE = 0x40
BEFORE = []


def cli(arg=''):
    """`Amarchy demo [arg]` as a user types it; returns (rc, text)."""
    return c.run(('%s/Amarchy demo %s' % (paths.UPTERM_BIN, arg)).strip())


def env(name):
    rc, out = c.run('GetEnv %s' % name)
    return out.strip() if rc == 0 else ''


def user_boxes():
    # boxes only: a title can change (the RAM Disk window shows its free space)
    return sorted(b for t, b, _ in windows() if not t.startswith(('Amarchy', 'Welcome to Amarchy')))


def settled():
    """The window list once three reads 3 s apart agree: a start-up window (Welcome, a tab host) that closes by itself
    is not part of the desktop the demo has to give back, and the demo ends when a snapshot window disappears."""
    last = None
    for _ in range(40):
        now = user_boxes()
        same = now == last
        last = now
        if same and user_boxes() == now:
            return now
        time.sleep(3)
    return last


def r_start():
    ar.arexx('QUIT')
    time.sleep(2)
    c.run('Delete ENV:AMARCHY_DESK_START_WRITES QUIET')
    ar.start()
    # Amarchy's own start apply runs on the main task about 30 s after start (the rig has no Workbench to wait for) and
    # blocks the loop for up to 20 s; the tour must not start inside it, or its first scenes are late and a key stops late.
    for _ in range(40):
        if env('AMARCHY_DESK_START_WRITES') != '':
            break
        time.sleep(3)
    c.run('Delete ENV:AMARCHY_DEMO_SCENE ENV:AMARCHY_DEMO_DONE ENV:AMARCHY_DEMO_WHY QUIET')
    BEFORE[:] = settled()
    return ar.port_up() and bool(BEFORE), 'port up %r, boxes before %r' % (ar.port_up(), BEFORE)


def r_tour():
    rc, out = cli()
    if 'running=1' not in out:
        return False, 'start reply rc %d %r' % (rc, out.strip())
    scene = ''
    for _ in range(14):                       # 14 polls of 5 s: scene 3 starts at 24 s
        time.sleep(5)
        scene = env('AMARCHY_DEMO_SCENE')
        if scene.isdigit() and int(scene) >= 3:
            break
    return scene.isdigit() and int(scene) >= 3, 'scene %r after the polls' % scene


def r_status():
    rc, out = cli('status')
    return 'running=1' in out, 'status %r (scene %r, why %r)' % (out.strip(), env('AMARCHY_DEMO_SCENE'), env('AMARCHY_DEMO_WHY'))


def r_key_stops():
    ami.key(RAW_SPACE, 0)
    ami.key(RAW_SPACE, 0x80)
    time.sleep(3)
    done = env('AMARCHY_DEMO_DONE')
    rc, out = cli('status')
    return done != '' and 'running=0' in out, 'DONE %r status %r why %r' % (done, out.strip(), env('AMARCHY_DEMO_WHY'))


def r_boxes():
    now = user_boxes()
    return now == BEFORE, 'before %r now %r' % (BEFORE, now)


ROWS = [('start', r_start), ('tour', r_tour), ('status', r_status), ('key_stops', r_key_stops), ('boxes', r_boxes)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', help='row name, or a comma list of rows')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    a.only = verdicts.parse_only(a.only, [n for n, _ in ROWS])
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    tabstate.open_host('demohost')
    passed = total = 0
    for name, fn in ROWS:
        if a.only and name not in a.only:
            continue
        wb_front()
        ok, detail = fn()
        (OUT / ('demo_' + name + '.verdict')).write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, detail), flush=True)
    print('amarchy_demo_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
