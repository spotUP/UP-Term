#!/usr/bin/env python3
"""amarchy_window_rig.py -- the win: verbs of the Amarchy commodity (plan 2026-10-25-amarchy-window-groups.md,
Phase R, rows K5-K12; reachability section 7). Resumable like amarchy_rig.py: a verdict file per step in
<RIG>/amarchy-window/<step>.verdict; --only <step>, --failed, --fresh.

Entry point and sentinel: the commodity's own dispatch. A known window (NewShell CON:40/40/320/120/amarchy-target)
is moved with the win: verb; STATUS `wins` (K9) proves the new code ran and the window's LeftEdge read back through
the same window list the wasabi MCP amiga_windows reports proves the effect (32 pixels, other box fields kept).
The move-right step presses the real hotkey (Left Amiga + Shift + Right Arrow, the shipped win:move right row).
The menu_move_left step opens the drawn menu (Super+Space) with the target active, picks Window then Move left, and
so drives the ticket (K5a) and the LF_WIN leaf (K8, WIN verb over the port). The rc 5 ticket-box refusal (K5a step c)
has no step: the menu window is active while it is open, so no input changes the target box without ending it.
The K6 probe sends each held-lock verb (front, back, zip, move, size, fullwidth) and then a STATUS that must answer
within 5 s: a deadlock of LockIBase with the queued Intuition call would leave the commodity silent.
Needs the rig up (UPTERM_RIG=<n> rig.py start --max --purpose ...). Run from the checkout whose build is under
test: python3 tools/rig/amarchy_window_rig.py [--no-build]"""
import argparse, pathlib, re, sys, threading, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import stage_rig

OUT = pathlib.Path(paths.RIG) / 'amarchy-window'
TARGET = 'amarchy-target'
SPEC = 'CON:40/40/320/120/' + TARGET
STEP = 32                      # win_core.h WIN_STEP
RAW_RIGHT, LAMIGA, LSHIFT = 0x4E, 0x40, 0x01
SPACE, MENU_TITLE = 0x40, 'Amarchy'
ENTER = b'\r'


def port(cmd, timeout=30):
    """(rc, result) of one command to port AMARCHY; (None, '') when there is no port"""
    m = ami._m()
    try:
        with ami._conn() as conn:
            rc, _, res = m.arexx(conn, 'AMARCHY', cmd, timeout=timeout)
            return rc, res.strip()
    except m.WasabiError:
        return None, ''


def status():
    d = {}
    for w in port('STATUS')[1].replace('\n', ' ').split():
        if '=' in w:
            k, v = w.split('=', 1)
            if v.lstrip('-').isdigit():
                d[k] = int(v)
    return d


def box():
    w = ami.window(TARGET)
    return w['box'] if w else None


def answers_within(seconds):
    """STATUS answers within `seconds` (a deadlocked commodity does not)"""
    got = []
    t = threading.Thread(target=lambda: got.append(port('STATUS', timeout=seconds)), daemon=True)
    t.start()
    t.join(seconds)
    return bool(got) and got[0][0] == 0 and 'wins=' in got[0][1]


def activate_target():
    """the target is the active window: a click on its title bar"""
    b = box()
    if b:
        ami.click_px(b[0] + 60, b[1] + 5)
        time.sleep(0.5)


STATE = {}


def s_start():
    port('QUIT')
    time.sleep(1.5)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    time.sleep(2.5)
    s = status()
    return 'wins' in s and s['wins'] == 0, 'STATUS %r' % s


def s_open_target():
    c.run('Run >NIL: NewShell "%s"' % SPEC)
    time.sleep(2)
    activate_target()
    w = ami.window(TARGET)
    STATE['box0'] = w['box'] if w else None
    return bool(w), 'window %r' % (w,)


def s_hotkey_move_right():
    b0 = STATE.get('box0')
    if not b0:
        return False, 'no target window'
    n0 = status().get('wins', -1)
    ami.key(RAW_RIGHT, LAMIGA | LSHIFT)
    how = 'real hotkey Super+Shift+Right'
    time.sleep(1)
    b1, n1 = box(), status().get('wins', -1)
    ok = b1 is not None and b1[0] == b0[0] + STEP and b1[1:] == b0[1:] and n1 == n0 + 1
    return ok, '%s: box %r -> %r, wins %d -> %d' % (how, b0, b1, n0, n1)


def s_menu_move_left():
    """drawn menu: Super+Space, Window, Move left; the ticket names the target although the menu is active"""
    n0, b0 = status().get('wins', -1), box()
    activate_target()
    ami.key(SPACE, LAMIGA)
    end = time.time() + 15
    while time.time() < end and not ami.window(MENU_TITLE):
        time.sleep(0.5)
    if not ami.window(MENU_TITLE):
        return False, 'no menu window titled %r' % MENU_TITLE
    time.sleep(1)
    with ami._conn() as conn:
        for text in (b'Window', b'Move left'):
            ami._type(conn, text)
            time.sleep(0.7)
            ami._type(conn, ENTER)
            time.sleep(1)
    time.sleep(2)
    b1, n1 = box(), status().get('wins', -1)
    still = bool(ami.window(MENU_TITLE))
    for _ in range(3 if still else 0):        # never leave the menu open for the next step
        ami.key(0x45, 0)
        time.sleep(0.7)
    ok = b0 is not None and b1 == STATE['box0'] and b1 != b0 and n1 == n0 + 1
    return ok, 'menu Window > Move left: box %r -> %r (first %r), wins %d -> %d, menu open: %s' % (
        b0, b1, STATE['box0'], n0, n1, still)


def s_move_left():
    port('WIN move right')                    # the menu step already moved the window back
    time.sleep(1)
    b0, n0 = box(), status().get('wins', -1)
    rc, _ = port('WIN move left')
    time.sleep(1)
    b1, n1 = box(), status().get('wins', -1)
    ok = rc == 0 and b1 == STATE['box0'] and n1 == n0 + 1
    return ok, 'rc %r box %r -> %r (first %r), wins %d -> %d' % (rc, b0, b1, STATE['box0'], n0, n1)


def s_refuse_and_unknown():
    n0 = status().get('wins', -1)
    rc_bad, _ = port('WIN teleport')
    n1 = status().get('wins', -1)
    return rc_bad == 10 and n1 == n0, 'unknown verb rc %r, wins %d -> %d (not counted)' % (rc_bad, n0, n1)


def probe(verb, undo=None):
    def step():
        activate_target()
        rc, _ = port('WIN ' + verb, timeout=5)
        ok = answers_within(5)
        if undo:
            port('WIN ' + undo, timeout=5)
        return rc == 0 and ok, 'WIN %s rc %r, STATUS answered within 5 s: %s' % (verb, rc, ok)
    return step


ROWS = [
    ('start',          s_start),
    ('open_target',    s_open_target),
    ('move_right',     s_hotkey_move_right),
    ('menu_move_left', s_menu_move_left),
    ('move_left',      s_move_left),
    ('unknown_verb',   s_refuse_and_unknown),
    ('k6_front',       probe('front')),
    ('k6_back',        probe('back')),
    ('k6_zip',         probe('zip', 'zip')),
    ('k6_move',        probe('move down', 'move up')),
    ('k6_size',        probe('size right', 'size left')),
    ('k6_fullwidth',   probe('fullwidth', 'restore')),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(names=('Amarchy', 'amarchy-gui'), build=not a.no_build))
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
        try:
            ok, detail = fn()
        except Exception as e:                      # a step that cannot run is a failure, recorded
            ok, detail = False, 'exception %r' % (e,)
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    print('amarchy_window_rig: passed %d of %d (verdicts in %s)' % (passed, total, OUT))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
