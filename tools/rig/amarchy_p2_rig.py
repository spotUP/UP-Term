#!/usr/bin/env python3
"""amarchy_p2_rig.py -- reachability of Amarchy P2 (plan 2026-10-29-amarchy-p2-status-bar.md, R2.16). Rig purpose text:
"Amarchy s24: P2 tick and BAR STATUS rig check". Needs the rig up (UPTERM_RIG=<n> rig.py start --max --purpose ...);
this script never starts or stops a rig. Entry point: the ARexx port AMARCHY of the running commodity (the same
rexx_cmd dispatch a script or menu row reaches). Helpers are amarchy_tile_rig's (send, wait_for, bar_box, stage).

Steps, in order (each prints "[OK] <step>: ..." or "[FAIL] <step>: <detail>"); later P2 rows add steps here:
  start        Amarchy restarted; STATUS answers, ends with ticks=N, fits the 160 byte reply (no cut key)
  tick_rises   R2.3a/R2.9: ticks= rises by 2 to 4 in 3 s (the 1 s commodity tick runs)
  bar_status   BAR STATUS answers "bar=1 draws=N panels=N launches=N raises=N ticks=N", draws >= 1, a window "Amarchy bar"
  bar_off      BAR OFF: bar=0, no window "Amarchy bar", ticks still rising (the tick outlives the bar)
  bar_on       BAR ON: bar=1, the window is back, ticks still rising
  status_keys  STATUS no longer carries draws= panels= launches= raises= (they moved to BAR STATUS)
  python3 tools/rig/amarchy_p2_rig.py [--only STEP] [--no-build]
Resumable: --only STEP reruns one step against the state the earlier steps left; verdicts are written to
<RIG>/amarchy-p2/verdict.json as each step completes. Exit code is nonzero when any step fails."""
import argparse, json, pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import condev_rig as c
import stage_rig
import amarchy_tile_rig as t

OUT = pathlib.Path(paths.RIG) / 'amarchy-p2'
KEYS_BAR = ['bar', 'draws', 'panels', 'launches', 'raises', 'ticks']


def kv(text):
    return {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', text)}


def status():
    rc, res = t.send('AMARCHY', 'STATUS', timeout=5)
    return rc, res


def ticks():
    rc, res = status()
    return kv(res).get('ticks') if rc == 0 else None


def rises(secs=3):
    t0 = ticks()
    time.sleep(secs)
    t1 = ticks()
    return t0, t1, t0 is not None and t1 is not None and 2 <= t1 - t0 <= 4


def s_start():
    t.send('AMTILE', 'QUIT')
    t.send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    ok = t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
    rc, res = status()
    return bool(ok) and rc == 0 and res.endswith(tuple('0123456789')) and 'ticks=' in res, 'len %d: %r' % (len(res), res)


def s_tick_rises():
    t0, t1, ok = rises()
    return ok, 'ticks %r -> %r in 3 s' % (t0, t1)


def s_bar_status():
    rc, res = t.send('AMARCHY', 'BAR STATUS', timeout=5)
    d = kv(res)
    ok = rc == 0 and list(d) == KEYS_BAR and d['bar'] == 1 and d['draws'] >= 1 and t.bar_box() is not None
    return ok, 'rc %r len %d: %r, bar window %r' % (rc, len(res), res, t.bar_box())


def s_bar_off():
    rc, res = t.send('AMARCHY', 'BAR OFF', timeout=5)
    d = kv(res)
    _, srs = status()
    t0, t1, rising = rises()
    gone = t.bar_box() is None
    ok = rc == 0 and d.get('bar') == 0 and kv(srs).get('bar') == 0 and gone and rising
    return ok, 'rc %r reply %r, STATUS bar=%r, window gone %s, ticks %r -> %r' % (rc, res, kv(srs).get('bar'), gone, t0, t1)


def s_bar_on():
    rc, res = t.send('AMARCHY', 'BAR ON', timeout=5)
    d = kv(res)
    t0, t1, rising = rises()
    back = t.wait_for(lambda: t.bar_box() is not None, 3)
    ok = rc == 0 and d.get('bar') == 1 and bool(back) and rising
    return ok, 'rc %r reply %r, window back %s, ticks %r -> %r' % (rc, res, bool(back), t0, t1)


def s_status_keys():
    rc, res = status()
    gone = [k for k in ('draws', 'panels', 'launches', 'raises') if k + '=' in res]
    return rc == 0 and not gone and 'bar=' in res and res.rstrip().split(' ')[-1].startswith('ticks='), \
        'moved keys still in STATUS: %r; %r' % (gone, res)


STEPS = [('start', s_start), ('tick_rises', s_tick_rises), ('bar_status', s_bar_status), ('bar_off', s_bar_off),
         ('bar_on', s_bar_on), ('status_keys', s_status_keys)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', choices=[n for n, _ in STEPS], help='run one step against the state left by earlier runs')
    ap.add_argument('--no-build', action='store_true', help='do not rebuild/stage Amarchy')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    verdict = OUT / 'verdict.json'
    done = json.loads(verdict.read_text()) if a.only and verdict.exists() else {}
    stage_rig.report(stage_rig.stage_build(names=('Amarchy',), build=not a.no_build))
    fails = 0
    for name, fn in STEPS:
        if a.only and a.only != name:
            continue
        try:
            ok, detail = fn()
        except Exception as e:
            ok, detail = False, 'exception %r' % (e,)
        fails += not ok
        done[name] = [bool(ok), detail]
        verdict.write_text(json.dumps(done))
        print('%s %s: %s' % ('[OK]' if ok else '[FAIL]', name, detail), flush=True)
    print('amarchy_p2_rig: %d step(s) failed' % fails)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
