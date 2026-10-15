#!/usr/bin/env python3
"""demo_rig.py -- D1 on the rig: Help > Demo tour opens a tab, the tour plays, the tab closes (self-checking).

  1. A titled XCON: window; `Echo marker` typed in its shell.
  2. Help > Demo tour picked from the menu strip (wasabi's menu pick, not a typed /demo).
  3. C:UPDemo is running (Status), the tab's window opens, two frames a few seconds apart differ and the pixels
     carry 4 or more colours (the tab's text cannot be read: a Copy there returns the old clipboard).
  4. Esc ends the tour: C:UPDemo is gone and the shell tab is back with its marker. Then a second tour left to
     play to its own end (TOUR_SECS, default 300): the same result with no key.

Usage: UPTERM_RIG=<n> python3 -I tools/rig/demo_rig.py [key|end]
Rig up (RTG screen), `rig.py install` done, `make amiga build/amiga/snipprobe`."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, typewin
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


def main(which):
    c.run('Run >NIL: NewShell "XCON:0/12/760/440/%s/CLOSE"' % TITLE)
    time.sleep(5)
    typewin._last_title = TITLE
    typewin.click_in(TITLE)
    time.sleep(1)
    typewin._last_clip = typewin._clip('RAM:clip-pre.txt')
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
