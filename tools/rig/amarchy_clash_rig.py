#!/usr/bin/env python3
"""amarchy_clash_rig.py -- Amarchy against Intuition and other applications (plan
2026-10-22-amarchy-commodity.md, Phase E, row E1).

With the commodity running:
  swedish_table   the Swedish keymap is active and every Left Amiga row of dist/keymap, pressed with Swedish raw
                  codes, fires its own action (each row is rebound to its own sentinel file, so no action runs:
                  the test is which keys the commodity recognises); the ARexx STATUS hit counter moves once a row.
  screen_m_n      Left Amiga+M sends the front screen to the back, Left Amiga+N brings Workbench to the front:
                  the front screen's name (the UITREE S line) follows, and the commodity's hit counter stays put.
  window_drag     Left Amiga + left mouse drag moves a window as Intuition does (the probe window's box moves).
  thirdparty_key  a Left Amiga chord that is no row reaches a third-party window with IEQUALIFIER_LCOMMAND set
                  (tests/amiga/lcmdprobe logs raw code and qualifier); a chord that IS a row goes to the
                  commodity and not to the window.
  disable         Exchange's Active button off: the row chord now reaches the window, the commodity's hits stay
                  put, M and N still work; Active on again: the row is swallowed again.

Table driven and resumable like amarchy_rig.py: rows run in table order and depend on the state earlier rows leave
(setup starts the commodity and the probe); a verdict is written to <RIG>/amarchy_clash/<row>.verdict as each row
completes and a row with a verdict is skipped next time. One row: --only <row>; the failures again: --failed;
all again: --fresh. Needs the rig up (rig.py start --max) and the handler of this branch installed. Effects are
read from text channels only: UITREE screen and window lines, ARexx STATUS, sentinel files, the probe's log.
"""
import argparse, pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig
from amarchy_rig import arexx, status, port_up

OUT = pathlib.Path(paths.RIG) / 'amarchy_clash'
ROOT = pathlib.Path(__file__).resolve().parents[2]
LAMIGA, LSHIFT, LALT, LCTRL = 0x40, 0x01, 0x10, 0x08       # IEQUALIFIER_*
RAW_LAMIGA = 0x66                                           # the Left Amiga key itself
USERMAP = 'ENV:up-term/keymap.user'
SHIPPED = 'ENV:up-term/keymap'
PROBE_LOG = 'RAM:lcmdprobe.log'
BIN = paths.UPTERM_BIN
NAMES = ('Amarchy', 'amarchy-menu', 'lcmdprobe')

# Raw key codes by key NAME on the Swedish keymap (letters and digits sit at the same place as on the USA map;
# the row names minus and plus are board positions 0x0B and 0x0C (config/keymap.c), whatever the keymap prints there).
RAW = {'1': 0x01, '2': 0x02, '3': 0x03, '4': 0x04, '5': 0x05, '6': 0x06, '7': 0x07, '8': 0x08, '9': 0x09,
       '0': 0x0A, 'minus': 0x0B, 'plus': 0x0C, 'period': 0x39, 'comma': 0x38, 'space': 0x40, 'return': 0x44,
       'backspace': 0x41, 'tab': 0x42, 'esc': 0x45, 'up': 0x4C, 'down': 0x4D, 'right': 0x4E, 'left': 0x4F}
RAW.update({'f%d' % n: 0x4F + n for n in range(1, 11)})
RAW.update({ch: code for ch, code in zip('qwertyuiop', range(0x10, 0x1A))})
RAW.update({ch: code for ch, code in zip('asdfghjkl', range(0x20, 0x29))})
RAW.update({ch: code for ch, code in zip('zxcvbnm', range(0x31, 0x38))})
MODS = {'super': LAMIGA, 'shift': LSHIFT, 'alt': LALT, 'ctrl': LCTRL}


def table():
    """[(chord, qualifier, raw)] of dist/keymap's [bind] rows; chord names a key this script has no raw for -> raw None."""
    rows, sect = [], ''
    for line in (ROOT / 'dist/keymap').read_text().splitlines():
        line = line.split(';', 1)[0].strip()
        if line.startswith('['):
            sect = line
        elif sect == '[bind]' and '=' in line:
            chord = line.split('=', 1)[0].strip()
            parts = chord.split('+')
            if len(parts) > 1 and parts[0] == 'super' and all(p in MODS for p in parts[:-1]):
                qual = 0
                for p in parts[:-1]:
                    qual |= MODS[p]
                keys = [parts[-1]]
                if '..' in keys[0]:                    # super+1..9 is nine rows
                    lo, hi = keys[0].split('..')
                    keys = [str(n) for n in range(int(lo), int(hi) + 1)]
                for k in keys:
                    rows.append(('+'.join(parts[:-1] + [k]), qual, RAW.get(k)))
    return rows


def front():
    """(front screen's name, [(window title, active)]) from the UITREE text."""
    name, wins = None, []
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        if l.startswith('S ') and name is None:
            name = l.split('"')[-2]
        elif l.startswith('W '):
            wins.append((l.split('"')[-2], ' active ' in l))
    return name, wins


def box(title):
    w = ami.window(title)
    return w['box'] if w else None


def probe_log():
    return c.run('Type %s' % PROBE_LOG)[1]


def probe_hits(raw):
    """Number of logged RAWKEY events with raw code `raw` carrying the Left Amiga qualifier."""
    n = 0
    for m in re.finditer(r'raw=([0-9a-f]{2}) qual=([0-9a-f]{4})', probe_log()):
        n += int(m.group(1), 16) == raw and bool(int(m.group(2), 16) & LAMIGA)
    return n


def focus_probe():
    b = box('lcmdprobe')
    if not b:                                # the probe quits after about a minute: start it again
        c.run('Run >NIL: %s/lcmdprobe' % BIN)
        time.sleep(4)
        b = box('lcmdprobe')
    if b:
        ami.click_px(b[0] + 60, b[1] + 6)
        time.sleep(1)
    return b


def exchange_active_click():
    """Open Exchange, select the first listed commodity (Amarchy is the only one the rig runs), click Active, close it."""
    c.run('Run >NIL: SYS:Tools/Commodities/Exchange')
    time.sleep(3)
    ami.click_px(60, 52)
    time.sleep(1)
    ami.click_px(300, 98)
    time.sleep(1.5)
    ami.click_px(8, 20)                     # the close gadget
    time.sleep(1)


def lamiga_drag(x0, y0, x1, y1):
    """Hold the Left Amiga key (a KEY frame down), drag with the left button, release."""
    with ami._conn() as conn:
        ami._keys(conn, [(RAW_LAMIGA, 0)])
    ami.script(('move', x0, y0), ('wait', 3), ('button', 0, 1), ('wait', 3), ('move', (x0 + x1) // 2, (y0 + y1) // 2),
               ('wait', 3), ('move', x1, y1), ('wait', 3), ('button', 0, 0), ('wait', 5))
    with ami._conn() as conn:
        ami._keys(conn, [(RAW_LAMIGA | 0x80, 0)])
    time.sleep(1)


def key_hit_delta(raw, qual):
    h0 = status().get('hits', 0)
    ami.key(raw, qual)
    time.sleep(1.5)
    return status().get('hits', 0) - h0


# ---- rows -----------------------------------------------------------------------------------------------------

def r_setup():
    # the shipped table (the rig kit may not carry it), every row rebound to a sentinel, the commodity, the probe
    c.run('Makedir ENV:up-term')
    ami.put(str(ROOT / 'dist/keymap'), SHIPPED)
    c.run('Delete T:amcl.#? QUIET')
    c.run('Echo >%s "[bind]"' % USERMAP)
    for i, (chord, _, raw) in enumerate(table()):
        c.run('Echo >>%s "%s = run:C:Echo >T:amcl.%d hit"' % (USERMAP, chord, i))
    c.run('Run >NIL: %s/Amarchy' % BIN)
    time.sleep(2)
    s = status()
    c.run('Run >NIL: %s/lcmdprobe' % BIN)
    time.sleep(4)
    ok = s.get('rows', 0) >= len(table()) and 'ready' in probe_log() and s.get('enabled') == 1
    return ok, 'status %r table rows %d probe window %r' % (s, len(table()), box('lcmdprobe'))


def r_swedish_table():
    rc, out = c.run('SetKeyboard s')
    pre = rc == 0
    rows = table()
    skipped = [r[0] for r in rows if r[2] is None]
    todo = [(i, r) for i, r in enumerate(rows) if r[2] is not None]
    c.run('Delete T:amcl.#? QUIET')
    h0 = status().get('hits', 0)
    for i, (chord, qual, raw) in todo:
        ami.key(raw, qual)
        time.sleep(0.6)
    time.sleep(1.5)
    listing = c.run('List T: PAT amcl.#? LFORMAT %N')[1]
    got = set(re.findall(r'amcl\.(\d+)', listing))
    missing = [r[0] for i, r in todo if str(i) not in got]
    hits = status().get('hits', 0) - h0
    return (pre and not missing and hits == len(todo),
            'SetKeyboard s rc %d, %d rows pressed, %d hits, missing %r, no raw code for %r' % (rc, len(todo), hits, missing, skipped))


def r_screen_m_n():
    c.run('Run >NIL: NewShell "XCON:0/20/400/100/own/OWNSCREEN"')
    time.sleep(3)
    own, _ = front()
    h0 = status().get('hits', 0)
    ami.key(RAW['m'], LAMIGA)
    time.sleep(1.5)
    after_m, _ = front()
    ami.key(RAW['n'], LAMIGA)
    time.sleep(1.5)
    after_n, _ = front()
    hits = status().get('hits', 0) - h0
    ok = own == 'UP-Term' and after_m != own and after_n == 'Workbench Screen' and hits == 0
    # leave the rig as found: the own screen's window closes with its shell
    ami.key(RAW['m'], LAMIGA)
    time.sleep(1.5)
    if front()[0] == own:
        ami.click_px(100, 60)
        time.sleep(1)
        ami.req(0x08, bytes([4]) + b'EndCLI')
        ami.key(0x44)
        time.sleep(2)
    return ok, 'own screen %r, after Left Amiga+M %r, after +N %r, commodity hits %+d' % (own, after_m, after_n, hits)


def _drag_delta(dx, dy, title=False):
    """Left Amiga drag of the probe window by (dx, dy) screen pixels; the window's body, or its title bar. Returns the move."""
    focus_probe()
    b0 = box('lcmdprobe')
    if not b0:
        return None
    x, y = b0[0] + b0[2] // 2, (b0[1] + 5 if title else b0[1] + b0[3] // 2)
    sx, sy = ami.pointer_scale()
    lamiga_drag(int(x * sx), int(y * sy), int((x + dx) * sx), int((y + dy) * sy))
    b1 = box('lcmdprobe')
    return None if b1 is None else (b1[0] - b0[0], b1[1] - b0[1])


def r_window_drag():
    """Left Amiga plus a drag must be Intuition's own: the body drag does what it does without Amarchy (control run with
    the commodity quit; this OS may give the qualified body drag no meaning at all), and the title bar still drags."""
    ami.key(RAW['n'], LAMIGA)                # Workbench in front
    time.sleep(1)
    h0 = status().get('hits', 0)
    with_cx = _drag_delta(120, 40)
    title = _drag_delta(-60, 20, title=True)
    hits = status().get('hits', 0) - h0
    arexx('QUIT')
    time.sleep(2)
    control = _drag_delta(120, 40)
    c.run('Run >NIL: %s/Amarchy' % BIN)      # back for the rows after this one
    time.sleep(3)
    return (with_cx is not None and with_cx == control and title not in (None, (0, 0)) and hits == 0,
            'body drag with Amarchy %r, without it %r; title-bar drag with Amarchy %r; commodity hits %+d'
            % (with_cx, control, title, hits))


def r_thirdparty_key():
    c.run('Delete T:amcl.#? QUIET')
    focus_probe()
    focus_probe()
    free = RAW['z']                           # super+z is no row (Workbench Internet prefs, shipped commented out)
    owned = RAW['t']                          # super+t is a row (New tab)
    h0 = status().get('hits', 0)
    ami.key(free, LAMIGA)
    time.sleep(1.5)
    ami.key(owned, LAMIGA)
    time.sleep(1.5)
    hits = status().get('hits', 0) - h0
    passed, swallowed = probe_hits(free), probe_hits(owned)
    return (passed == 1 and swallowed == 0 and hits == 1,
            'free chord reached the window %dx, row chord reached it %dx, commodity hits %+d' % (passed, swallowed, hits))


def r_disable():
    focus_probe()
    exchange_active_click()
    off = status().get('enabled')
    h0 = status().get('hits', 0)
    n0 = probe_hits(RAW['t'])
    focus_probe()
    ami.key(RAW['t'], LAMIGA)
    time.sleep(1.5)
    restored = probe_hits(RAW['t']) - n0 == 1 and status().get('hits', 0) == h0
    ami.key(RAW['m'], LAMIGA)
    time.sleep(1.5)
    ami.key(RAW['n'], LAMIGA)
    time.sleep(1.5)
    mn = front()[0] == 'Workbench Screen'
    exchange_active_click()                   # back on
    on = status().get('enabled')
    n1 = probe_hits(RAW['t'])
    focus_probe()
    ami.key(RAW['t'], LAMIGA)
    time.sleep(1.5)
    swallowed_again = probe_hits(RAW['t']) == n1 and status().get('hits', 0) == h0 + 1
    return (off == 0 and restored and mn and on == 1 and swallowed_again,
            'Disable: enabled=%r row chord reaches window %s, M/N fine %s; Enable: enabled=%r row swallowed again %s'
            % (off, restored, mn, on, swallowed_again))


def r_cleanup():
    arexx('QUIT')
    time.sleep(1.5)
    c.run('Delete %s QUIET' % USERMAP)
    c.run('Delete T:amcl.#? QUIET')
    b = box('lcmdprobe')
    if b:
        ami.click_px(8, b[1] + 6)             # the probe's close gadget
        time.sleep(1)
    return not port_up(), 'commodity gone, user keymap removed, probe window closed %s' % (box('lcmdprobe') is None)


ROWS = [
    ('setup',          r_setup),
    ('swedish_table',  r_swedish_table),
    ('screen_m_n',     r_screen_m_n),
    ('window_drag',    r_window_drag),
    ('thirdparty_key', r_thirdparty_key),
    ('disable',        r_disable),
    ('cleanup',        r_cleanup),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true', help='stage what build/amiga holds, do not run make')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    # the rows start VTC:Apps/bin/Amarchy and lcmdprobe: those files must be this build
    stage_rig.report(stage_rig.stage_build(NAMES, build=not a.no_build))
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
    print('amarchy_clash_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
