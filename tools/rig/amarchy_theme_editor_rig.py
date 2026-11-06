#!/usr/bin/env python3
"""Rig case for the Amarchy theme editor, plan row TE-9.1 (reachability): amarchy-gui > Settings > Theme Editor >
duplicate tokyo-night as te-test, set a swatch to #ff8800, Save, then assert the saved file, both prefs files, the
gt_dos_edit_applies sentinel and the measured title bar pixels. One [OK]/[FAIL] line per assert, each verdict appended to
the log as it completes; exit 1 on any FAIL.  Run: UPTERM_RIG=<n> python3 tools/rig/amarchy_theme_editor_rig.py --case te91"""
import argparse, collections, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import condev_rig as c
import amarchy_gui_rig as g

CASES = {}
FAILS = []
LOG = os.environ.get('TE_LOG', '')
FILE = 'ENVARC:Amarchy/themes/te-test.theme'
PREFS = ('ENV:Amarchy/prefs', 'ENVARC:Amarchy/prefs')
SENTINEL = 'gt_dos_edit_applies'


def case(name):
    def deco(fn):
        CASES[name] = fn
        return fn
    return deco


def say(ok, text):
    line = '[%s] %s' % ('OK' if ok else 'FAIL', text)
    print(line, flush=True)
    if LOG:
        with open(LOG, 'a') as f:
            f.write(line + '\n')
    if not ok:
        FAILS.append(text)
    return ok


def getenv(name):
    rc, out = c.run('GetEnv ' + name)
    return out.strip() if rc == 0 else None


def type_text(text):
    with ami._conn() as conn:
        ami._type(conn, text.encode('latin-1'))


def level():
    rows = g.rows_now()
    print('  rows: %s' % rows, flush=True)
    return rows


def walk(label):
    rows = g.rows_now()
    ok = g.walk_to(label, rows)
    print('  walk %-22s %s (rows were %s)' % (label, ok, rows), flush=True)
    return ok


def prompt_set(text, back=40):
    """Return on the focused row opened a prompt: wipe the prefilled text, type `text`, Return (0x44)."""
    for _ in range(back):
        ami.key(0x41)
    type_text(text)
    ami.key(g.RETURN)
    time.sleep(2)


def title_bar_colour(shot):
    """Modal RGB of the active Amarchy window's title strip, measured from a screen grab: (rgb, box, count)."""
    s, w = g.where(g.TITLE)
    left, top, ww, hh = g.box_of(s, w)
    _, _, rows = ami.grab((left, top, ww, 12))
    cnt = collections.Counter()
    for r in rows:
        for x in range(0, len(r) - 2, 3):
            cnt[tuple(r[x:x + 3])] += 1
    rgb, n = cnt.most_common(1)[0]
    return rgb, (left, top, ww, 12), n


@case("te91")
def case_te91(args):
    before = getenv(SENTINEL)
    c.run('Delete >NIL: ' + FILE + ' QUIET')
    g.close_menu()
    s, w = g.open_menu()
    time.sleep(3)
    if not walk('Settings') or not (time.sleep(1) or level()) or not walk('Theme Editor'):
        return say(False, 'step: menu path Settings > Theme Editor not reachable')
    rows = level()
    if rows and rows[0].startswith('Mode: Simple'):
        g.walk_to('Mode', rows)
        rows = level()
    ok = walk('Presets and Files')
    rows = level()
    ok = ok and walk('Duplicate Theme')
    time.sleep(2)
    print('  prompt screen text: %s' % ami.screen_text(), flush=True)
    prompt_set('te-test')
    print('  after duplicate rows: %s' % level(), flush=True)
    print('  file present: %s' % (ami.read_file(FILE) is not None), flush=True)
    return ok


def finish(args):
    # asserts that need the saved file
    data = ami.read_file(FILE)
    text = data.decode('latin-1') if data else ''
    say('sky = #ff8800' in text.replace('"', '') and 'amarchy-theme = 2' in text,
        '1: %s holds sky = #ff8800 and amarchy-theme = 2 (file %s, %d bytes)' % (FILE, 'present' if data else 'MISSING', len(text)))
    for p in PREFS:
        d = ami.read_file(p)
        t = d.decode('latin-1') if d else ''
        say('te-test' in t, '2: %s names te-test (%s)' % (p, t.strip().replace('\n', ' ; ')[:120] if d else 'MISSING'))
    after = getenv(SENTINEL)
    say(False, '3: sentinel %s went up by 1 (before=%s after=%s; the counter is a C global in gthemes_core.c:1479, never published as an AmigaDOS variable)' % (SENTINEL, args.before, after))
    rgb, box, n = title_bar_colour(None)
    say(rgb == (0xff, 0x88, 0x00), '4: active title bar modal pixel #%02x%02x%02x (%d px of window strip %s), want #ff8800' % (rgb + (n, box)) if False else
        '4: active title bar modal pixel #%02x%02x%02x (%d px in strip %s), want #ff8800' % (rgb[0], rgb[1], rgb[2], n, box))


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('--case', default='te91')
    ap.add_argument('--rig', type=int, default=0)
    args = ap.parse_args(argv)
    if args.rig:
        os.environ['UPTERM_RIG'] = str(args.rig)
    args.before = getenv(SENTINEL)
    CASES[args.case](args)
    finish(args)
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
