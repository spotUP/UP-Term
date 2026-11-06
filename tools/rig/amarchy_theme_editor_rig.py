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
SENTINEL = 'AMARCHY_EDIT_APPLIES'
SWATCH = 'background'
PRESETS_INDEX = 16
EDIT_INDEX = 4
SAVE_INDEX = 5
APPLY_INDEX = 8
SWATCHES_INDEX = 9


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


def prompt_set(text, back=40, raw=False):
    """Return on the focused row opened a prompt: wipe the prefilled text, type `text`, Return (0x44)."""
    for _ in range(back):
        ami.key(0x41)
    if raw:                                 # ami._type drops '-' at this prompt: raw Amiga US key codes
        for ch in text:
            ami.key(0x0B if ch == '-' else g.RAW_KEYS[ch])
            time.sleep(.3)
    else:
        type_text(text)
    ami.key(g.RETURN)
    time.sleep(2)


def title_bar_colour(shot):
    """Modal RGB of the active Amarchy window's title strip, measured from a screen grab: (rgb, box, count)."""
    s, w = g.where(g.TITLE)
    if not w:
        return (0, 0, 0), None, 0
    left, top, ww, hh = g.box_of(s, w)
    _, _, rows = ami.grab((left, top, ww, 12))
    cnt = collections.Counter()
    for r in rows:
        for x in range(0, len(r) - 2, 3):
            cnt[tuple(r[x:x + 3])] += 1
    rgb, n = cnt.most_common(1)[0]
    return rgb, (left, top, ww, 12), n


def back():
    """One level up (Escape) and wait for the row sentinel to be republished."""
    ami.key(g.ESC)
    time.sleep(1.5)


def shot(name):
    """A screenshot to the scratch dir and ~/Desktop/upterm-shots (pixel checks read the grab, not the file)."""
    try:
        w, h, rows = ami.grab()
        path = os.path.join(os.environ.get('TE_SHOTS', '.'), name + '.png')
        ami.png(path, w, h, rows)
        d = os.path.expanduser('~/Desktop/upterm-shots')
        os.makedirs(d, exist_ok=True)
        import shutil
        shutil.copy(path, d)
    except Exception as e:
        print('  shot %s failed: %s' % (name, e), flush=True)


def walk_path(*labels):
    for lab in labels:
        if not walk(lab):
            say(False, 'step: row "%s" not reachable' % lab)
            return False
        time.sleep(1)
    return True


def enter_te():
    """Menu closed and reopened at Settings > Theme Editor, Advanced mode; True when the level is shown."""
    for _ in range(3):
        g.close_menu()
        time.sleep(1)
        g.open_menu()
        time.sleep(3)
        if not g.rows_now(8):
            continue
        if walk('Settings') and (time.sleep(1) or level()) and walk('Theme Editor'):
            time.sleep(1)
            rows = level()
            if rows and rows[0].startswith('Mode: Simple'):
                g.walk_to('Mode', rows)
                time.sleep(2)
                level()
            return True
    return say(False, 'step: menu path Settings > Theme Editor not reachable')


def goto(index, what):
    """Return on child `index` of the Theme Editor level. ENV:AMARCHY_ROWS is capped at 256 bytes (gui.c sentinel_rows),
    so the late rows (Presets and Files and the rest) cannot be found by label: Up to the top, `index` Downs, checked
    against AMARCHY_SEL. Child order (dist/keymap settings.theme.*): Mode, Base Theme, Contrast, Pen Budget,
    Edit Theme, Save, Save As, Revert, Apply to Desktop, Swatches, Groups, Elements, Type, Shape, States, Desktop,
    Presets and Files, Screens."""
    for _ in range(25):
        ami.key(g.UP)
        time.sleep(.15)
    for _ in range(index):
        ami.key(g.DOWN)
        time.sleep(.3)
    sel = getenv('AMARCHY_SEL')
    print('  goto %s: child %d, AMARCHY_SEL=%s' % (what, index, sel), flush=True)
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    ami.key(g.RETURN)
    time.sleep(2)


@case("te91")
def case_te91(args):
    c.run('Delete >NIL: ' + FILE + ' QUIET')
    if not enter_te():
        return False
    goto(EDIT_INDEX, 'Edit Theme...')           # Duplicate and Save are skipped (GTP_SKIP) until an editor is open
    prompt_set('tokyo-night', raw=True)
    if not enter_te():
        return False
    goto(PRESETS_INDEX, 'Presets and Files')
    rows = level()
    if not any(r.startswith('Duplicate Theme') for r in rows):
        return say(False, 'step: Presets and Files (child %d) did not open the file actions: %s' % (PRESETS_INDEX, rows))
    if not walk('Duplicate Theme'):
        return say(False, 'step: Duplicate Theme... not reachable')
    time.sleep(2)
    prompt_set('te-test', raw=True)
    print('  copy present: %s' % (ami.read_file(FILE) is not None), flush=True)
    if not enter_te():
        return False
    goto(EDIT_INDEX, 'Edit Theme... te-test')
    prompt_set('te-test', raw=True)
    if not enter_te():
        return False
    goto(APPLY_INDEX, 'Apply to Desktop')
    time.sleep(4)
    args.before = getenv('AMARCHY_EDIT_APPLIES')
    print('  sentinel before: %s' % args.before, flush=True)
    if not enter_te():
        return False
    goto(SWATCHES_INDEX, 'Swatches')
    time.sleep(1)
    rows = level()
    if not g.walk_to(SWATCH, rows):
        return say(False, 'step: swatch row "%s" not reachable' % SWATCH)
    time.sleep(1)
    rows = level()
    if not g.walk_to('Custom', rows):
        return say(False, 'step: Custom... not reachable')
    time.sleep(2)
    prompt_set('ff8800')
    time.sleep(2)
    level()
    if not enter_te():
        return False
    goto(SAVE_INDEX, 'Save')
    time.sleep(5)
    shot('te91-after-save')
    return True


def finish(args):
    # asserts that need the saved file
    data = ami.read_file(FILE)
    text = data.decode('latin-1') if data else ''
    say('background = #ff8800' in text.replace('"', '') and 'amarchy-theme = 2' in text,
        '1: %s holds background = #ff8800 and amarchy-theme = 2 (file %s, %d bytes)' % (FILE, 'present' if data else 'MISSING', len(text)))
    for p in PREFS:
        d = ami.read_file(p)
        t = d.decode('latin-1') if d else ''
        say('te-test' in t, '2: %s names te-test (%s)' % (p, t.strip().replace('\n', ' ; ')[:120] if d else 'MISSING'))
    after = getenv(SENTINEL)
    try:
        up = int(after) - int(args.before)
    except (TypeError, ValueError):
        up = None
    say(up == 1, '3: sentinel %s went up by exactly 1 across Save (before=%s after=%s)' % (SENTINEL, args.before, after))
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
    args.before = None
    CASES[args.case](args)
    finish(args)
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
