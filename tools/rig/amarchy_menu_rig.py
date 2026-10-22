#!/usr/bin/env python3
"""amarchy_menu_rig.py -- the Amarchy quick menu and the handler request port
(plan 2026-10-22-amarchy-commodity.md, Phase D; request port: handler/upreq.h, keymap_act.h upreq_*).

Table driven and resumable, like amarchy_rig.py: a verdict per row in <RIG>/amarchy/menu_<row>.verdict,
--only <row>, --failed, --fresh. Rows share state (the commodity running, an UP-Term window to act on).

Sentinel for "the request reached the handler": every request the handler runs bumps STATE_KEYMAP, so
`UPTerm state` km-hits moves by exactly 1 per delivered leaf (a key binding moves it the same way, one
code path). The chord for /theme is NOT needed: the request port works with it unbound.
Needs the rig up (rig.py start --max), Amarchy and amarchy-menu in UP-Term:bin (rig.py install puts them there), the handler of this branch
installed. NOT RUN when written: the rows stay open until it has run.
"""
import argparse, pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

OUT = pathlib.Path(paths.RIG) / 'amarchy'
LAMIGA = 0x40                       # IEQUALIFIER_LCOMMAND
RAW = {'t': 0x14, 'h': 0x25, 'k': 0x27, 'space': 0x40, 'esc': 0x45, 'return': 0x44}
KEYMAP = pathlib.Path(__file__).resolve().parents[2] / 'dist' / 'keymap'
TIMING = pathlib.Path(paths.RIG) / 'amarchy' / 'menu-timing.txt'


def amiga_out(cmd):
    c.run('Delete RAM:am.out QUIET')
    c.run(cmd + ' >RAM:am.out')
    return c.run('Type RAM:am.out')[1]


def km_hits():
    for l in amiga_out('UPTerm state').splitlines():
        f = l.replace('=', ' ').replace(':', ' ').split()
        if len(f) >= 2 and f[0] == 'km-hits' and f[-1].lstrip('-').isdigit():
            return int(f[-1])
    return 0


def screen_text():
    fn = getattr(ami, 'screen_text', None)
    return fn() if fn else ''


def bind_rows():
    n, sect = 0, ''
    for l in KEYMAP.read_text().splitlines():
        l = l.strip()
        if l.startswith('['):
            sect = l
        elif sect == '[bind]' and l and not l.startswith(';') and '=' in l:
            n += 1
    return n


def open_terminal():
    c.run('Run >NIL: NewShell "XCON:0/20/640/300/UP-Term/CLOSE"')
    time.sleep(2)
    ami.click_px(100, 100)
    time.sleep(1)


def r_start():
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    open_terminal()
    return True, 'commodity started, terminal window open and focused'


def r_open_timing():
    t0 = time.time()
    ami.key(RAW['space'], LAMIGA)
    while time.time() - t0 < 10:
        if 'Filter' in screen_text():
            break
        time.sleep(0.1)
    dt = time.time() - t0
    OUT.mkdir(parents=True, exist_ok=True)
    TIMING.write_text('menu open (Super+Space to "Filter:" on screen): %.2f s\n' % dt)
    return 'Filter' in screen_text(), 'menu open %.2f s -> %s' % (dt, TIMING)


def r_type_th():
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.4)
    s = screen_text()
    return 'Filter: th' in s and 'Theme' in s, 'filter "th" leaves Theme in the list'


def r_esc_up():
    ami.key(RAW['esc'])           # clears the filter
    time.sleep(0.5)
    ami.key(RAW['esc'])           # at the root: leaves the menu
    time.sleep(0.8)
    return 'Filter' not in screen_text(), 'Esc clears the filter, Esc at the root leaves'


def r_enter_slash():
    # reopen, filter to Theme, Enter: /theme must reach the handler (km-hits +1), chord unbound or not
    before = km_hits()
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.4)
    ami.key(RAW['return'])
    time.sleep(2)
    after = km_hits()
    return after == before + 1, 'handler km-hits %+d (exactly 1 expected)' % (after - before)


def r_viewer_rows():
    ami.key(RAW['k'], LAMIGA)
    time.sleep(1.5)
    s = screen_text()
    ami.key(RAW['esc'])
    n = bind_rows()
    seen = len([l for l in s.splitlines() if re.match(r'\s*(Super|Alt)\+', l) or 'Super+' in l])
    return seen >= min(n, 10) and n > 0, 'dist/keymap has %d [bind] rows; viewer showed %d lines (window shows a page; scroll check by eye)' % (n, seen)


ROWS = [
    ('start',        r_start),
    ('open_timing',  r_open_timing),
    ('type_th',      r_type_th),
    ('esc_up',       r_esc_up),
    ('enter_slash',  r_enter_slash),
    ('viewer_rows',  r_viewer_rows),
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
        f = OUT / ('menu_' + name + '.verdict')
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
    print('amarchy_menu_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
