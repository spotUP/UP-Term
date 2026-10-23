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
import stage_rig, tabstate

OUT = pathlib.Path(paths.RIG) / 'amarchy'
LAMIGA = 0x40                       # IEQUALIFIER_LCOMMAND
RAW = {'down': 0x4D, 't': 0x14, 'h': 0x25, 'k': 0x27, 'space': 0x40, 'esc': 0x45, 'return': 0x44}
KEYMAP = pathlib.Path(__file__).resolve().parents[2] / 'dist' / 'keymap'
TIMING = pathlib.Path(paths.RIG) / 'amarchy' / 'menu-timing.txt'


def km_hits():
    return tabstate.counters().get('km-hits', 0)


def screen_text():
    return ami.screen_text()


def bind_rows():
    n, sect = 0, ''
    for l in KEYMAP.read_text().splitlines():
        l = l.strip()
        if l.startswith('['):
            sect = l
        elif sect == '[bind]' and l and not l.startswith(';') and '=' in l:
            # "super+1..9" is one line of the file and nine rows (dist/keymap header, handler keymap parser)
            m = re.match(r'\S*?(\d)\.\.(\d)\s*=', l)
            n += int(m.group(2)) - int(m.group(1)) + 1 if m else 1
    return n


def open_terminal():
    # a titled UP-Term window the handler owns: `UPTerm state` and the request port need one
    tabstate.open_host('menuhost')


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


def into_style():
    # The root lists Apps, Style, Window (submenus); Theme is the first leaf of Style: Down, Enter descends.
    # (The old stale-hotkey ordering rule is gone: upterm-ports ebf5d1c makes ui_hotkey() describe the last run only.)
    for k in 'down', 'return':
        ami.key(RAW[k])
        time.sleep(0.6)


def r_type_th():
    into_style()
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.4)
    s = screen_text()
    return 'Filter: th' in s and 'Theme' in s, 'filter "th" leaves Theme in the list'


def r_esc_up():
    ami.key(RAW['esc'])           # clears the filter
    time.sleep(0.5)
    ami.key(RAW['esc'])           # in Style: back up to the root
    time.sleep(0.5)
    up = 'Style' in screen_text()
    ami.key(RAW['esc'])           # at the root: leaves the menu
    time.sleep(0.8)
    return up and 'Filter' not in screen_text(), 'Esc clears the filter, Esc goes up to the root (Style listed: %s), Esc at the root leaves' % up


def r_enter_slash():
    # reopen, Style, Enter on Theme: the request reaches the handler, which runs /theme through the keymap runner.
    # /theme opens a file requester ("UP-Term theme") and the handler sits in it until it closes, so the
    # effect is the requester on screen; the counter is read after its Cancel gadget (id 2) closed it, as keymap_rig.py does.
    before = km_hits()
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    into_style()
    ami.key(RAW['return'])             # Theme is the first row
    time.sleep(2)
    with ami._conn() as conn:
        shown = '"UP-Term theme"' in ami._uitree(conn).decode('latin-1')
        if shown:
            ami._click_gadget(conn, 'UP-Term theme', '2')
    time.sleep(1)
    ami.click_px(100, 100)             # the requester had the focus; the vsh host takes it back
    time.sleep(1)
    after = km_hits()
    return shown and after == before + 1, 'theme requester shown: %s; handler km-hits %+d (exactly 1 expected)' % (shown, after - before)


def viewer_lines(s):
    return set(l.strip(' |') for l in s.splitlines() if re.match(r'\|\s*(Super|Alt)\+', l))


def r_viewer_rows():
    # the viewer is a text box (amarchy/menu.c viewer, K_TEXTBOX in a 640x300 window): one page of rows at a time.
    # Scroll with Down, a few lines per step, and collect every distinct row line seen until a step adds none.
    ami.key(RAW['k'], LAMIGA)
    time.sleep(1.5)
    rows = viewer_lines(screen_text())
    for _ in range(40):
        for _ in range(6):
            ami.key(RAW['down'])
            time.sleep(0.15)
        time.sleep(0.4)
        new = viewer_lines(screen_text()) - rows
        if not new:
            break
        rows |= new
    ami.key(RAW['esc'])
    time.sleep(0.8)
    n = bind_rows()
    return n > 0 and len(rows) == n, 'dist/keymap has %d [bind] rows; viewer printed %d distinct rows while scrolling' % (n, len(rows))


def menu_rows(s):
    # the list rows of the menu box, labels only, in screen order (the frame and Filter line left out)
    return [l.strip(' |') for l in s.splitlines() if re.match(r'\|  \S', l) and 'Filter' not in l]


def theme_requester(before):
    # the /theme requester is the proof the leaf ran; Cancel it (gadget 2) and give the host window the focus back
    time.sleep(2)
    with ami._conn() as conn:
        shown = '"UP-Term theme"' in ami._uitree(conn).decode('latin-1')
        if shown:
            ami._click_gadget(conn, 'UP-Term theme', '2')
    time.sleep(1)
    ami.click_px(100, 100)
    time.sleep(1)
    return shown, km_hits() - before


def r_drill_redraw():
    # Owner bug AM-DRILL-REDRAW (A1200): Style drawn over the root, then "th" left "   me" and a stale Font row.
    # Cause: ncurses tparm memo keyed by the %i-bumped cup values (upterm-ports 7600cda), so rows landed one
    # line off. The cells must hold exactly the submenu's rows after the drill and after the filter.
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    into_style()
    style = menu_rows(screen_text())
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.5)
    th = menu_rows(screen_text())
    for _ in range(3):
        ami.key(RAW['esc'])
        time.sleep(0.5)
    ok = style == ['Theme', 'Font', 'Background'] and th == ['Theme']
    return ok, 'Style rows %s; after "th" %s' % (style, th)


def r_enter_th():
    # Owner bug AM-ENTER-H: Enter after the filter "th" added another "h" (stale ui_hotkey, upterm-ports ebf5d1c).
    # Enter must run Theme: the requester opens and km-hits moves by 1.
    before = km_hits()
    ami.key(RAW['space'], LAMIGA)
    time.sleep(1.5)
    into_style()
    for k in 't', 'h':
        ami.key(RAW[k])
        time.sleep(0.5)
    ami.key(RAW['return'])
    shown, d = theme_requester(before)
    return shown and d == 1, 'filter "th", Enter: theme requester shown: %s; km-hits %+d (1 expected)' % (shown, d)


def r_lamiga_minus_sv():
    # Owner bug AM-MINUS-EXIT: Swedish keymap, Left Amiga + "-" (raw 0x3A there) made Amarchy exit on the A1200.
    # With the keymap set to "s", the chord must reach the handler (km-hits +1) and the commodity must stay up.
    import amarchy_rig
    c.run('SetKeyboard s')
    try:
        before = km_hits()
        ami.key(0x3A, LAMIGA)
        time.sleep(2)
        d, up = km_hits() - before, amarchy_rig.port_up()
    finally:
        c.run('SetKeyboard usa')
    return d == 1 and up, 'Swedish Left Amiga+minus: km-hits %+d (1 expected), Amarchy port up: %s' % (d, up)


ROWS = [
    ('start',        r_start),
    ('open_timing',  r_open_timing),
    ('type_th',      r_type_th),
    ('esc_up',       r_esc_up),
    ('enter_slash',  r_enter_slash),
    ('viewer_rows',  r_viewer_rows),
    ('drill_redraw', r_drill_redraw),
    ('enter_th',     r_enter_th),
    ('lamiga_minus_sv', r_lamiga_minus_sv),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only')
    ap.add_argument('--failed', action='store_true')
    ap.add_argument('--fresh', action='store_true')
    ap.add_argument('--no-build', action='store_true', help='stage what build/amiga holds, do not run make')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    # the rows run VTC:Apps/bin/Amarchy and amarchy-menu: those files must be this build
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
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
