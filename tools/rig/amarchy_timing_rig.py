#!/usr/bin/env python3
"""amarchy_timing_rig.py -- Amarchy quick-menu open latency, cold and warm (plan 2026-10-22-amarchy-commodity.md,
Phase D1: the menu needs a window in under 0.3 s).

No screen reads: with `SetEnv AMARCHY_TIMING 1` the commodity and amarchy-menu append marks to RAM:amarchy.timing
(amarchy/atiming.h, off by default; the clock is DateStamp, 20 ms ticks). Per press the marks are
hotkey (the commodity got Left Amiga + Space), start (the helper's main), terminal (its XCON: window is up), ready
(the dialog is prepared, just before the first draw). open latency = ready - hotkey. The file is polled with a
cheap `Type` over wasabi.
  cold = `Avail FLUSH` before the press (libraries and fonts gone from memory)
  warm = the press after a previous one, nothing flushed
Needs the rig up (rig.py start --max). Medians go to <RIG>/amarchy/menu-timing.txt.
Warm server checks (plan 2026-10-27-amarchy-warm-menu-server.md, section 5): the "send" mark (commodity put the
window spec on AMARCHY.MENUSRV: the warm path ran, the fallback has none), marks in order hotkey, send, start,
terminal, ready; the port owner (AMARCHY.MENUSRV) is the same task at every open; hotkey to ready under --max-ms;
the Amarchy window gone after each open; a stale port (owner removed, port left) is taken over by the next open, which falls back cold and restarts the server;
commodity quit removes the port.
Usage: UPTERM_RIG=<n> python3 tools/rig/amarchy_timing_rig.py [--runs 3] [--no-build] [--max-ms 500]"""
import argparse, pathlib, re, statistics, subprocess, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig, tabstate

LAMIGA, SPACE, ESC = 0x40, 0x40, 0x45
KEY_K, KEY_C, CTRL = 0x27, 0x33, 0x08   # raw codes: K is 0x27 (0x20 is A); dist/keymap: super+k = viewer
PRESS = {}
RESULTS = []


def verdict(ok, what, seen=''):
    RESULTS.append(ok)
    print('%s %s %s' % ('PASS' if ok else 'FAIL', what, seen), flush=True)
FILE = 'RAM:amarchy.timing'
OUT = pathlib.Path(paths.RIG) / 'amarchy' / 'menu-timing.txt'


ORDER = ('hotkey', 'send', 'start', 'terminal', 'ready')


def wasabi(*args):
    """The wasabi client against this rig (ps, kill, windows)."""
    import paths as P
    r = subprocess.run([str(P.VTCON / 'build/wasabi/wasabi'), '--host', '127.0.0.1', '--port', str(P.WASABI_PORT),
                        '--key', P.WASABI_KEY or '', *args], capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr)


def server_tasks():
    """Addresses of the amarchy-menu processes (wasabi ps): the warm server and any cold menu."""
    return sorted(l.split()[0] for l in wasabi('ps')[1].splitlines() if 'amarchy-menu' in l)


def port_owner():
    """The task that owned AMARCHY.MENUSRV at the last open (the commodity notes mp_SigTask in ENV:AMARCHY_PORT_OWNER
    when AMARCHY_TIMING is set; a stale port keeps its dead owner). '0x...' like wasabi ps, or None for no port."""
    rc, out = c.run('GetEnv AMARCHY_PORT_OWNER')
    m = re.search(r'\b([0-9a-f]{8})\b', out)
    return None if not m or int(m.group(1), 16) == 0 else '0x' + m.group(1)


def task_up(addr):
    return any(l.split()[0] == addr for l in wasabi('ps')[1].splitlines()[1:] if l.strip())


def port_listed():
    return 'AMARCHY.MENUSRV' in wasabi('arexx', '--ports')[1]


def window_up():
    """An XCON window other than the host (the menu window has no title)."""
    return any('task XCON' in l and 'timinghost' not in l for l in wasabi('windows')[1].splitlines())


def marks():
    rc, out = c.run('Type %s' % FILE)
    res = []
    for l in out.splitlines():
        p = l.split()
        if len(p) == 2 and p[1].isdigit():
            res.append((p[0], int(p[1])))
    return res


def press(flush, key=SPACE, leave=lambda: ami.key(ESC)):
    c.run('Delete >NIL: %s QUIET' % FILE)
    if flush:
        c.run('Avail >NIL: FLUSH')
        time.sleep(1)
    ami.key(key, LAMIGA)
    t0 = time.time()
    got = []
    while time.time() - t0 < 15:
        got = marks()
        if any(t == 'ready' for t, _ in got):
            break
        time.sleep(0.1)
    seen = window_up()
    tasks = server_tasks()
    owner = port_owner()
    leave()                 # at the root Esc leaves the menu
    time.sleep(2.5)
    PRESS.update(order=[t for t, _ in sorted(got, key=lambda x: (x[1], ORDER.index(x[0]) if x[0] in ORDER else 9))],
                 window=seen, closed=not window_up(), tasks=tasks, owner=owner,
                 box=None)
    d = dict(got)
    if 'hotkey' not in d or 'ready' not in d:
        return None, got
    return {t: (v - d['hotkey']) * 20 for t, v in d.items()}, got   # ticks are 1/50 s = 20 ms


def kill_server():
    """RemTask (kill --force) of every amarchy-menu process: no cleanup, the port stays (a stale port)."""
    out = [wasabi('kill', '--force', t)[1].strip() for t in server_tasks()]
    return 0, ' '.join(out)


def warm_checks(a):
    """After the cold/warm medians. The server is the port owner (not a count of amarchy-menu tasks: a cold menu
    is one too). Every open below must be a warm one served by that owner."""
    kill_server()           # strays of earlier runs (a killed commodity leaves its server): start from none
    time.sleep(1)
    press(False)            # stale port or none: the open falls back cold and starts the one server
    time.sleep(3)
    press(False)            # the first warm open reads the new owner
    base = PRESS['owner']
    verdict(base is not None and task_up(base), 'the port owner is a live task before the opens', str(base))
    for i in range(3):
        r, got = press(False)
        ms = r['ready'] if r else None
        verdict(PRESS['order'] == list(ORDER), 'open %d marks in order %s' % (i + 1, '/'.join(ORDER)), str(PRESS['order']))
        verdict(r is not None and ms <= a.max_ms, 'open %d hotkey to ready <= %d ms' % (i + 1, a.max_ms), '%s ms' % ms)
        verdict(PRESS['owner'] == base, 'open %d served by the same port owner' % (i + 1), str(PRESS['owner']))
        verdict(PRESS['window'] and PRESS['closed'], 'open %d window up, then gone after Escape' % (i + 1))
    r, got = press(False, KEY_K)      # the key viewer: another size
    verdict(PRESS['window'] and PRESS['closed'] and PRESS['owner'] == base and 'send' in PRESS['order'],
            'viewer open (second size) works', '%s ms marks %s' % (r and r['ready'], PRESS['order']))
    r, got = press(False, SPACE, lambda: ami.key(KEY_C, CTRL))
    verdict(PRESS['closed'] and task_up(base), 'Ctrl-C closes the menu, the port owner stays alive', str(base))
    r, got = press(False)
    verdict(PRESS['order'] == list(ORDER) and PRESS['owner'] == base, 'open after Ctrl-C is warm again', str(PRESS['order']))
    # a real stale port: the owner task is gone (RemTask), AMARCHY.MENUSRV is still on the list
    c.run('Delete >NIL: %s QUIET' % FILE)
    wasabi('kill', '--force', base)
    time.sleep(1)
    verdict(not task_up(base) and port_listed(), 'owner removed, port left behind (stale)', 'owner %s' % base)
    r, got = press(False)
    verdict(PRESS['owner'] == base and PRESS['window'] and PRESS['closed'] and 'send' not in PRESS['order']
            and 'ready' in PRESS['order'], 'stale port: the open falls back cold (menu opens, no send mark)',
            '%s owner %s' % (PRESS['order'], PRESS['owner']))
    time.sleep(3)
    r, got = press(False)
    new = PRESS['owner']
    verdict(new not in (None, base) and task_up(new) and PRESS['order'] == list(ORDER),
            'next open is warm on a restarted server (new port owner)', '%s owner %s' % (PRESS['order'], new))
    # commodity quit: MS_QUIT, the port disappears and the server ends
    wasabi('arexx', 'AMARCHY', 'QUIT')
    time.sleep(3)
    verdict(not port_listed() and not task_up(new), 'commodity quit: AMARCHY.MENUSRV gone, server task ended',
            'owner %s port listed %s' % (new, port_listed()))
    # no server, no commodity port: a fresh commodity starts one again (cold fallback is the stale-port step above)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    time.sleep(4)
    press(False)
    time.sleep(3)
    r, got = press(False)
    verdict(PRESS['owner'] is not None and task_up(PRESS['owner']) and PRESS['order'] == list(ORDER),
            'after the quit a restarted commodity serves warm again', '%s owner %s' % (PRESS['order'], PRESS['owner']))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runs', type=int, default=3)
    ap.add_argument('--no-build', action='store_true')
    ap.add_argument('--max-ms', type=int, default=500, help='hotkey to ready, warm opens')
    a = ap.parse_args()
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    wasabi('arexx', 'AMARCHY', 'QUIT')     # an Amarchy of the rig's boot would keep the old code and refuse a second one
    time.sleep(3)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    tabstate.open_host('timinghost')
    c.run('SetEnv AMARCHY_TIMING 1')
    time.sleep(1)
    press(False)            # not counted: the first press of a fresh process pair
    lines, res = [], {}
    for kind, flush in (('cold', True), ('warm', False)):
        vals = []
        for i in range(a.runs):
            r, got = press(flush)
            print('%s %d: %s' % (kind, i + 1, r or ('NO MARKS ' + str(got))), flush=True)
            if r:
                vals.append(r)
        res[kind] = vals
        for tag in ('start', 'terminal', 'ready'):
            xs = [v[tag] for v in vals if tag in v]
            if xs:
                lines.append('%s %s median %d ms (runs %s)' % (kind, tag, statistics.median(xs), xs))
    warm_checks(a)
    c.run('SetEnv AMARCHY_TIMING 0')
    c.run('Delete >NIL: ENV:AMARCHY_TIMING QUIET')
    c.run('Delete >NIL: %s QUIET' % FILE)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    return 0 if all(len(v) == a.runs for v in res.values()) and all(RESULTS) else 1


if __name__ == '__main__':
    sys.exit(main())
