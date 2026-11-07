#!/usr/bin/env python3
"""amarchy_restart_rig.py -- B2-RIG: Amarchy QUIT then start, ten times, over the hardened ipatch gates (2988e11c).

The three ipatch users (wbreset_patch.c, strut_patch.c, borderblank_patch.c) must survive a restart series: no Guru,
no MuForce hit for an Amarchy task, no *_BUSY_WAIT sentinel (the remove never had to wait a tick), and the patched
vectors (OpenWindow) still work after the last QUIT. Rows: round_1..round_10 (port up, verb answered, port gone
within 10 s), no_guru, no_muforce_hit, no_busy_wait_sentinel, patch_chain_restored.
Needs the rig up with `rig.py start --max --os 32 --mmu` and `rig.py muforce on`. Resumable like amarchy_rig.py:
verdicts in <RIG>/amarchy_restart/<row>.verdict; --only a,b, --failed, --fresh. Round timings go to timings.txt.
"""
import argparse, pathlib, subprocess, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths, verdicts
import ami, condev_rig as c
import stage_rig

OUT = pathlib.Path(paths.REPORTS) / 'amarchy_restart'
ROUNDS = 10
RIGPY = str(pathlib.Path(__file__).with_name('rig.py'))
SENTINELS = ['AMARCHY_WBRESET_BUSY_WAIT', 'AMARCHY_STRUT_BUSY_WAIT', 'AMARCHY_BORDERBLANK_BUSY_WAIT']


def arexx(cmd):
    m = ami._m()
    try:
        with ami._conn() as conn:
            return m.arexx(conn, 'AMARCHY', cmd)[2].strip()
    except m.WasabiError:
        return ''             # no such port: the commodity is not running


def port_up():
    return arexx('STATUS') != ''


def rigpy(*args):
    r = subprocess.run([sys.executable, RIGPY] + list(args), capture_output=True, text=True, timeout=120)
    return (r.stdout + r.stderr).strip()


def wait(cond, limit):
    t0 = time.time()
    while time.time() - t0 < limit:
        if cond():
            return round(time.time() - t0, 1)
        time.sleep(0.5)
    return None


def round_row(n):
    def fn():
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        up = wait(port_up, 30)
        if up is None:
            return False, 'port AMARCHY not up within 30 s'
        st = arexx('STATUS')
        arexx('QUIT')
        gone = wait(lambda: not port_up(), 10)
        with open(OUT / 'timings.txt', 'a') as f:
            f.write('round %d: port up %s s, gone %s s, STATUS %r\n' % (n, up, gone, st[:60]))
        if gone is None:
            return False, 'port still up 10 s after QUIT (up %s s)' % up
        time.sleep(2)
        return True, 'port up %.1f s, STATUS answered %r, gone %.1f s' % (up, st[:40], gone)
    return fn


def r_no_guru():
    out = rigpy('wasabi', 'guru')
    low = out.lower()
    ok = (not out) or 'no guru' in low or 'none' in low or 'no alert' in low
    return ok, 'guru: %r' % out[:200]


def r_no_muforce_hit():
    out = rigpy('muforce', 'log')
    hits = [l for l in out.splitlines() if 'amarchy' in l.lower()]
    return not hits, 'muforce log %d lines, %d for Amarchy: %r' % (len(out.splitlines()), len(hits), out[:200])


def r_no_sentinel():
    # am_sentinel() is SetVar(GVF_GLOBAL_ONLY): GetEnv prints 'object not found' and returns nonzero when unset.
    seen = []
    for s in SENTINELS:
        rc, o = c.run('GetEnv %s' % s)
        if rc == 0:
            seen.append('%s=%r' % (s, o.strip()))
    # positive control: the patch's own INSTALLED sentinel proves GetEnv sees sentinels (set to 0 by the remove)
    rc, o = c.run('GetEnv AMARCHY_BORDERBLANK_INSTALLED')
    ctl = 'control AMARCHY_BORDERBLANK_INSTALLED rc=%d %r' % (rc, o.strip())
    return (not seen) and rc == 0, ('all unset; ' if not seen else 'SET ' + ' '.join(seen) + '; ') + ctl


def clock_window():
    with ami._conn() as conn:
        scr = ami._m().list_windows(conn)[0][0]
    return [w['title'] for w in scr['windows'] if 'clock' in (w['title'] or '').lower()
            or 'clock' in (w['task'] or '').lower()]


def r_chain():
    ps = rigpy('wasabi', 'ps', 'Amarchy')
    running = [l for l in ps.splitlines() if 'amarchy' in l.lower() and l.split()[:1] != ['ps']]
    c.run('Run >NIL: SYS:Utilities/Clock')
    seen = wait(clock_window, 15)
    wins = clock_window()
    st = c.run('Status')[1]
    for l in st.splitlines():
        if 'clock' in l.lower():
            c.run('Break %s C' % l.split()[1].rstrip(':'))      # Clock quits on CTRL-C: closes the probe window
    gone = wait(lambda: not clock_window(), 10)
    return (not running) and seen is not None and gone is not None, \
        'Amarchy tasks %r; Clock window %r seen after %s s, closed after %s s' % (running, wins, seen, gone)


ROWS = [('round_%d' % n, round_row(n)) for n in range(1, ROUNDS + 1)] + [
    ('no_guru', r_no_guru), ('no_muforce_hit', r_no_muforce_hit),
    ('no_busy_wait_sentinel', r_no_sentinel), ('patch_chain_restored', r_chain)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', help='row name, or a comma list of rows')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    a.only = verdicts.parse_only(a.only, [n for n, _ in ROWS])
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / (name + '.verdict')
        if a.only and name not in a.only:
            continue
        if f.exists() and not a.fresh and not a.only:
            old = f.read_text()
            if old.startswith('ok') or not a.failed:
                total += 1
                passed += old.startswith('ok')
                continue
        ok, detail = fn()
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
        if not ok and name.startswith('round_'):
            break             # a failed round stops the series: the evidence is the state it left
    print('amarchy_restart_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
