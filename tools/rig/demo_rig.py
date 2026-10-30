#!/usr/bin/env python3
"""demo_rig.py -- D1 on the rig: Help > Demo tour opens a tab, the tour plays, the tab closes (self-checking).

  1. A titled XCON: window; `Echo marker` typed in its shell.
  2. Help > Demo tour picked from the menu strip (wasabi's menu pick, not a typed /demo).
  3. C:UPDemo is running (Status), the tab's window opens, two frames a few seconds apart differ and the pixels
     carry 4 or more colours (the tab's text cannot be read: a Copy there returns the old clipboard).
  4. Esc ends the tour: C:UPDemo is gone and the shell tab is back with its marker. Then a second tour left to
     play to its own end (TOUR_SECS, default 300): the same result with no key.

Usage: UPTERM_RIG=<n> python3 -I tools/rig/demo_rig.py [key|end|resize]
Rig up (RTG screen), `rig.py install` done, `make amiga build/amiga/snipprobe`."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import re
import ami, typewin, winch_rig
import condev_rig as c
from hlmd_rig import colours

TITLE = 'demotab'
DEMO_TITLE = 'UP-Term demo'
failed = 0
TOUR_SECS = int(os.environ.get('TOUR_SECS', '300'))


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok  ' if ok else 'FAIL', what, '' if ok else ': ' + str(seen).strip()[-300:]), flush=True)
    return ok


def running():
    return 'UPDemo' in c.run('Status FULL')[1]


def until(cond, secs):
    end = time.time() + secs
    while time.time() < end:
        if cond():
            return True
        time.sleep(2)
    return cond()


def frame(title):
    x, y, ww, hh = ami.window(title)['box']
    return b''.join(ami.grab((x + 4, y + 14, ww - 8, hh - 18))[2])


def pick_tour():
    with ami._conn(60) as conn:
        ami._m().ui_menu(conn, 'Help/Demo tour', TITLE)


def tour(how):
    """Start the tour from the menu and end it by a key or by itself. The tab is a window of its own
    (TAB_DEMO), titled DEMO_TITLE, in front of the host window."""
    before = typewin.screen_text()
    pick_tour()
    check(until(running, 40), '%s: Help > Demo tour starts C:UPDemo' % how)
    check(until(lambda: ami.window(DEMO_TITLE) is not None, 20), '%s: the demo tab window %r opens' % (how, DEMO_TITLE))
    typewin._last_title = DEMO_TITLE
    typewin.click_in(DEMO_TITLE)
    time.sleep(8)
    col = colours(DEMO_TITLE)
    f1 = frame(DEMO_TITLE)
    time.sleep(6)
    f2 = frame(DEMO_TITLE)
    check(f1 != f2, '%s: the tab animates (two frames 6 s apart differ)' % how)
    check(len(col) >= 4, '%s: 4 or more colours in the tour\'s pixels' % how, ' '.join(col))
    if how == 'key':
        typewin.key(0x45, wait=3)      # Esc ends the tour (so do Q and Ctrl-C; the space bar and Return go to the next scene)
    gone = until(lambda: not running(), 30 if how == 'key' else TOUR_SECS)
    check(gone, '%s: C:UPDemo is gone (%s)' % (how, 'after a key' if how == 'key' else 'at its own end'))
    check(until(lambda: ami.window(DEMO_TITLE) is None, 20), '%s: the demo tab window closed' % how)
    typewin._last_title = TITLE
    typewin.click_in(TITLE)
    time.sleep(2)
    after = typewin.screen_text()
    check('marker-' + TITLE in after and after.strip() == before.strip() and ami.window(TITLE) is not None,
          '%s: the shell tab is back as it was (same text, marker in it), the window is still open' % how, after[-200:])


def reflow_number():
    """The reflow scene's number for `UPDemo <n>`: its place in demo/updemo.c's scene table, from 1."""
    src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'demo', 'updemo.c')).read()
    table = src[src.index('static const scene_def scenes[]'):]
    names = re.findall(r'^\s*\{ "([^"]+)", sc_', table, re.M)
    return names.index('reflow') + 1


def resize_then_continue():
    """UPDEMO-REFLOW-RESIZE: the reflow scene asks for a resize. The window is made smaller, then bigger
    (winch_rig.drag on the size gadget, as resize_rig does); the demo must still run and the scene after
    the reflow one must draw (two frames of the window differ), not end the demo."""
    title = 'rzdemo'
    c.run('Run >NIL: NewShell "XCON:0/12/760/440/%s/CLOSE"' % title)
    time.sleep(5)
    typewin._last_title = title
    typewin.click_in(title)
    typewin.typ('VTC:UPDemo %d' % reflow_number())
    typewin.key(typewin.RET, wait=2)
    check(until(running, 30), 'resize: C:UPDemo starts at the reflow scene')
    time.sleep(6)
    gx, gy, gw, gh = ami.window(title)['sys:size']
    pos = (gx + gw // 2, gy + gh // 2)
    winch_rig.drag(pos, -200, -120)
    time.sleep(4)
    check(running(), 'resize: the demo still runs after the window was made smaller')
    w = ami.window(title)['box']
    gx, gy, gw, gh = ami.window(title)['sys:size']
    winch_rig.drag((gx + gw // 2, gy + gh // 2), 200, 120)
    time.sleep(4)
    check(ami.window(title)['box'][2] > w[2], 'resize: the window could be made bigger again', w)
    check(running(), 'resize: the demo still runs after the window was made bigger')
    time.sleep(20)  # the reflow scene is 15 s: the next scene is on screen now
    f1 = frame(title)
    time.sleep(3)
    f2 = frame(title)
    check(running() and f1 != f2, 'resize: the scene after the reflow one animates (two frames differ)')
    typewin.key(0x45, wait=3)
    check(until(lambda: not running(), 30), 'resize: Esc ends the demo')
    typewin.typ('EndCLI')
    typewin.key(typewin.RET, wait=2)


def main(which):
    if which == 'resize':
        resize_then_continue()
        print('demo_rig: %s' % ('FAIL' if failed else 'PASS'))
        sys.exit(1 if failed else 0)
    c.run('Run >NIL: NewShell "XCON:0/12/760/440/%s/CLOSE"' % TITLE)
    time.sleep(5)
    typewin._last_title = TITLE
    typewin.click_in(TITLE)
    time.sleep(1)
    typewin.typ('Echo marker-' + TITLE)
    typewin.key(typewin.RET, wait=2)
    for how in ('key', 'end'):
        if which in (None, how):
            tour(how)
    typewin.typ('EndCLI')
    typewin.key(typewin.RET, wait=2)
    print('demo_rig: %s' % ('FAIL' if failed else 'PASS'))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
