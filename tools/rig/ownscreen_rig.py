#!/usr/bin/env python3
"""ownscreen_rig.py -- a screen of UP-Term's own (plan
thoughts/shared/plans/2026-10-03-screens-and-dctelnet.md, P1).

  1. XCON:.../OWNSCREEN opens a screen "UP-Term" with the window on it.
  2. Another program's window (CON:.../SCREEN UP-Term) opens on it: public.
  3. The UP-Term window closes while the visitor is open: the screen stays;
     the visitor closes: the screen goes.
  4. XCON:.../FULLSCREEN: one window, borderless, the whole screen, its
     title in the screen's title bar; it closes with the window.
  5. The profile key screen = fullscreen does the same for a plain window.

UITREE lists the front screen only, so each check reads the front screen's
name and its windows. Run with the rig up and the handler installed (rig.py
install + a reboot): python3 tools/rig/ownscreen_rig.py
"""
import pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / 'build/rig/vtc'
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def front():
    """(front screen's name, [(window title, w, h)])"""
    name, wins = None, []
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        if l.startswith('S ') and name is None:
            name = l.split('"')[-2]
        elif l.startswith('W '):
            f = l.split()
            w, h = map(int, f[4].split('x'))
            wins.append((l.split('"')[-2], w, h))
    return name, wins


def t(s, wt=2):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(wt)


def click_in(title):
    x, y, w, h = ami.window(title)['box']
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)


def main():
    had = c.run('Copy ENV:up-term/up-term RAM:ownscreen.bak QUIET')[0] == 0
    try:
        c.run('Run >NIL: NewShell "XCON:0/20/640/300/own/OWNSCREEN"')
        time.sleep(5)
        name, wins = front()
        check(name == 'UP-Term' and any(w[0] == 'own' for w in wins), 'OWNSCREEN: a screen "UP-Term" with the window',
              (name, wins))
        c.run('Run >NIL: NewShell "CON:0/340/300/100/visitor/SCREEN UP-Term"')
        time.sleep(4)
        name, wins = front()
        check(any(w[0] == 'visitor' for w in wins), 'it is public: another window opens on it', (name, wins))
        click_in('own')
        t('EndCLI', 3)
        name, wins = front()
        check(name == 'UP-Term' and [w[0] for w in wins] == ['visitor'],
              'UP-Term closed, the visitor open: the screen stays', (name, wins))
        click_in('visitor')
        t('EndCLI', 5)
        name, wins = front()
        check(name != 'UP-Term', 'the visitor closed: the screen goes', name)

        c.run('Run >NIL: NewShell "XCON:0/20/640/300/full/FULLSCREEN"')
        time.sleep(6)
        name, wins = front()
        check(name == 'full' and len(wins) == 1 and wins[0][0] == '' and wins[0][1] >= 640,
              'FULLSCREEN: one borderless window over the screen, the title on the screen', (name, wins))
        t('EndCLI', 4)
        check(front()[0] != 'full', 'it closes with the window', front()[0])

        c.run('MakeDir ENV:up-term QUIET')
        (VTC / 'ownscreen.conf').write_text('[profile default]\nscreen = fullscreen\n')
        c.run('Copy VTC:ownscreen.conf ENV:up-term/up-term QUIET')
        c.run('Run >NIL: NewShell "XCON:0/20/640/300/byprofile/CLOSE"')
        time.sleep(6)
        name, wins = front()
        check(name == 'byprofile' and len(wins) == 1 and wins[0][1] >= 640,
              'the profile key screen = fullscreen does it for a plain XCON: window', (name, wins))
        t('EndCLI', 4)
    finally:
        if had:
            c.run('Copy RAM:ownscreen.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:ownscreen.bak QUIET')
        (VTC / 'ownscreen.conf').unlink(missing_ok=True)
    print('ownscreen_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
