#!/usr/bin/env python3
"""amarchy_rig.py -- the Amarchy commodity (plan 2026-10-22-amarchy-commodity.md, Phase C, rows C6 and C7).

Table driven and resumable. Rows run in table order; later rows depend on the state earlier ones leave
(the commodity running, the sentinel keymap row loaded), so --only runs one row against whatever state
the rig is in. A verdict is written to <RIG>/amarchy/<row>.verdict as each row completes; a row with a
verdict is skipped on the next run. One row: --only <row>; the failures again: --failed; all: --fresh.

Sentinel: the user keymap gets the row  super+alt+j = run:C:Echo >T:amarchy.sentinel hit  so the key
creates T:amarchy.sentinel. The commodity's ARexx STATUS hit counter and `UPTerm state` km-hits are the
two counters that prove WHICH side ran the action (R3: exactly one of them moves).
Needs the rig up (rig.py start --max), Amarchy in UP-Term:bin (rig.py install puts it there) and the handler of this branch installed.
NOT RUN when written (rig 4 busy, rig 1 never): C6 and C7 stay open until it has run.
"""
import argparse, pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

OUT = pathlib.Path(paths.RIG) / 'amarchy'
LAMIGA, LALT = 0x40, 0x10          # IEQUALIFIER_LCOMMAND, IEQUALIFIER_LALT
RAW_J = 0x26
SENT = 'T:amarchy.sentinel'
USERMAP = 'ENV:up-term/keymap.user'
ROW_LINE = 'super+alt+j = run:C:Echo >%s hit' % SENT


def amiga_out(cmd):
    c.run('Delete RAM:am.out QUIET')
    c.run(cmd + ' >RAM:am.out')
    return c.run('Type RAM:am.out')[1]


def arexx(cmd):
    # The rig has no RexxMaster (no RX): send the command to the AMARCHY port through wasabi's ARexx call
    m = ami._m()
    try:
        with ami._conn() as conn:
            return m.arexx(conn, 'AMARCHY', cmd)[2].strip()
    except m.WasabiError:
        return ''             # no such port: the commodity is not running


def status():
    d = {}
    for w in arexx('STATUS').replace('\n', ' ').split():
        if '=' in w:
            k, v = w.split('=', 1)
            if v.lstrip('-').isdigit():
                d[k] = int(v)
    return d


def km_hits():
    d = {}
    for l in amiga_out('UPTerm state').splitlines():
        f = l.replace('=', ' ').replace(':', ' ').split()
        if len(f) >= 2 and f[-1].lstrip('-').isdigit():
            d[f[0]] = int(f[-1])
    return d.get('km-hits', 0)


def port_up():
    return arexx('STATUS') != ''


def start():
    c.run('Run >NIL: UP-Term:bin/Amarchy')
    time.sleep(2)


def chord():
    c.run('Delete %s QUIET' % SENT)
    ami.key(RAW_J, LAMIGA | LALT)
    time.sleep(1.5)
    out = c.run('List %s' % SENT)[1]
    return 'amarchy.sentinel' in out and 'not found' not in out.lower()


def r_start():
    # keymap.user is parsed by section like the keymap: the row needs its [bind] header
    c.run('Echo >%s "[bind]"' % USERMAP)
    c.run('Echo >>%s "%s"' % (USERMAP, ROW_LINE))
    start()
    s = status()
    return 'rows' in s and s['rows'] > 0, 'status %r' % s


def r_exchange():
    ok = port_up()
    return ok, 'ARexx port AMARCHY answers: %s (Exchange listing is a screen check, see R-visual)' % ok


def r_reload():
    arexx('RELOAD')
    return status().get('rows', 0) > 0, 'rows %r' % status().get('rows')


def r_r1_hit():
    # R1: no UPTerm window active (the Workbench window has focus), the chord reaches the commodity
    before = status().get('hits', 0)
    ok = chord()
    after = status().get('hits', 0)
    return ok and after == before + 1, 'sentinel %s hits %+d' % (ok, after - before)


def r_r3_terminal():
    # R3: UPTerm window active, same chord: exactly one action runs, and it is the handler's
    c.run('Run >NIL: NewShell "XCON:0/20/640/300/UP-Term/CLOSE"')
    time.sleep(2)
    ami.click_px(100, 100)
    time.sleep(1)
    h0, k0 = status().get('hits', 0), km_hits()
    ok = chord()
    h1, k1 = status().get('hits', 0), km_hits()
    one = (h1 - h0) + (k1 - k0) == 1
    return ok and one, 'sentinel %s commodity hits %+d km-hits %+d' % (ok, h1 - h0, k1 - k0)


def r_second_start():
    start()
    s = status()
    return s.get('rows', 0) > 0, 'second start refused; first instance still answers %r' % s


def r_quit():
    arexx('QUIT')
    time.sleep(1.5)
    return not port_up(), 'port gone after QUIT'


# row: (name, function)
ROWS = [
    ('start',         r_start),
    ('port_answers',  r_exchange),
    ('reload',        r_reload),
    ('r1_commodity',  r_r1_hit),
    ('r3_terminal',   r_r3_terminal),
    ('second_start',  r_second_start),
    ('quit',          r_quit),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / (name + '.verdict')
        if a.only and a.only != name:
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
    print('amarchy_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
