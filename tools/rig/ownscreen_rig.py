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
  7. /screen own, fullscreen, workbench (Settings > Screen): the window
     moves live, the text before the move still in it each time.
  6. SCREENMODE 0x29000 (PAL hires, AGA): 5 planes, and after colour
     output the palette's first 16 still hold the ANSI colours (ObtainBestPen
     took free pens and overwrote them until the 16 were allocated shared at
     open); the frames are in the Workbench's colours (its DrawInfo pens'
     RGB on the screen's DrawInfo pens, VTC:dripens).

UITREE lists the front screen only, so each check reads the front screen's
name and its windows. Run with the rig up and the handler installed (rig.py
install + a reboot): python3 tools/rig/ownscreen_rig.py
"""
import pathlib, shutil, struct, sys, time
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


def dri_rgb(screen):
    """the colours of a public screen's DrawInfo pens (VTC:dripens)"""
    for l in c.run('VTC:dripens "%s"' % screen)[1].splitlines():
        if l.startswith('rgb:'):
            return l.split()[1:]
    return None


def cursor_colour(title):
    """the colour of the solid run of 8 pixels (the block cursor) in the
    window, or the colours seen in its text area when there is none"""
    b = ami.req(0x07)
    w = struct.unpack('>H', b[2:4])[0]
    if b[0] == 1:
        nc = struct.unpack('>H', b[6:8])[0]
        pal, px = b[8:8 + nc * 3], b[8 + nc * 3:]
        rgb = lambda o: pal[px[o] * 3:px[o] * 3 + 3].hex()
    else:
        rgb = lambda o: b[8 + o * 3:11 + o * 3].hex()
    x0, y0, ww, hh = ami.window(title)['box']
    seen = set()
    for y in range(y0 + 12, y0 + hh - 4):
        run, last = 0, None
        for x in range(x0 + 4, x0 + ww - 20):
            v = rgb(y * w + x)
            seen.add(v)
            run = run + 1 if v == last and v != '000000' else 1
            last = v
            if run == 8:
                return v
    return sorted(seen)


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
        c.run('Run >NIL: NewShell "XCON:300/340/300/100/xvis/SCREEN UP-Term"')
        time.sleep(4)
        seen = cursor_colour('xvis')
        check(seen == 'c0c0c0', "a visitor UP-Term window's cursor is the default foreground (the pens past "
              "the 16 made COMPLEMENT draw it cyan)", seen)
        click_in('xvis')
        t('EndCLI', 3)
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
        # 8: a native AGA screen, 5 planes, the ANSI colours exact after colour
        # output, Intuition's pens in the Workbench's colours
        ansi = ['000000', 'cd0000', '00cd00', 'cdcd00', '0000ee', 'cd00cd', '00cdcd', 'e5e5e5',
                '7f7f7f', 'ff0000', '00ff00', 'ffff00', '5c5cff', 'ff00ff', '00ffff', 'ffffff']
        c.run('Delete ENV:up-term/up-term QUIET')
        (VTC / 'cols.txt').write_bytes(b''.join(b'\x1b[%dm %02d \x1b[0m' % (40 + i if i < 8 else 92 + i, i)
                                                for i in range(16)) + b'\n')
        c.run('Run >NIL: NewShell "XCON:0/20/640/200/nat/FULLSCREEN/SCREENMODE 0x29000/XTERM"')
        time.sleep(6)
        t('Type VTC:cols.txt', 3)
        b = ami.req(0x07)
        nc = struct.unpack('>H', b[6:8])[0]
        allpal = [b[8 + i * 3:11 + i * 3].hex() for i in range(nc)]
        pal = allpal[:16]
        check(b[0] == 1 and nc == 32 and pal == ansi,
              'a native AGA PAL screen: 5 planes, the 16 ANSI colours exact after colour output', (nc, pal))
        shutil.copyfile(ROOT / 'build/amiga/dripens', VTC / 'dripens')
        ours, wb = dri_rgb('UP-Term'), dri_rgb('Workbench')
        check(ours and ours == wb, "the frames, bars and menus in the Workbench's colours", (ours, wb))
        t('EndCLI', 4)
        # 9-11: the window moves live (Settings > Screen = /screen), its text with it
        shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
        c.run('Run >NIL: NewShell "XCON:0/20/640/300/sw/CLOSE"')
        time.sleep(5)
        click_in('sw')
        t('Set w marker')
        t('Echo before-$w')     # the word comes through a variable: the typed line is not it

        def kept():
            c.run('Delete RAM:sw.rc QUIET')
            t('VTC:UPTerm find before-$w >NIL:')
            t('Echo >RAM:sw.rc $RC')
            return c.run('Type RAM:sw.rc')[1].strip() == '0'
        t('/screen own', 5)
        name, wins = front()
        check(name == 'UP-Term' and ('sw', 640, 300) in wins and kept(),
              '/screen own: the window on a screen of its own, its text with it', (name, wins))
        t('/screen fullscreen', 5)
        name, wins = front()
        check(name == 'sw' and len(wins) == 1 and wins[0][1] >= 640 and kept(),
              '/screen fullscreen: the whole screen, the text still there', (name, wins))
        t('/screen workbench', 5)
        name, wins = front()
        check(name != 'UP-Term' and ('sw', 640, 300) in wins and kept(),
              '/screen workbench: back at its size, the text still there', (name, wins))
        t('EndCLI', 3)
    finally:
        if had:
            c.run('Copy RAM:ownscreen.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:ownscreen.bak QUIET')
        (VTC / 'ownscreen.conf').unlink(missing_ok=True)
        c.run('Delete RAM:sw.rc QUIET')
    print('ownscreen_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
