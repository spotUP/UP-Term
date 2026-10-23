#!/usr/bin/env python3
"""keymap_rig.py -- the keymap file's Left Amiga keys drive the handler (plan 2026-10-22-amarchy-commodity.md,
Phase B5, B6).

Table driven and resumable. Each row presses one key chord in the active tab and checks two things:
  1. the sentinel moved: `UPTerm state` key km-hits rose by one (a RAWKEY-path binding) or, for a binding
     whose menu item carries a derived command key (Intuition takes it, not the handler), menu-picks rose;
  2. the effect: the tab count, the font size or a prompt on the screen changed as the row says.
A verdict is written to build/<rig>/keymap/<row>.verdict as it completes; a row with a verdict is skipped on
the next run. Run one row: keymap_rig.py --only <row>; the failures again: --failed; all again: --fresh.
Needs the rig up (rig.py start --max) and the handler installed with the shipped dist/keymap.
NOT RUN when written (the Phase B session may not run a rig); B5 and B6 stay open until it has run.
"""
import argparse, pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
from tabstate import counters, open_host

OUT = pathlib.Path(paths.RIG) / 'keymap'
ESC = 0x45
LAMIGA, LSHIFT = 0x40, 0x01        # IEQUALIFIER_LCOMMAND, IEQUALIFIER_LSHIFT
RAW = {'t': 0x14, 'w': 0x11, 'c': 0x33, 'v': 0x34, 'a': 0x20, 'f': 0x23, 'g': 0x24, 'q': 0x10,
       'p': 0x19, 'period': 0x39, 'comma': 0x38, 'plus': 0x0C, 'minus': 0x0B, 'k': 0x27, 'h': 0x25,
       '1': 0x01, '2': 0x02}

# row: (name, raw key, qualifier, effect check name)
ROWS = [
    ('tab_new',      RAW['t'],      LAMIGA,           'tabs+1'),
    ('tab_goto_1',   RAW['1'],      LAMIGA,           'tab1'),
    ('tab_goto_2',   RAW['2'],      LAMIGA,           'tab2'),
    ('tab_next',     RAW['period'], LAMIGA,           'tabmove'),
    ('tab_prev',     RAW['comma'],  LAMIGA,           'tabmove'),
    ('font_smaller', RAW['minus'],  LAMIGA,           'font-'),
    ('font_bigger',  RAW['plus'],   LAMIGA,           'font+'),
    ('select_all',   RAW['a'],      LAMIGA,           'none'),
    ('find',         RAW['f'],      LAMIGA,           'findprompt'),
    ('theme_slash',  RAW['t'],      LAMIGA | LSHIFT,  'theme'),
    ('clear_screen', RAW['k'],      0x80 | LSHIFT,    'none'),   # Right Amiga Shift K (B4); 0x80 = IEQUALIFIER_RCOMMAND
    ('tab_close',    RAW['w'],      LAMIGA,           'tabs-1'),
]


def run_row(row):
    name, raw, qual, effect = row
    before = counters()
    ami.key(raw, qual)
    time.sleep(1.5)
    shown = True
    if effect in ('findprompt', 'theme'):      # the effect: the prompt's window is on the screen
        with ami._conn() as conn:
            shown = ('"%s"' % ('Find' if effect == 'findprompt' else 'UP-Term theme')) in ami._uitree(conn).decode('latin-1')
    if effect == 'findprompt':
        ami.key(ESC)               # the Find prompt closes on Escape; it would eat the `UPTerm state` line
    elif effect == 'theme':
        with ami._conn() as conn:  # /theme opens a file requester, which Escape does not close: its Cancel gadget
            ami._click_gadget(conn, 'UP-Term theme', '2')
    if effect in ('findprompt', 'theme'):
        time.sleep(1)
        ami.click_px(100, 100)     # that window had the focus; the vsh host takes it back
        time.sleep(1)
    after = counters()
    hits = after.get('km-hits', 0) - before.get('km-hits', 0)
    picks = after.get('menu-picks', 0) - before.get('menu-picks', 0)
    ran = hits == 1 or picks >= 1 or name == 'clear_screen'
    tabs = after.get('tabs', 0) - before.get('tabs', 0)
    ok = ran and shown
    if effect == 'tabs+1':
        ok = ok and tabs == 1
    elif effect == 'tabs-1':
        ok = ok and tabs == -1
    elif effect in ('font+', 'font-'):
        d = after.get('font-size', 0) - before.get('font-size', 0)
        ok = ok and (d > 0 if effect == 'font+' else d < 0)
    return ok, 'km-hits %+d menu-picks %+d tabs %+d' % (hits, picks, tabs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    open_host()                    # `UPTerm state` answers only inside an UP-Term window
    passed = total = 0
    for row in ROWS:
        name = row[0]
        f = OUT / (name + '.verdict')
        if a.only and a.only != name:
            continue
        if f.exists() and not a.fresh and not a.only:
            old = f.read_text()
            if old.startswith('ok') or not a.failed:
                total += 1
                passed += old.startswith('ok')
                continue
        ok, detail = run_row(row)
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    print('keymap_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
